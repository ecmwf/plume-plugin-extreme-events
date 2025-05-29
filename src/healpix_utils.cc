/*
 * (C) Copyright 2025- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */
#include "atlas/functionspace.h"
#include "atlas/library.h"
#include "atlas/mesh.h"
#include "atlas/parallel/mpi/mpi.h"
#include "atlas/runtime/Log.h"
#include "atlas/util/KDTree.h"

#include "healpix_utils.h"

using namespace atlas::meshgenerator;
using namespace atlas::grid;

namespace HEALPixUtils {

void mapLonLatToHEALPixCell(int resolution, const atlas::FunctionSpace& modelFS, std::vector<int>& mappingVector,
                            std::vector<std::vector<atlas::PointLonLat>>& cellVertices) {
    /* Generate a HEALPix mesh
    The resolution is expected to be coarse enough to avoid the need for
    partitioning the mesh. Each partition has access to the global indices of the
    HEALPix mesh.
    /!\ If this were to change, the mesh needs to change to include a halo of 1,
    otherwise there might be an edge case where the search tree isn't populated
    with all the necessary cells to produce the mapping.
    */
    atlas::Grid grid("H" + std::to_string(resolution));
    atlas::util::Config healpix_config;
    healpix_config.set("pole_elements", "pentagons");
    healpix_config.set("mpi_comm", "self");
    atlas::Mesh HPmesh(grid, healpix_config);
    atlas::functionspace::CellColumns healpix_cell_fs(HPmesh);

    // Get the lonlat coordinates and global indices of the HP mesh cell centres
    auto healpix_lonlat = atlas::array::make_view<double, 2>(healpix_cell_fs.lonlat());
    auto healpix_gidx   = atlas::array::make_view<atlas::gidx_t, 1>(healpix_cell_fs.global_index());
    // Create and populate a KDTree search to find closest HP cell
    atlas::util::IndexKDTree search;
    search.reserve(healpix_lonlat.shape(0));
    for (atlas::idx_t jcell = 0; jcell < healpix_lonlat.shape(0); ++jcell) {
        atlas::PointLonLat p{healpix_lonlat(jcell, 0), healpix_lonlat(jcell, 1)};
        search.insert(p, jcell);
    }
    search.build();

    auto model_lonlat = atlas::array::make_view<double, 2>(modelFS.lonlat());
    // Halo: value 0 everywhere except in halo cells
    auto model_ghost = atlas::array::make_view<int, 1>(modelFS.ghost());
    // Resize the mapping vector to the size of model grid points
    mappingVector.resize(modelFS.size());
    // Search for the closest cell in the HEALPix mesh
    for (atlas::idx_t j = 0; j < model_lonlat.shape(0); ++j) {
        // Skip grid points in halo as their detection is run in another partition
        if (model_ghost[j]) {
            // Cell global indexing starts with 1 but we use 0 to map to the vector
            // containing the cell vertices, so using -1 as invalid value for halo
            // points
            mappingVector[j] = -1;
            continue;
        }
        atlas::PointLonLat p{model_lonlat(j, 0), model_lonlat(j, 1)};
        auto closest     = search.closestPoint(p);
        mappingVector[j] = closest.payload();
    }

    // Now map the vertices lonlat coordinates of each cell
    auto healpix_nodes_lonlat = atlas::array::make_view<double, 2>(HPmesh.nodes().lonlat());
    auto& cell2node           = HPmesh.cells().node_connectivity();
    std::vector<atlas::idx_t> nodes(cell2node.maxcols());
    cellVertices.resize(HPmesh.cells().size());
    for (atlas::idx_t jcell = 0; jcell < HPmesh.cells().size(); ++jcell) {
        atlas::idx_t nb_nodes_per_cell = cell2node.cols(jcell);
        for (atlas::idx_t jnode = 0; jnode < nb_nodes_per_cell; ++jnode) {
            nodes[jnode] = cell2node(jcell, jnode);
            // store the lonlat of the nodes in cellVertices
            cellVertices[jcell].push_back(
                atlas::PointLonLat{healpix_nodes_lonlat(nodes[jnode], 0), healpix_nodes_lonlat(nodes[jnode], 1)});
        }
    }
}

std::vector<std::vector<atlas::PointLonLat>> cellToPolygons(std::set<int>& eeCells,
                                                            std::vector<std::vector<atlas::PointLonLat>>& vertices) {
    std::vector<std::vector<atlas::PointLonLat>> ee_polygons;
    // Separate contiguous events and remove inner vertices
    // TODO: edge case: a region with holes has been detected
    // currently will end up with two separate events: hole and borders
    std::map<std::pair<atlas::PointLonLat, atlas::PointLonLat>, int> count_edges;
    for (const int& cell_idx : eeCells) {
        for (size_t vidx = 0; vidx < vertices[cell_idx].size(); ++vidx) {
            // Add all cell edges, handles quads and pents pole elements
            std::pair<atlas::PointLonLat, atlas::PointLonLat> ee_edge = {
                vertices[cell_idx][vidx], vertices[cell_idx][(vidx + 1) % vertices[cell_idx].size()]};
            if (count_edges.find(ee_edge) != count_edges.end()) {
                ++count_edges[ee_edge];
            }
            else if (count_edges.find({ee_edge.second, ee_edge.first}) != count_edges.end()) {
                ++count_edges[{ee_edge.second, ee_edge.first}];
            }
            else {
                count_edges[ee_edge] = 1;
            }
        }
    }
    for (auto it = count_edges.cbegin(); it != count_edges.cend();) {
        // Remove all inner edges
        if (it->second > 1) {
            it = count_edges.erase(it);
            continue;
        }
        ++it;
    }
    if (count_edges.empty()) {
        // TODO: /!\ case when event is global is not handled
        // (all edges belong to 2 firing cells so the counter is empty)
        std::cout << "All the globe has fired... case not handled" << std::endl;
        return {{atlas::PointLonLat(0.0, 0.0)}};
    }
    // Separate polygons
    std::map<atlas::PointLonLat, std::vector<atlas::PointLonLat>> vertex_walk;
    for (auto& ee_edge : count_edges) {
        // populate a hash map of the edges to preserve vertex order
        auto it = std::lower_bound(vertex_walk[ee_edge.first.first].begin(), vertex_walk[ee_edge.first.first].end(),
                                   ee_edge.first.second);
        vertex_walk[ee_edge.first.first].insert(it, ee_edge.first.second);
    }
    bool newPolygon = true;
    atlas::PointLonLat currentPoint;
    atlas::PointLonLat previousPoint;
    int previousPointIdx = 0;
    int nextPointIdx     = 0;
    int polyIdx          = -1;
    while (!vertex_walk.empty()) {
        if (newPolygon) {
            // Start a new polygon as no more edges belong to the previous one
            polyIdx++;
            previousPoint = vertex_walk.begin()->first;
            currentPoint  = vertex_walk.begin()->second[0];
            ee_polygons.push_back({previousPoint, currentPoint});
            newPolygon = false;
        }
        if (vertex_walk.find(currentPoint) != vertex_walk.end()) {
            nextPointIdx = 0;
            if (vertex_walk[currentPoint].size() > 1 && currentPoint[0] > vertex_walk[currentPoint][0][0]) {
                // TODO: may actually need to check by latitude if longitudes ~
                // Make sure we walk in the right direction if the vertex is the
                // starting point of two edges
                nextPointIdx = 1;
            }
            ee_polygons[polyIdx].push_back(vertex_walk[currentPoint][nextPointIdx]);
            if (vertex_walk[previousPoint].size() == 1) {
                vertex_walk.erase(previousPoint);
            }
            else {
                vertex_walk[previousPoint].erase(vertex_walk[previousPoint].begin() + previousPointIdx);
            }
            previousPoint    = currentPoint;
            currentPoint     = vertex_walk[previousPoint][nextPointIdx];
            previousPointIdx = nextPointIdx;
        }
        else {
            newPolygon = true;
            vertex_walk.erase(previousPoint);
        }
    }
    return ee_polygons;
}

}  // namespace HEALPixUtils