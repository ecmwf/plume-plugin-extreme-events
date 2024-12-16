#include "atlas/field/Field.h"
#include "atlas/grid.h"

namespace HEALPixUtils {

void mapLonLatToHEALPixCell(
    int resolution, const atlas::Field &fieldToMap,
    std::vector<int> &mappingVector,
    std::vector<std::vector<atlas::PointLonLat>> &cellVertices);

std::vector<std::vector<atlas::PointLonLat>>
cellToPolygons(std::vector<int> &ee_indices, std::vector<int> &mapping,
               std::vector<std::vector<atlas::PointLonLat>> &vertices);

} // namespace HEALPixUtils