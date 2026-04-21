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
#include "atlas/mesh.h"
#include "atlas/util/KDTree.h"

#include "healpix_utils.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <tuple>
#include <cstdlib>
#include <atomic>
#include <limits>
#include <string>

using namespace atlas::meshgenerator;
using namespace atlas::grid;

namespace HEALPixUtils {

namespace {

constexpr double kLonPeriod  = 360.0;
constexpr double kCoordScale = 1e9;
constexpr double kPoleLatAbs = 90.0;
constexpr double kPoleTol    = 1e-8;
constexpr double kMinPolyArea = 1e-8;

double normalizeLon360(double lon) {
    double normalized = std::fmod(lon, kLonPeriod);
    if (normalized < 0.0) {
        normalized += kLonPeriod;
    }
    if (normalized >= kLonPeriod) {
        normalized -= kLonPeriod;
    }
    return normalized;
}

long long quantizeCoord(double value) {
    return static_cast<long long>(std::llround(value * kCoordScale));
}

atlas::PointLonLat canonicalPoint(const atlas::PointLonLat& point) {
    return atlas::PointLonLat{normalizeLon360(point.lon()), point.lat()};
}

atlas::PointLonLat canonicalPointForTopology(const atlas::PointLonLat& point) {
    const double lat = point.lat();
    // All longitudes collapse to the same physical point at the poles.
    if (std::fabs(std::fabs(lat) - kPoleLatAbs) <= kPoleTol) {
        return atlas::PointLonLat{0.0, lat};
    }
    return canonicalPoint(point);
}

using Edge = std::pair<atlas::PointLonLat, atlas::PointLonLat>;
using DirectedEdge = std::pair<atlas::PointLonLat, atlas::PointLonLat>;

struct WrappedPointComparator {
    bool operator()(const atlas::PointLonLat& lhs, const atlas::PointLonLat& rhs) const {
        const auto lhsKey = std::make_tuple(quantizeCoord(normalizeLon360(lhs.lon())), quantizeCoord(lhs.lat()));
        const auto rhsKey = std::make_tuple(quantizeCoord(normalizeLon360(rhs.lon())), quantizeCoord(rhs.lat()));
        return lhsKey < rhsKey;
    }
};

struct WrappedEdgeComparator {
    bool operator()(const Edge& lhs, const Edge& rhs) const {
        WrappedPointComparator pointLess;
        if (pointLess(lhs.first, rhs.first)) {
            return true;
        }
        if (pointLess(rhs.first, lhs.first)) {
            return false;
        }
        return pointLess(lhs.second, rhs.second);
    }
};

struct DirectedEdgeComparator {
    bool operator()(const DirectedEdge& lhs, const DirectedEdge& rhs) const {
        WrappedPointComparator pointLess;
        if (pointLess(lhs.first, rhs.first)) {
            return true;
        }
        if (pointLess(rhs.first, lhs.first)) {
            return false;
        }
        return pointLess(lhs.second, rhs.second);
    }
};

bool wrappedPointEqual(const atlas::PointLonLat& lhs, const atlas::PointLonLat& rhs) {
    WrappedPointComparator less;
    return !less(lhs, rhs) && !less(rhs, lhs);
}

bool topologyPointEqual(const atlas::PointLonLat& lhs, const atlas::PointLonLat& rhs) {
    return wrappedPointEqual(canonicalPointForTopology(lhs), canonicalPointForTopology(rhs));
}

Edge orderedEdge(const atlas::PointLonLat& a, const atlas::PointLonLat& b) {
    // Shared cell edges can arrive with opposite winding. Canonicalise them so
    // inner edges collapse to a single map entry regardless of source cell.
    WrappedPointComparator less;
    if (less(a, b)) {
        return {a, b};
    }
    return {b, a};
}

double shortestLonDelta(double fromLon, double toLon) {
    double delta = normalizeLon360(toLon) - normalizeLon360(fromLon);
    if (delta > 180.0) {
        delta -= kLonPeriod;
    }
    else if (delta < -180.0) {
        delta += kLonPeriod;
    }
    return delta;
}

std::vector<double> unwrapPolygonLongitudes(const std::vector<atlas::PointLonLat>& polygon) {
    std::vector<double> lons;
    if (polygon.empty()) {
        return lons;
    }

    lons.reserve(polygon.size());
    double previous = normalizeLon360(polygon.front().lon());
    lons.push_back(previous);

    for (size_t i = 1; i < polygon.size(); ++i) {
        // Keep consecutive vertices within +/-180 degrees of each other so
        // dateline-crossing polygons remain contiguous in the working frame.
        const double delta = shortestLonDelta(previous, polygon[i].lon());
        previous += delta;
        lons.push_back(previous);
    }

    return lons;
}

double lonNearReference(double lon, double refLon) {
    double shifted = normalizeLon360(lon);
    double delta   = shifted - refLon;
    if (delta > 180.0) {
        shifted -= kLonPeriod;
    }
    else if (delta < -180.0) {
        shifted += kLonPeriod;
    }
    return shifted;
}

// Arithmetic mean centroid in an unwrapped longitude frame.
atlas::PointLonLat polygonCentroid(const std::vector<atlas::PointLonLat>& polygon) {
    const auto unwrappedLons = unwrapPolygonLongitudes(polygon);
    double sumLon = 0.0, sumLat = 0.0;
    for (size_t i = 0; i < polygon.size(); ++i) {
        sumLon += unwrappedLons[i];
        sumLat += polygon[i].lat();
    }
    const double n = static_cast<double>(polygon.size());
    return atlas::PointLonLat{normalizeLon360(sumLon / n), sumLat / n};
}

// Unsigned planar area via the shoelace formula (lon/lat degrees, not spherical),
// computed in an unwrapped longitude frame to avoid seam artifacts.
double polygonArea(const std::vector<atlas::PointLonLat>& polygon) {
    const auto unwrappedLons = unwrapPolygonLongitudes(polygon);
    double area              = 0.0;
    const size_t n           = polygon.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        area += unwrappedLons[j] * polygon[i].lat();
        area -= unwrappedLons[i] * polygon[j].lat();
    }
    return std::fabs(area) / 2.0;
}

// Ray-casting point-in-polygon: casts a horizontal ray eastward from `point`
// and counts edge crossings. Polygon longitudes are shifted near the probe
// longitude to stay seam-consistent around 360/0.
bool pointInPolygon(const atlas::PointLonLat& point, const std::vector<atlas::PointLonLat>& polygon) {
    const double px = normalizeLon360(point.lon());
    const double py = point.lat();
    bool inside     = false;
    const size_t n  = polygon.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const double xi = lonNearReference(polygon[i].lon(), px);
        const double yi = polygon[i].lat();
        const double xj = lonNearReference(polygon[j].lon(), px);
        const double yj = polygon[j].lat();
        if (((yi > py) != (yj > py)) && (px < (xj - xi) * (py - yi) / (yj - yi) + xi)) {
            inside = !inside;
        }
    }
    return inside;
}

