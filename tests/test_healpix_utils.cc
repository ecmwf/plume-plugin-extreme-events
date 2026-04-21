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

#include <cmath>
#include <set>

#include "atlas/functionspace.h"
#include "atlas/mesh.h"
#include "atlas/util/Config.h"
#include "eckit/testing/Test.h"

#include "healpix_utils.h"

using namespace eckit::testing;

namespace {

bool inLonRange(const atlas::PointLonLat& point) {
    return point.lon() >= 0.0 && point.lon() <= 360.0;
}

bool spansDateline(const std::vector<atlas::PointLonLat>& polygon) {
    double minLon = 360.0;
    double maxLon = 0.0;
    for (const auto& point : polygon) {
        minLon = std::min(minLon, point.lon());
        maxLon = std::max(maxLon, point.lon());
    }
    return minLon < 30.0 && maxLon > 330.0;
}

bool hasImmediateBacktrack(const std::vector<atlas::PointLonLat>& polygon) {
    if (polygon.size() < 3) {
        return false;
    }
    for (size_t i = 1; i + 1 < polygon.size(); ++i) {
        if (polygon[i - 1] == polygon[i + 1]) {
            return true;
        }
    }
    return false;
}

bool near(double a, double b, double tol = 1e-9) {
    return std::fabs(a - b) <= tol;
}

bool containsPoint(const std::vector<atlas::PointLonLat>& polygon, double lon, double lat, double tol = 1e-9) {
    for (const auto& point : polygon) {
        if (near(point.lon(), lon, tol) && near(point.lat(), lat, tol)) {
            return true;
        }
    }
    return false;
}

}  // namespace

