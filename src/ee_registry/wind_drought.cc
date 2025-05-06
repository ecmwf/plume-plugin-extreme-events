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

#include "atlas/functionspace.h"
#include "eckit/exception/Exceptions.h"

#include "wind_drought.h"

const std::string WindDrought::type_ = "wind_drought";

WindDrought::WindDrought(const eckit::LocalConfiguration& config, plume::data::ModelData& modelData,
                         const std::vector<int>& coarseMapping) :
    ExtremeEvent(config, type_), coarseMapping_(coarseMapping) {
    std::sort(requiredFields_.begin(), requiredFields_.end());
    /** @todo 100u and 100v will not exist after the GRIB2 migration, the event will need a refactor with an updated
     *        Plume interface to access levtype 'hl' and level '100', or it can use the levtype 'ml' and compute the
     *        height from the geopotential.
     */
    if (requiredFields_ != std::vector<std::string>{"100u", "100v"}) {
        throw eckit::BadValue("Wind drought event requires 100m wind component fields.", Here());
    }
    windSpeedCutout_ = static_cast<FIELD_TYPE_REAL>(config.getDouble("wind_speed_cutout"));
    if (windSpeedCutout_ < 0) {
        throw eckit::BadValue("The cutout wind magnitude for the wind drought event should be greater than 0", Here());
    }

    windDroughtSteps_.assign(coarseMapping_.size(), 0);
    timeWindow_  = config.getUnsigned("time_window");
    ntimeSteps_  = std::ceil(timeWindow_ * 60 / modelData.getDouble("TSTEP"));
    description_ = "Wind drought (100m wind speed remains below " + config.getString("wind_speed_cutout") +
                   "m/s for over " + config.getString("time_window") + "minutes)";
}

std::vector<ExtremeEvent::DetectionData> WindDrought::detect(plume::data::ModelData& modelData) {
    std::vector<DetectionData> ee_points;
    auto fieldU = atlas::array::make_view<const FIELD_TYPE_REAL, 2>(modelData.getAtlasFieldShared("100u"));
    auto fieldV = atlas::array::make_view<const FIELD_TYPE_REAL, 2>(modelData.getAtlasFieldShared("100v"));
    auto halo = atlas::array::make_view<int, 1>(modelData.getAtlasFieldShared("100u").functionspace().ghost());
    // 1. Compute spatial wind speed average & update count for each cell
    for (atlas::idx_t idx = 0; idx < coarseMapping_.size(); idx++) {
        if (halo(idx) > 0) {
            continue; // Halo counters stay at 0 so they have no effect
        }

        FIELD_TYPE_REAL windMagnitude = std::sqrt(fieldU(idx, 0) * fieldU(idx, 0) + fieldV(idx, 0) * fieldV(idx, 0));
        if (windMagnitude < windSpeedCutout_) {
            ++windDroughtSteps_[idx];
        }
        else {
            windDroughtSteps_[idx] = 0; // This resets the counter if the wind finally exceeds the threshold
        }
    }

    // 2. Detect the cells that have not exceeded the wind threshold during the time window
    /** @todo Extremes DT output these fields as levtype 'hl' levelist '100', the keys will need an update for
     *        GRIB2 compatibility. Ideally it is fetched from Plume or the field metadata instead of being hardcoded.
     */
    ee_points.push_back({{}, description_, "100u/100v", "sfc", "0"});
    for (atlas::idx_t idx = 0; idx < coarseMapping_.size(); idx++) {
        if (ntimeSteps_ < windDroughtSteps_[idx]) {
            ee_points[0].detectedCells.insert(coarseMapping_[idx]);
        }
    }
    return ee_points;
}

WindDrought::Registrar WindDrought::registrar;