double containmentRatio(const std::vector<atlas::PointLonLat>& inner,
                        const std::vector<atlas::PointLonLat>& outer) {
    if (inner.empty() || outer.size() < 3) {
        return 0.0;
    }
    size_t insideCount = 0;
    for (const auto& p : inner) {
        if (pointInPolygon(p, outer)) {
            ++insideCount;
        }
    }
    return static_cast<double>(insideCount) / static_cast<double>(inner.size());
}

std::vector<atlas::PointLonLat> toOpenRing(const std::vector<atlas::PointLonLat>& polygon) {
    if (polygon.empty()) {
        return {};
    }
    if (polygon.size() >= 2 && wrappedPointEqual(polygon.front(), polygon.back())) {
        return std::vector<atlas::PointLonLat>(polygon.begin(), polygon.end() - 1);
    }
    return polygon;
}

std::vector<atlas::PointLonLat> rotateOpenRing(const std::vector<atlas::PointLonLat>& ring, size_t startIdx) {
    std::vector<atlas::PointLonLat> rotated;
    if (ring.empty()) {
        return rotated;
    }
    rotated.reserve(ring.size());
    for (size_t i = 0; i < ring.size(); ++i) {
        rotated.push_back(ring[(startIdx + i) % ring.size()]);
    }
    return rotated;
}

bool findSharedVertexIndex(const std::vector<atlas::PointLonLat>& polyA, const std::vector<atlas::PointLonLat>& polyB,
                           size_t& idxA, size_t& idxB) {
    const auto ringA = toOpenRing(polyA);
    const auto ringB = toOpenRing(polyB);
    for (size_t i = 0; i < ringA.size(); ++i) {
        for (size_t j = 0; j < ringB.size(); ++j) {
            if (topologyPointEqual(ringA[i], ringB[j])) {
                idxA = i;
                idxB = j;
                return true;
            }
        }
    }
    return false;
}