namespace test {

CASE("cellToPolygons merges wrapped 360-0 boundary cells") {
    std::vector<std::vector<atlas::PointLonLat>> vertices(2);

    // West fragment near 360.
    vertices[0] = {
        atlas::PointLonLat{350.0, 10.0},
        atlas::PointLonLat{360.0, 10.0},
        atlas::PointLonLat{360.0, 20.0},
        atlas::PointLonLat{350.0, 20.0},
    };

    // East fragment near 0, sharing the same meridian edge modulo 360.
    vertices[1] = {
        atlas::PointLonLat{0.0, 10.0},
        atlas::PointLonLat{10.0, 10.0},
        atlas::PointLonLat{10.0, 20.0},
        atlas::PointLonLat{0.0, 20.0},
    };

    std::set<int> eeCells = {0, 1};
    auto polygons = HEALPixUtils::cellToPolygons(eeCells, vertices);

    EXPECT_EQUAL(polygons.size(), 1);
    EXPECT(spansDateline(polygons.front()));

    for (const auto& p : polygons.front()) {
        EXPECT(inLonRange(p));
    }
}

CASE("cellToPolygons keeps disconnected events separated") {
    std::vector<std::vector<atlas::PointLonLat>> vertices(2);
    vertices[0] = {
        atlas::PointLonLat{40.0, 10.0},
        atlas::PointLonLat{50.0, 10.0},
        atlas::PointLonLat{50.0, 20.0},
        atlas::PointLonLat{40.0, 20.0},
    };
    vertices[1] = {
        atlas::PointLonLat{140.0, -10.0},
        atlas::PointLonLat{150.0, -10.0},
        atlas::PointLonLat{150.0, 0.0},
        atlas::PointLonLat{140.0, 0.0},
    };

    std::set<int> eeCells = {0, 1};
    auto polygons = HEALPixUtils::cellToPolygons(eeCells, vertices);

    EXPECT_EQUAL(polygons.size(), 2);
    for (const auto& polygon : polygons) {
        for (const auto& point : polygon) {
            EXPECT(inLonRange(point));
        }
    }
}

CASE("cellToPolygons merges reported storm fragments across 360-0 seam") {
    std::vector<std::vector<atlas::PointLonLat>> vertices(2);

    // Reported west fragment (337.5 -> 360 side), represented as one HEALPix-like cell.
    vertices[0] = {
        atlas::PointLonLat{337.5, 66.44353569},
        atlas::PointLonLat{342.0, 60.43443884},
        atlas::PointLonLat{360.0, 66.44353569},
        atlas::PointLonLat{360.0, 72.38756093},
    };

    // Reported east fragment (0 -> 22.5 side), sharing seam vertices modulo 360.
    vertices[1] = {
        atlas::PointLonLat{0.0, 66.44353569},
        atlas::PointLonLat{18.0, 60.43443884},
        atlas::PointLonLat{22.5, 66.44353569},
        atlas::PointLonLat{0.0, 72.38756093},
    };

    std::set<int> eeCells = {0, 1};
    auto polygons = HEALPixUtils::cellToPolygons(eeCells, vertices);

    EXPECT_EQUAL(polygons.size(), 1);

    bool hasNearZeroLon = false;
    bool hasNear360Lon  = false;
    for (const auto& point : polygons.front()) {
        EXPECT(inLonRange(point));
        hasNearZeroLon = hasNearZeroLon || point.lon() < 25.0;
        hasNear360Lon  = hasNear360Lon || point.lon() > 335.0;
    }

    EXPECT(hasNearZeroLon);
    EXPECT(hasNear360Lon);
    EXPECT(spansDateline(polygons.front()));
}

CASE("Atlas HEALPix seam scenario produces valid 0-360 polygons") {
    atlas::Grid grid("H2");
    atlas::util::Config healpixConfig;
    healpixConfig.set("pole_elements", "pentagons");
    healpixConfig.set("mpi_comm", "self");

    atlas::Mesh hpMesh(grid, healpixConfig);
    atlas::functionspace::CellColumns fs(hpMesh);

    std::vector<int> mapping;
    std::vector<std::vector<atlas::PointLonLat>> cellVertices;
    HEALPixUtils::mapLonLatToHEALPixCell(2, fs, mapping, cellVertices);

    auto lonlat = atlas::array::make_view<double, 2>(fs.lonlat());
    std::set<int> seamCells;

    // Select a contiguous strip around the 360/0 seam near the equator.
    for (atlas::idx_t i = 0; i < lonlat.shape(0); ++i) {
        const double lon = lonlat(i, 0);
        const double lat = lonlat(i, 1);
        if ((lon > 330.0 || lon < 30.0) && std::fabs(lat) < 30.0) {
            seamCells.insert(mapping[i]);
        }
    }

    EXPECT(!seamCells.empty());
    auto polygons = HEALPixUtils::cellToPolygons(seamCells, cellVertices);
    EXPECT(!polygons.empty());

    bool hasDatelineSpan = false;
    for (const auto& polygon : polygons) {
        for (const auto& point : polygon) {
            EXPECT(inLonRange(point));
        }
        hasDatelineSpan = hasDatelineSpan || spansDateline(polygon);
    }

    EXPECT(hasDatelineSpan);
}

CASE("cellToPolygons fills holes from surrounded unfired cells") {
    // 3x3 grid of 10-degree square cells. Cell 4 (centre, [10-20] lon x [10-20] lat)
    // is NOT fired. All 8 surrounding cells are fired.
    // Without hole filling: two polygons (outer boundary + inner hole boundary).
    // With hole filling: exactly one outer polygon.
    std::vector<std::vector<atlas::PointLonLat>> vertices(9);
    int idx = 0;
    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 3; ++col) {
            const double x0 = col * 10.0;
            const double y0 = row * 10.0;
            vertices[idx++] = {
                atlas::PointLonLat{x0,        y0},
                atlas::PointLonLat{x0 + 10.0, y0},
                atlas::PointLonLat{x0 + 10.0, y0 + 10.0},
                atlas::PointLonLat{x0,        y0 + 10.0},
            };
        }
    }

    // Fire all cells except the centre (index 4, covering [10,20] x [10,20]).
    std::set<int> eeCells = {0, 1, 2, 3, 5, 6, 7, 8};
    auto polygons = HEALPixUtils::cellToPolygons(eeCells, vertices);

    // Hole must be discarded: only the outer boundary polygon should remain.
    EXPECT_EQUAL(polygons.size(), 1);

    for (const auto& point : polygons.front()) {
        EXPECT(inLonRange(point));
    }
}

