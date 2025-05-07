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
#include <algorithm>
#include <cmath>
#include <iostream>
#include <cstring>
#include <limits>
#include <unordered_map>

#include "atlas/array.h"
#include "eckit/exception/Exceptions.h"

#include "storm.h"

const std::string Storm::type_ = "storm";

Storm::Storm(const eckit::LocalConfiguration& config, plume::data::ModelData& modelData,
             const std::vector<int>& coarseMapping) :
    ExtremeEvent(config, type_), coarseMapping_(coarseMapping) {
    std::sort(requiredFields_.begin(), requiredFields_.end());
    /** @todo 100u and 100v will not exist after the GRIB2 migration, the event will need a refactor with an updated
     *        Plume interface to access levtype 'hl' and level '100', or it can use the levtype 'ml' and compute the
     *        height from the geopotential.
     */ 
    if (requiredFields_ != std::vector<std::string>{"100u", "100v"}) {
        throw eckit::BadValue("Storm requires 100m wind component fields.", Here());
    }
    double windSpeedCutout = config.getDouble("wind_speed_cutout");
    if (windSpeedCutout < 0 || windSpeedCutout > (std::numeric_limits<uint16_t>::max() / 100)) {
        throw eckit::BadValue("The cutout wind speed for the storm event should be between 0 and " + std::to_string(std::numeric_limits<uint16_t>::max() / 100), Here());
    }

    windSpeedCutout_ = static_cast<uint16_t>(std::round(windSpeedCutout * 100));
    timeWindow_      = config.getUnsigned("time_window");
    ntimeSteps_      = std::ceil(timeWindow_ * 60 / modelData.getDouble("TSTEP"));
    windSpeeds_      = std::deque<uint16_t>(ntimeSteps_ * coarseMapping_.size(), 0);
    description_     = "Storm (100m wind speed average over " + config.getString("time_window") + "min exceeding " +
                   config.getString("wind_speed_cutout") + "m/s)";
}

std::vector<ExtremeEvent::DetectionData> Storm::detect(plume::data::ModelData& modelData) {
    std::vector<DetectionData> ee_points;
    auto fieldU = atlas::array::make_view<const FIELD_TYPE_REAL, 2>(modelData.getAtlasFieldShared("100u"));
    auto fieldV = atlas::array::make_view<const FIELD_TYPE_REAL, 2>(modelData.getAtlasFieldShared("100v"));
    // 1. Slide the wind speeds window with current time step values
    windSpeeds_.erase(windSpeeds_.begin() + (ntimeSteps_ - 1) * coarseMapping_.size(), windSpeeds_.end());
    // reverse inserting element to maintain indices
    for (atlas::idx_t idx = coarseMapping_.size() - 1; idx >= 0; idx--) {
        FIELD_TYPE_REAL windMagnitude = std::sqrt(fieldU(idx, 0) * fieldU(idx, 0) + fieldV(idx, 0) * fieldV(idx, 0));
        windSpeeds_.push_front(static_cast<uint16_t>(std::round(windMagnitude * 100.0)));
    }
    

    if (modelData.getInt("NSTEP") < ntimeSteps_) { // Fill the wind speed array but do not run detection yet
        return ee_points;
    }

    // 2. Compute temporal average and run detection on the time window
    std::unordered_map<int, uint32_t> cellMaximums;
    for (atlas::idx_t idx = 0; idx < coarseMapping_.size(); idx++){
        uint32_t windAvg = 0;
        for (size_t tstep = 0; tstep < ntimeSteps_; tstep++) {
            windAvg += windSpeeds_[tstep * coarseMapping_.size() + idx];
        }
        if (cellMaximums.find(coarseMapping_[idx]) == cellMaximums.end()) {
            cellMaximums[coarseMapping_[idx]] = windAvg;
        } else {
            cellMaximums[coarseMapping_[idx]] = std::max(cellMaximums[coarseMapping_[idx]], windAvg);
        }
    }

    /** @todo Extremes DT output these fields as levtype 'hl' levelist '100', the keys will need an update for
     *        GRIB2 compatibility. Ideally it is fetched from Plume or the field metadata instead of being hardcoded.
     */
    ee_points.push_back({{}, description_, "100u/100v", "sfc", "0"});
    for (const auto& [cell, max] : cellMaximums) {
        if (max > windSpeedCutout_ * ntimeSteps_) {
            ee_points[0].detectedCells.insert(cell);
        }
    }

    return ee_points;
}

Storm::Registrar Storm::registrar;