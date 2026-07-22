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
#include <cstring>
#include <iostream>
#include <unordered_map>

#include "atlas/array.h"
#include "atlas/field.h"
#include "atlas/functionspace.h"
#include "eckit/exception/Exceptions.h"

#include "storm.h"

const std::string Storm::type_ = "storm";

Storm::Storm(const eckit::LocalConfiguration& config, plume::data::ModelDataView& modelData,
             const std::vector<int>& coarseMapping) :
    ExtremeEvent(config, type_), coarseMapping_(coarseMapping) {
    const auto& fields = requiredFields();
    const bool hasU    = std::find(fields.begin(), fields.end(), "u") != fields.end();
    const bool hasV    = std::find(fields.begin(), fields.end(), "v") != fields.end();
    if (!hasU || !hasV) {
        throw eckit::BadValue("Storm requires two components of wind fields.", Here());
    }
    windSpeedCutout_ = static_cast<FIELD_TYPE_REAL>(config.getDouble("wind_speed_cutout"));
    if (windSpeedCutout_ < 0) {
        throw eckit::BadValue("The cutout wind speed for the storm event should be positive", Here());
    }

    levtype_ = config.has("model_level") ? "ml" : "hl";
    level_   = heightLevel().value_or(config.getUnsigned("model_level", 1));
    // There is no need for height check as Plume update strategy will already have validated it, unless an event has a
    // specific height cap it needs to enforce.
    if (levtype_ == "ml" && level_ > modelData.getParam<int>("NFLEVG")) {
        throw eckit::BadValue(
            "The specified level for the storm event is higher than the number of levels in the model", Here());
    }
    std::string levelString = levtype_ == "ml" ? "ml " + std::to_string(level_) : std::to_string(level_) + "m";

    timeWindow_  = config.getUnsigned("time_window");
    ntimeSteps_  = std::ceil(timeWindow_ * 60 / modelData.getParam<double>("TSTEP"));
    windSpeeds_  = std::deque<FIELD_TYPE_REAL>(ntimeSteps_ * coarseMapping_.size(), 0);
    description_ = "Storm (wind speed average over " + config.getString("time_window") + "min exceeding " +
                   config.getString("wind_speed_cutout") + "m/s at " + levelString + ")";
}

std::vector<ExtremeEvent::DetectionData> Storm::detect(plume::data::ModelDataView& modelData) {
    std::vector<DetectionData> ee_points;

    // 1. Slide the wind speeds window with current time step values, support for both ml (3D) and hl (2D)
    auto slideWindSpeeds = [&](const auto& u, const auto& v, const auto& halo, auto accessor) {
        // reverse inserting element to maintain indices
        for (atlas::idx_t idx = coarseMapping_.size() - 1; idx >= 0; idx--) {
            if (halo(idx) > 0) {
                windSpeeds_.push_front(0);  // Add values with no effect for halo points
            }
            else {
                windSpeeds_.push_front(
                    std::sqrt(accessor(u, idx) * accessor(u, idx) + accessor(v, idx) * accessor(v, idx)));
            }
        }
    };
    windSpeeds_.erase(windSpeeds_.begin() + (ntimeSteps_ - 1) * coarseMapping_.size(), windSpeeds_.end());

    if (levtype_ == "ml") {
        auto halo = atlas::array::make_view<int, 1>(modelData.getParam<atlas::Field>("u").functionspace().ghost());
        auto u    = atlas::array::make_view<const FIELD_TYPE_REAL, 2>(modelData.getParam<atlas::Field>("u"));
        auto v    = atlas::array::make_view<const FIELD_TYPE_REAL, 2>(modelData.getParam<atlas::Field>("v"));
        // Model levels are 1-indexed in the input files but 0-indexed in the code, hence the -1
        slideWindSpeeds(u, v, halo, [this](const auto& field, atlas::idx_t idx) { return field(idx, level_ - 1); });
    }
    else {
        // (wind at) height levels are 2D fields (3D fields with a single level owned by Plume)
        auto uField = modelData.getParam<atlas::Field>("u", std::to_string(level_));
        auto vField = modelData.getParam<atlas::Field>("v", std::to_string(level_));
        auto halo   = atlas::array::make_view<int, 1>(uField.functionspace().ghost());
        auto u      = atlas::array::make_view<const FIELD_TYPE_REAL, 2>(uField);
        auto v      = atlas::array::make_view<const FIELD_TYPE_REAL, 2>(vField);
        slideWindSpeeds(u, v, halo, [this](const auto& field, atlas::idx_t idx) { return field(idx, 0); });
    }
    // Fill the wind speed array but do not run detection yet
    if (modelData.getParam<int>("NSTEP") < ntimeSteps_) {
        return ee_points;
    }

    // 2. Compute temporal average and run detection on the time window
    std::unordered_map<int, FIELD_TYPE_REAL> cellMaximums;
    for (atlas::idx_t idx = 0; idx < coarseMapping_.size(); idx++) {
        FIELD_TYPE_REAL windAvg = 0;
        for (size_t tstep = 0; tstep < ntimeSteps_; tstep++) {
            windAvg += windSpeeds_[tstep * coarseMapping_.size() + idx];
        }
        windAvg /= ntimeSteps_;
        if (cellMaximums.find(coarseMapping_[idx]) == cellMaximums.end()) {
            cellMaximums[coarseMapping_[idx]] = windAvg;
        }
        else {
            cellMaximums[coarseMapping_[idx]] = std::max(cellMaximums[coarseMapping_[idx]], windAvg);
        }
    }

    // 3. Build the detection result with the cell indices that exceed the cutout value
    ee_points.push_back({{}, description_, "u/v", levtype_, std::to_string(level_)});
    for (const auto& [cell, max] : cellMaximums) {
        if (max > windSpeedCutout_) {
            ee_points[0].detectedCells.insert(cell);
        }
    }

    return ee_points;
}

Storm::Registrar Storm::registrar;