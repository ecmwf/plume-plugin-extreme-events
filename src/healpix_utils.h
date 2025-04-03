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
#include "atlas/grid.h"

namespace HEALPixUtils {

/**
 * @brief Creates a mapping of grid points from a given function space to the HEALPix cell they belong to.
 * 
 * This mapping can then be used to represent an extreme event region coarser than the model resolution.
 * If grid point `i` fires, `k = mappingVector[i]` is the index of the HEALPix cell it belongs to, and
 * `cellVertices[k]` is the vertices of that cell, which can be used to define a polygon on the globe.
 * 
 * @param[in] resolution The HEALPix resolution to use for the HEALPix mesh.
 * @param[in] modelFS The function space to map to HEALPix cells.
 * @param[out] mappingVector The vector mapping grid point remote indices to global HEALPix cell indices.
 * @param[out] cellVertices The vector mapping a HEALPix cell index to its 4 (or 5 for poles) vertex coordinates.
 */
void mapLonLatToHEALPixCell(int resolution, const atlas::FunctionSpace& modelFS, std::vector<int>& mappingVector,
                            std::vector<std::vector<atlas::PointLonLat>>& cellVertices);

/**
 * @brief Extracts HEALPix polygons from given firing points.
 * 
 * This extraction function uses a map of the edges of the cells to determine contigous events, remove inner edges
 * which are not on a polygon boundary, and traverse vertices in a counter clockwise manner to ensure points are
 * populated in an order that correctly defines a polygon.
 * 
 * @param eeIndices The indices of the firing points on the model function space.
 * @param mapping The grid point to HEALPix cell mapping vector.
 * @param vertices The HEALPix cell to its vertices coordinates mapping vector.
 * 
 * @return A vector containing all the polygons extracted from the firing points.
 * 
 * @warning All edge cases are not handled: - polygons with holes (holes are misclassified as single events)
 *                                          - global HEALPix mesh firing (the event is discarded)
 */
std::vector<std::vector<atlas::PointLonLat>> cellToPolygons(std::vector<int>& eeIndices, std::vector<int>& mapping,
                                                            std::vector<std::vector<atlas::PointLonLat>>& vertices);

}  // namespace HEALPixUtils