CASE("cellToPolygons fills holes when outer boundary crosses 360-0 seam") {
    // 3x3 grid crossing the seam in longitude:
    // columns are [340,350], [350,360], [0,10] and rows are [0,10], [10,20], [20,30].
    // Centre cell (index 4, [350,360]x[10,20]) is unfired while all neighbours fire.
    std::vector<std::vector<atlas::PointLonLat>> vertices(9);

    const double lonBounds[4] = {340.0, 350.0, 360.0, 10.0};
    int idx = 0;
    for (int row = 0; row < 3; ++row) {
        const double y0 = row * 10.0;
        for (int col = 0; col < 3; ++col) {
            const double x0 = lonBounds[col];
            const double x1 = lonBounds[col + 1];
            vertices[idx++] = {
                atlas::PointLonLat{x0, y0},
                atlas::PointLonLat{x1, y0},
                atlas::PointLonLat{x1, y0 + 10.0},
                atlas::PointLonLat{x0, y0 + 10.0},
            };
        }
    }

    std::set<int> eeCells = {0, 1, 2, 3, 5, 6, 7, 8};
    auto polygons = HEALPixUtils::cellToPolygons(eeCells, vertices);

    EXPECT_EQUAL(polygons.size(), 1);
    EXPECT(spansDateline(polygons.front()));

    bool hasNearZeroLon = false;
    bool hasNear360Lon  = false;
    for (const auto& point : polygons.front()) {
        EXPECT(inLonRange(point));
        hasNearZeroLon = hasNearZeroLon || point.lon() < 20.0;
        hasNear360Lon  = hasNear360Lon || point.lon() > 330.0;
    }
    EXPECT(hasNearZeroLon);
    EXPECT(hasNear360Lon);
}

CASE("cellToPolygons walk avoids longitude-tie backtracking") {
    // Two stacked cells form a single rectangle with intermediate collinear
    // vertices along lon=0 and lon=10. At these vertices, neighbors can share
    // the same longitude and the walk must still progress without bouncing.
    std::vector<std::vector<atlas::PointLonLat>> vertices(2);
    vertices[0] = {
        atlas::PointLonLat{0.0, 0.0},
        atlas::PointLonLat{10.0, 0.0},
        atlas::PointLonLat{10.0, 10.0},
        atlas::PointLonLat{0.0, 10.0},
    };
    vertices[1] = {
        atlas::PointLonLat{0.0, 10.0},
        atlas::PointLonLat{10.0, 10.0},
        atlas::PointLonLat{10.0, 20.0},
        atlas::PointLonLat{0.0, 20.0},
    };

    std::set<int> eeCells = {0, 1};
    auto polygons = HEALPixUtils::cellToPolygons(eeCells, vertices);

    EXPECT_EQUAL(polygons.size(), 1);
    EXPECT(!hasImmediateBacktrack(polygons.front()));

    for (const auto& point : polygons.front()) {
        EXPECT(inLonRange(point));
    }
}

CASE("cellToPolygons merges components touching at one vertex") {
    // Regression: two event components touching only at one vertex should be
    // treated as one connected polygon in plugin output.
    std::vector<std::vector<atlas::PointLonLat>> vertices(2);

    vertices[0] = {
        atlas::PointLonLat{163.125, 24.62431835},
        atlas::PointLonLat{168.75, 19.47122063},
        atlas::PointLonLat{174.375, 24.62431835},
        atlas::PointLonLat{180.0, 19.47122063},
        atlas::PointLonLat{185.625, 24.62431835},
        atlas::PointLonLat{191.25, 30.0},
        atlas::PointLonLat{185.625, 35.68533471},
        atlas::PointLonLat{180.0, 41.8103149},
        atlas::PointLonLat{174.375, 35.68533471},
        atlas::PointLonLat{168.75, 41.8103149},
        atlas::PointLonLat{163.125, 35.68533471},
        atlas::PointLonLat{168.75, 30.0},
    };

    vertices[1] = {
        atlas::PointLonLat{191.25, 30.0},
        atlas::PointLonLat{196.875, 24.62431835},
        atlas::PointLonLat{202.5, 19.47122063},
        atlas::PointLonLat{208.125, 24.62431835},
        atlas::PointLonLat{213.75, 19.47122063},
        atlas::PointLonLat{219.375, 24.62431835},
        atlas::PointLonLat{225.0, 30.0},
        atlas::PointLonLat{230.625, 35.68533471},
        atlas::PointLonLat{225.0, 41.8103149},
        atlas::PointLonLat{218.5714286, 48.14120779},
        atlas::PointLonLat{210.0, 54.3409123},
        atlas::PointLonLat{205.7142857, 48.14120779},
        atlas::PointLonLat{195.0, 54.3409123},
        atlas::PointLonLat{192.8571429, 48.14120779},
        atlas::PointLonLat{191.25, 41.8103149},
        atlas::PointLonLat{196.875, 35.68533471},
    };

    std::set<int> eeCells = {0, 1};
    auto polygons = HEALPixUtils::cellToPolygons(eeCells, vertices);

    EXPECT_EQUAL(polygons.size(), 1);
    EXPECT(containsPoint(polygons.front(), 191.25, 30.0, 1e-7));
}