std::vector<atlas::PointLonLat> mergePolygonsAtSharedVertex(const std::vector<atlas::PointLonLat>& polyA,
                                                             const std::vector<atlas::PointLonLat>& polyB,
                                                             size_t sharedIdxA, size_t sharedIdxB) {
    const auto ringA = toOpenRing(polyA);
    const auto ringB = toOpenRing(polyB);
    if (ringA.empty() || ringB.empty()) {
        return polyA;
    }

    const auto rotA = rotateOpenRing(ringA, sharedIdxA);
    const auto rotB = rotateOpenRing(ringB, sharedIdxB);

    // Stitch both cycles through the common vertex so touching components
    // are exported as one connected polygon ring.
    std::vector<atlas::PointLonLat> merged;
    merged.reserve(rotA.size() + rotB.size() + 1);
    merged.push_back(rotA.front());
    for (size_t i = 1; i < rotA.size(); ++i) {
        merged.push_back(rotA[i]);
    }
    merged.push_back(rotA.front());
    for (size_t i = 1; i < rotB.size(); ++i) {
        merged.push_back(rotB[i]);
    }
    merged.push_back(rotB.front());
    return merged;
}

bool isDegeneratePolygon(const std::vector<atlas::PointLonLat>& polygon) {
    const auto ring = toOpenRing(polygon);
    if (ring.size() < 3) {
        return true;
    }

    std::set<atlas::PointLonLat, WrappedPointComparator> uniqueVertices;
    for (const auto& p : ring) {
        uniqueVertices.insert(canonicalPointForTopology(p));
    }
    if (uniqueVertices.size() < 3) {
        return true;
    }

    return polygonArea(ring) <= kMinPolyArea;
}

void dumpCellToPolygonsSnapshot(const char* path, int callId, const char* stage, const std::set<int>& eeCells,
                                const std::vector<std::vector<atlas::PointLonLat>>& polygons,
                                size_t boundaryEdgeCount = 0, size_t brokenWalkCount = 0,
                                size_t openWalkCount = 0) {
    if (path == nullptr || *path == '\0') {
        return;
    }
    std::ofstream out(path, std::ios::app);
    if (!out) {
        return;
    }

    double minArea = 0.0;
    double maxArea = 0.0;
    double sumArea = 0.0;
    size_t nDegenerateLike = 0;
    bool first = true;
    for (const auto& poly : polygons) {
        const auto ring = toOpenRing(poly);
        const double a = polygonArea(ring);
        if (first) {
            minArea = maxArea = a;
            first = false;
        }
        else {
            minArea = std::min(minArea, a);
            maxArea = std::max(maxArea, a);
        }
        sumArea += a;
        if (isDegeneratePolygon(poly)) {
            ++nDegenerateLike;
        }
    }

    out << "{"
        << "\"call_id\":" << callId
        << ",\"stage\":\"" << stage << "\""
        << ",\"ee_cells\":" << eeCells.size()
        << ",\"boundary_edges\":" << boundaryEdgeCount
        << ",\"broken_walks\":" << brokenWalkCount
        << ",\"open_walks\":" << openWalkCount
        << ",\"polygon_count\":" << polygons.size()
        << ",\"degenerate_like_count\":" << nDegenerateLike
        << ",\"min_area\":" << minArea
        << ",\"max_area\":" << maxArea
        << ",\"sum_area\":" << sumArea
        << "}" << '\n';
}

void dumpCellToPolygonsCoordinates(const char* path, int callId, const char* stage,
                                   const std::vector<std::vector<atlas::PointLonLat>>& polygons) {
    if (path == nullptr || *path == '\0') {
        return;
    }
    std::ofstream out(path, std::ios::app);
    if (!out) {
        return;
    }

    out << "{"
        << "\"call_id\":" << callId
        << ",\"stage\":\"" << stage << "\""
        << ",\"polygons\":[";

    for (size_t i = 0; i < polygons.size(); ++i) {
        if (i > 0) {
            out << ',';
        }
        out << '[';
        for (size_t j = 0; j < polygons[i].size(); ++j) {
            if (j > 0) {
                out << ',';
            }
            out << '[' << polygons[i][j].lon() << ',' << polygons[i][j].lat() << ']';
        }
        out << ']';
    }

    out << "]}" << '\n';
}

