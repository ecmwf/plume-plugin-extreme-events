/*
 * (C) Copyright 2023- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */

#include <memory>
#include <vector>

#include "atlas/option.h"
#include "atlas/parallel/mpi/mpi.h"
#include "atlas/runtime/Log.h"

#include "atlas/array/ArrayView.h"
#include "atlas/grid/Grid.h"
#include "atlas/grid/StructuredGrid.h"
#include "atlas/grid/detail/grid/Gaussian.h"
#include "atlas/grid/Distribution.h"
#include "atlas/grid/Vertical.h"
#include "atlas/util/function/VortexRollup.h"
#include "atlas/util/CoordinateEnums.h"

#include "atlas/util/function/VortexRollup.h"

#include "nwp_utils.h"
#include "../src/plugin_types.h"

using atlas::Log;

namespace nwp_utils {

void FieldGenerator::setupFunctionSpace() {

    long n_procs = atlas::mpi::comm().size();

    // ------------------ grid ------------------
    std::vector<long> gaussian_x(gridSizeX_,gridSizeY_);
    atlas::StructuredGrid grid = atlas::grid::detail::grid::reduced_gaussian( gaussian_x );
    atlas::grid::Distribution distribution(grid, 
                                           atlas::util::Config("type", "checkerboard") | 
                                           atlas::util::Config("bands", n_procs));

    // ----------- function space ---------------
    fs_ = atlas::functionspace::StructuredColumns(grid, 
                                                    distribution, 
                                                    atlas::util::Config("halo", nHalo_) | 
                                                    atlas::util::Config("levels", nLevels_) );

}

atlas::Field FieldGenerator::createField3D(const std::string& name){
    return createField(name, nLevels_);
}

atlas::Field FieldGenerator::createField2D(const std::string& name){
    return createField(name, 1);
}

atlas::Field FieldGenerator::createField(const std::string& name, int n_levels){
    atlas::Field field_source = fs_.createField<FIELD_TYPE_REAL>(atlas::option::name(name) | 
                                                        atlas::option::levels(n_levels));
    
    auto lonlat = atlas::array::make_view<FIELD_TYPE_REAL,2>(fs_.xy());
    auto source = atlas::array::make_view<FIELD_TYPE_REAL,2>(field_source);

    int n_vertical_lev = source.shape(1);

    // fill-in field with vortex rollup
    for (atlas::idx_t i_pt = 0; i_pt < fs_.size(); i_pt++) {
        for (atlas::idx_t i_ver = 0; i_ver < n_levels; i_ver++) {
            double t = double(i_ver)/n_levels;
            double lon = lonlat(i_pt, atlas::LON);
            double lat = lonlat(i_pt, atlas::LAT);
            FIELD_TYPE_REAL val = atlas::util::function::vortex_rollup(lon, lat, t);

            source(i_pt, i_ver) = val;
        }
    }

    Log::info() << "Created field: " << name << ", with shape" << field_source.shape() << std::endl;

    return field_source;
}

void updateField(atlas::Field& field, int tstep) {
    auto source = atlas::array::make_view<FIELD_TYPE_REAL, 2>(field);
    auto lonlat = atlas::array::make_view<FIELD_TYPE_REAL, 2>(field.functionspace().lonlat());
    std::vector<std::vector<std::pair<double, double>>> demoHighCoords = {
        {{0,41.5325}},
        {{0,41.5325}, {90,41.5325}},
        {{0,41.5325}, {90,41.5325}, {95.625,35.9951}},
        {{90,41.5325}, {95.625,35.9951}, {180,41.5325}, {191.25,41.5325}},
        {{180,41.5325}, {191.25,41.5325}, {157.5,2.7689}, {225,41.5325}, {270,41.5325}},
        {{191.25,41.5325}, {225,41.5325}, {270,41.5325}, {95.625,35.9951}},
        {{225,41.5325}, {270,41.5325}, {95.625,35.9951}},
        {{270,41.5325}, {95.625,35.9951}},
        {{95.625,35.9951}},
        {}
    };

    std::vector<std::vector<std::pair<double, double>>> demoLowCoords = {
        {{270,-19.3822}},
        {{270,-19.3822}, {225,-24.9199}},
        {{270,-19.3822}, {225,-24.9199}, {180,-24.9199}},
        {{225,-24.9199}, {180,-24.9199}, {135,-24.9199}},
        {{180,-24.9199}, {135,-24.9199}, {180,-41.5325}},
        {{135,-24.9199}, {180,-41.5325}},
        {{135,-24.9199}, {180,-41.5325}, {202.5,-47.0696}},
        {{180,-41.5325}, {202.5,-47.0696}},
        {{202.5,-47.0696}},
        {}
    };

    for (atlas::idx_t idx = 0; idx < source.size(); idx++) {
        source(idx, 0) = 10.0;
        for (auto &ee_point : demoHighCoords[tstep]) {
            if (std::abs(lonlat(idx, 0) - ee_point.first) < 0.1 && std::abs(lonlat(idx, 1) - ee_point.second) < 0.1) {
                source(idx, 0) = 30.0;
                break;
            }
        }
        for (auto &ee_point : demoLowCoords[tstep]) {
            if (std::abs(lonlat(idx, 0) - ee_point.first) < 0.1 && std::abs(lonlat(idx, 1) - ee_point.second) < 0.1) {
                source(idx, 0) = 0.0;
                break;
            }
        }
    }
}

} // namespace nwp_util



extern "C" {

double vortex_roll(double lon, double lat, double t) {
    return atlas::util::function::vortex_rollup(lon, lat, t);
}

}