CASE("cellToPolygons discards pole point artifacts") {
    // Regression: all longitudes at either pole represent the same physical
    // point. These rings must not be emitted as polygons.
    std::vector<std::vector<atlas::PointLonLat>> vertices(2);
    vertices[0] = {
        atlas::PointLonLat{0.0, -90.0},
        atlas::PointLonLat{90.0, -90.0},
        atlas::PointLonLat{180.0, -90.0},
        atlas::PointLonLat{270.0, -90.0},
    };

    vertices[1] = {
        atlas::PointLonLat{0.0, 90.0},
        atlas::PointLonLat{90.0, 90.0},
        atlas::PointLonLat{180.0, 90.0},
        atlas::PointLonLat{270.0, 90.0},
    };

    std::set<int> eeCells = {0, 1};
    auto polygons = HEALPixUtils::cellToPolygons(eeCells, vertices);

    EXPECT(polygons.empty());
}

CASE("cellToPolygons keeps polar region when connected through pole vertex") {
    // A regular high-latitude cell touching the north pole and a degenerate
    // pole ring must produce a retained polygon (not fully discarded).
    std::vector<std::vector<atlas::PointLonLat>> vertices(2);

    vertices[0] = {
        atlas::PointLonLat{330.0, 84.14973294},
        atlas::PointLonLat{0.0, 90.0},
        atlas::PointLonLat{30.0, 84.14973294},
        atlas::PointLonLat{0.0, 78.28414761},
    };

    // Degenerate north-pole artifact ring (all points are the same physical location).
    vertices[1] = {
        atlas::PointLonLat{90.0, 90.0},
        atlas::PointLonLat{180.0, 90.0},
        atlas::PointLonLat{270.0, 90.0},
        atlas::PointLonLat{0.0, 90.0},
    };

    std::set<int> eeCells = {0, 1};
    auto polygons = HEALPixUtils::cellToPolygons(eeCells, vertices);

    EXPECT_EQUAL(polygons.size(), 1);
    EXPECT(!polygons.front().empty());
}

CASE("cellToPolygons handles large mid-latitude HEALPix cluster without fragmentation") {
    // Regression fixture for the step-3 NH dropout:
    // A large contiguous cluster of HEALPix cells at H8 resolution covering
    // a 90°×40° mid-latitude box produced many tiny polygon fragments with
    // the legacy undirected walk but a single correct polygon with the
    // directed-edge traversal.
    //
    // The H8 grid gives ~768 cells; the box lon[20,110] x lat[25,65] selects
    // roughly 60-80 cells whose boundary has enough topology (irregular cell
    // shapes, shared-vertex junctions) to expose the walk fragmentation.
    atlas::Grid grid("H8");
    atlas::util::Config healpixConfig;
    healpixConfig.set("pole_elements", "pentagons");
    healpixConfig.set("mpi_comm", "self");

    atlas::Mesh hpMesh(grid, healpixConfig);
    atlas::functionspace::CellColumns fs(hpMesh);

    std::vector<int> mapping;
    std::vector<std::vector<atlas::PointLonLat>> cellVertices;
    HEALPixUtils::mapLonLatToHEALPixCell(8, fs, mapping, cellVertices);

    // Select all HEALPix cells whose centre falls inside the target box.
    auto lonlat = atlas::array::make_view<double, 2>(fs.lonlat());
    std::set<int> clusterCells;
    for (int i = 0; i < static_cast<int>(cellVertices.size()); ++i) {
        const double lon = lonlat(i, 0);
        const double lat = lonlat(i, 1);
        if (lon >= 20.0 && lon <= 110.0 && lat >= 25.0 && lat <= 65.0) {
            clusterCells.insert(i);
        }
    }

    EXPECT(!clusterCells.empty());

    auto polygons = HEALPixUtils::cellToPolygons(clusterCells, cellVertices);

    // The selected box is a single geographically connected region. The walk
    // must yield exactly one outer polygon, not a scattering of tiny fragments.
    EXPECT_EQUAL(polygons.size(), 1);

    for (const auto& point : polygons.front()) {
        EXPECT(inLonRange(point));
    }
    EXPECT(!hasImmediateBacktrack(polygons.front()));

    // Sanity-check that the polygon covers the expected longitude range.
    bool hasWestEdge = false;
    bool hasEastEdge = false;
    for (const auto& point : polygons.front()) {
        hasWestEdge = hasWestEdge || point.lon() <= 25.0;
        hasEastEdge = hasEastEdge || point.lon() >= 105.0;
    }
    EXPECT(hasWestEdge);
    EXPECT(hasEastEdge);
}

}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