bool envEnabled(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr && *v != '\0' && std::string(v) != "0";
}

}  // namespace

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

    // Optional debug dump of model->HEALPix mapping.
    // Set EE_HEALPIX_MAPPING_CSV to a writable file path to enable.
    if (const char* csvPath = std::getenv("EE_HEALPIX_MAPPING_CSV"); csvPath && *csvPath) {
        std::ofstream csv(csvPath);
        if (csv) {
            csv << "model_idx,lon,lat,ghost,mapped_cell,mapped_gidx,cell_center_lon,cell_center_lat\n";
            for (atlas::idx_t j = 0; j < model_lonlat.shape(0); ++j) {
                const int mappedCell = mappingVector[j];
                const bool validCell = (mappedCell >= 0) && (mappedCell < static_cast<int>(healpix_lonlat.shape(0)));

                csv << j << ',' << model_lonlat(j, 0) << ',' << model_lonlat(j, 1) << ',' << model_ghost(j) << ','
                    << mappedCell << ',';

                if (validCell) {
                    csv << healpix_gidx(mappedCell) << ',' << healpix_lonlat(mappedCell, 0) << ','
                        << healpix_lonlat(mappedCell, 1);
                }
                else {
                    csv << "-1,,";
                }
                csv << '\n';
            }
        }
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
    static std::atomic<int> callCounter{0};
    const int callId = ++callCounter;
    const char* snapshotPath = std::getenv("EE_CELLTOPOLYGONS_SNAPSHOT_JSONL");
    const char* snapshotCoordsPath = std::getenv("EE_CELLTOPOLYGONS_COORDS_JSONL");
    // New directed-edge traversal is the default fix. Set
    // EE_CELLTOPOLYGONS_LEGACY_WALK=1 to temporarily fall back.
    const bool experimentalFix = !envEnabled("EE_CELLTOPOLYGONS_LEGACY_WALK");
    // Separate contiguous events and remove inner vertices
    // Edge case: a cell not in eeCells that is fully surrounded by fired cells
    // produces a separate inner polygon (its boundary has edge-count == 1 from
    // all adjacent fired cells). These hole polygons are removed below.
    std::map<Edge, int, WrappedEdgeComparator> count_edges;
    std::map<DirectedEdge, int, DirectedEdgeComparator> directed_edges;
    for (const int& cell_idx : eeCells) {
        for (size_t vidx = 0; vidx < vertices[cell_idx].size(); ++vidx) {
            const atlas::PointLonLat a = canonicalPoint(vertices[cell_idx][vidx]);
            const atlas::PointLonLat b = canonicalPoint(vertices[cell_idx][(vidx + 1) % vertices[cell_idx].size()]);

            // Keep undirected counts for legacy/non-experimental path and diagnostics.
            ++count_edges[orderedEdge(a, b)];

            // Directed cancellation: if reverse edge already exists, cancel it;
            // otherwise keep current direction as boundary candidate.
            const DirectedEdge e{a, b};
            const DirectedEdge rev{b, a};
            auto revIt = directed_edges.find(rev);
            if (revIt != directed_edges.end() && revIt->second > 0) {
                --revIt->second;
                if (revIt->second == 0) {
                    directed_edges.erase(revIt);
                }
            }
            else {
                ++directed_edges[e];
            }
        }
    }

    if (!experimentalFix) {
        for (auto it = count_edges.cbegin(); it != count_edges.cend();) {
            // Remove all inner edges
            if (it->second > 1) {
                it = count_edges.erase(it);
                continue;
            }
            ++it;
        }
    }
    dumpCellToPolygonsSnapshot(snapshotPath, callId, "after_edge_filter", eeCells, ee_polygons, count_edges.size());
    dumpCellToPolygonsCoordinates(snapshotCoordsPath, callId, "after_edge_filter", ee_polygons);
    if ((!experimentalFix && count_edges.empty()) || (experimentalFix && directed_edges.empty())) {
        // TODO: /!\ case when event is global is not handled
        // (all edges belong to 2 firing cells so the counter is empty)
        std::cout << "All the globe has fired... case not handled" << std::endl;
        return {{atlas::PointLonLat(0.0, 0.0)}};
    }
    // Separate polygons by consuming boundary edges.
    std::map<atlas::PointLonLat, std::vector<atlas::PointLonLat>, WrappedPointComparator> adjacency;
    std::set<Edge, WrappedEdgeComparator> remainingEdges;
    std::map<DirectedEdge, int, DirectedEdgeComparator> remainingDirected;
    if (experimentalFix) {
        remainingDirected = directed_edges;
        for (const auto& entry : remainingDirected) {
            for (int n = 0; n < entry.second; ++n) {
                adjacency[entry.first.first].push_back(entry.first.second);
            }
        }
    }
    else {
        for (const auto& entry : count_edges) {
            const auto& a = entry.first.first;
            const auto& b = entry.first.second;
            adjacency[a].push_back(b);
            adjacency[b].push_back(a);
            remainingEdges.insert(entry.first);
        }
    }

    auto hasRemainingEdge = [&](const atlas::PointLonLat& a, const atlas::PointLonLat& b) {
        if (experimentalFix) {
            const DirectedEdge e{a, b};
            auto it = remainingDirected.find(e);
            return it != remainingDirected.end() && it->second > 0;
        }
        return remainingEdges.find(orderedEdge(a, b)) != remainingEdges.end();
    };

    auto consumeEdge = [&](const atlas::PointLonLat& a, const atlas::PointLonLat& b) {
        if (experimentalFix) {
            const DirectedEdge e{a, b};
            auto it = remainingDirected.find(e);
            if (it != remainingDirected.end()) {
                --it->second;
                if (it->second == 0) {
                    remainingDirected.erase(it);
                }
            }
        }
        else {
            remainingEdges.erase(orderedEdge(a, b));
        }
    };

    size_t brokenWalkCount = 0;
    size_t openWalkCount = 0;

    while ((experimentalFix && !remainingDirected.empty()) || (!experimentalFix && !remainingEdges.empty())) {
        atlas::PointLonLat start;
        atlas::PointLonLat prev;
        atlas::PointLonLat cur;
        if (experimentalFix) {
            const auto& seed = *remainingDirected.begin();
            start = seed.first.first;
            prev = start;
            cur = seed.first.second;
        }
        else {
            const auto& seed = *remainingEdges.begin();
            start = seed.first;
            prev = seed.first;
            cur = seed.second;
        }

        std::vector<atlas::PointLonLat> polygon = {start, cur};
        consumeEdge(start, cur);

        while (cur != start) {
            const auto adjIt = adjacency.find(cur);
            if (adjIt == adjacency.end()) {
                ++brokenWalkCount;
                break;
            }

            const auto& neigh = adjIt->second;
            atlas::PointLonLat next;
            if (experimentalFix) {
                auto nextIt = std::find_if(neigh.begin(), neigh.end(), [&](const atlas::PointLonLat& candidate) {
                    return hasRemainingEdge(cur, candidate) && !topologyPointEqual(candidate, prev);
                });
                if (nextIt == neigh.end()) {
                    nextIt = std::find_if(neigh.begin(), neigh.end(), [&](const atlas::PointLonLat& candidate) {
                        return hasRemainingEdge(cur, candidate);
                    });
                }
                if (nextIt == neigh.end()) {
                    ++brokenWalkCount;
                    break;
                }
                next = *nextIt;
            }
            else {
                auto nextIt = std::find_if(neigh.begin(), neigh.end(), [&](const atlas::PointLonLat& candidate) {
                    return candidate != prev && hasRemainingEdge(cur, candidate);
                });
                if (nextIt == neigh.end()) {
                    nextIt = std::find_if(neigh.begin(), neigh.end(), [&](const atlas::PointLonLat& candidate) {
                        return hasRemainingEdge(cur, candidate);
                    });
                }
                if (nextIt == neigh.end()) {
                    ++brokenWalkCount;
                    break;
                }
                next = *nextIt;
            }
            polygon.push_back(next);
            consumeEdge(cur, next);
            prev = cur;
            cur  = next;
        }

        if (cur != start) {
            ++openWalkCount;
        }

        ee_polygons.push_back(std::move(polygon));
    }
    dumpCellToPolygonsSnapshot(snapshotPath, callId, "after_boundary_walk", eeCells, ee_polygons,
                               count_edges.size(), brokenWalkCount, openWalkCount);
    dumpCellToPolygonsCoordinates(snapshotCoordsPath, callId, "after_boundary_walk", ee_polygons);

    // Discard inner (hole) polygons: a polygon whose centroid lies inside a
    // strictly larger sibling polygon is the boundary of an unfired cell that
    // is completely surrounded by fired cells. We fill such holes by removing
    // the inner polygon; the outer boundary already bounds the entire region.
    if (ee_polygons.size() > 1) {
        std::vector<double> areas;
        areas.reserve(ee_polygons.size());
        for (const auto& poly : ee_polygons) {
            areas.push_back(polygonArea(poly));
        }
        std::vector<bool> isHole(ee_polygons.size(), false);
        for (size_t i = 0; i < ee_polygons.size(); ++i) {
            if (isHole[i]) {
                continue;
            }
            const atlas::PointLonLat probe = polygonCentroid(ee_polygons[i]);
            for (size_t j = 0; j < ee_polygons.size(); ++j) {
                if (i == j || isHole[j]) {
                    continue;
                }
                const bool holeByLegacyRule = areas[i] < areas[j] && pointInPolygon(probe, ee_polygons[j]);
                const bool holeByExperimentalRule =
                    (areas[i] < (0.5 * areas[j])) && (containmentRatio(ee_polygons[i], ee_polygons[j]) >= 0.75) &&
                    pointInPolygon(probe, ee_polygons[j]);
                if ((experimentalFix && holeByExperimentalRule) || (!experimentalFix && holeByLegacyRule)) {
                    isHole[i] = true;
                    break;
                }
            }
        }
        std::vector<std::vector<atlas::PointLonLat>> nonHoles;
        nonHoles.reserve(ee_polygons.size());
        for (size_t i = 0; i < ee_polygons.size(); ++i) {
            if (!isHole[i]) {
                nonHoles.push_back(std::move(ee_polygons[i]));
            }
        }
        ee_polygons = std::move(nonHoles);
    }
    dumpCellToPolygonsSnapshot(snapshotPath, callId, "after_hole_removal", eeCells, ee_polygons,
                               count_edges.size(), brokenWalkCount, openWalkCount);
    dumpCellToPolygonsCoordinates(snapshotCoordsPath, callId, "after_hole_removal", ee_polygons);

    // Merge polygons that touch at a vertex into a single connected ring.
    // Without this, a degree-4 junction vertex yields two separate loops.
    bool merged = true;
    while (merged && ee_polygons.size() > 1) {
        merged = false;
        for (size_t i = 0; i < ee_polygons.size() && !merged; ++i) {
            for (size_t j = i + 1; j < ee_polygons.size(); ++j) {
                size_t sharedIdxI = 0;
                size_t sharedIdxJ = 0;
                if (!findSharedVertexIndex(ee_polygons[i], ee_polygons[j], sharedIdxI, sharedIdxJ)) {
                    continue;
                }
                ee_polygons[i] = mergePolygonsAtSharedVertex(ee_polygons[i], ee_polygons[j], sharedIdxI, sharedIdxJ);
                ee_polygons.erase(ee_polygons.begin() + static_cast<std::ptrdiff_t>(j));
                merged = true;
                break;
            }
        }
    }
    dumpCellToPolygonsSnapshot(snapshotPath, callId, "after_vertex_merge", eeCells, ee_polygons,
                               count_edges.size(), brokenWalkCount, openWalkCount);
    dumpCellToPolygonsCoordinates(snapshotCoordsPath, callId, "after_vertex_merge", ee_polygons);
    
    for (auto it = ee_polygons.begin(); it != ee_polygons.end();) {
        if (isDegeneratePolygon(*it)) {
            it = ee_polygons.erase(it);
            continue;
        }
        ++it;
    }
    dumpCellToPolygonsSnapshot(snapshotPath, callId, "after_degenerate_filter", eeCells, ee_polygons,
                               count_edges.size(), brokenWalkCount, openWalkCount);
    dumpCellToPolygonsCoordinates(snapshotCoordsPath, callId, "after_degenerate_filter", ee_polygons);

    // Keep the plugin output convention in [0,360].
    for (auto& polygon : ee_polygons) {
        for (auto& point : polygon) {
            point[0] = normalizeLon360(point.lon());
        }
    }

    return ee_polygons;
}

}  // namespace HEALPixUtils