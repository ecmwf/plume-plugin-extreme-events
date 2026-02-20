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
#include <sstream>
#include <unordered_map>

#include "atlas/array.h"
#include "atlas/functionspace.h"
#include "eckit/exception/Exceptions.h"

#include "ramp.h"

const std::string RampEvent::type_ = "ramp";

RampEvent::RampEvent(const eckit::LocalConfiguration& config, plume::data::ModelData& modelData,
                     const std::vector<int>& coarseMapping) :
    ExtremeEvent(config, type_), coarseMapping_(coarseMapping) {
    if (config.has("ramp_up_value")) {
        rampUpValue_ = static_cast<FIELD_TYPE_REAL>(config.getDouble("ramp_up_value"));
        rampUp_      = true;
    }
    if (config.has("ramp_down_value")) {
        rampDownValue_ = static_cast<FIELD_TYPE_REAL>(config.getDouble("ramp_down_value"));
        rampDown_      = true;
    }
    if ((rampUp_ && rampUpValue_ < 1e-6) || (rampDown_ && rampDownValue_ < 1e-6) || (!rampUp_ && !rampDown_)) {
        throw eckit::BadValue(
            "The ramp values for the ramp event should be positive, and at least one of them should be provided",
            Here());
    }

    timeWindow_     = config.getUnsigned("time_window");
    ntimeSteps_     = std::ceil(timeWindow_ * 60 / modelData.getParam<double>("TSTEP"));
    previousValues_ = std::deque<FIELD_TYPE_REAL>(ntimeSteps_ * coarseMapping_.size(), 0);
    if (heightLevel().has_value()) {
        levtype_ = "hl";
        level_   = std::to_string(*heightLevel());
    }

    const auto& fields      = requiredFields();
    const size_t fieldCount = fields.size();
    std::ostringstream fieldVec;
    for (size_t i = 0; i + 1 < fieldCount; i++) {
        fieldVec << fields[i] << "/";
    }
    fieldVec << fields[fieldCount - 1];
    fieldNamesStr_ = fieldVec.str();
    if (rampUp_) {
        descriptionUp_ = "Ramp up of " + config.getString("ramp_up_value") + " over " +
                         config.getString("time_window") + "min for param(s) " + fieldNamesStr_;
    }
    if (rampDown_) {
        descriptionDown_ = "Ramp down of " + config.getString("ramp_down_value") + " over " +
                           config.getString("time_window") + "min for param(s) " + fieldNamesStr_;
    }
}

std::vector<ExtremeEvent::DetectionData> RampEvent::detect(plume::data::ModelData& modelData) {
    std::vector<DetectionData> ee_points;
    const auto& fields = requiredFields();
    std::vector<atlas::array::ArrayView<const FIELD_TYPE_REAL, 2>> arrayViews;
    arrayViews.reserve(fields.size());
    const auto height          = heightLevel();
    const std::string levelStr = height.has_value() ? std::to_string(*height) : "";
    for (const auto& fieldName : fields) {
        auto field = height.has_value() ? modelData.getParam<atlas::Field>(fieldName, levelStr)
                                            : modelData.getParam<atlas::Field>(fieldName);
        arrayViews.push_back(atlas::array::make_view<const FIELD_TYPE_REAL, 2>(field));
    }
    auto haloField = height.has_value() ? modelData.getParam<atlas::Field>(fields[0], levelStr)
                                        : modelData.getParam<atlas::Field>(fields[0]);
    auto halo      = atlas::array::make_view<int, 1>(haloField.functionspace().ghost());
    // 1. Slide the values window with current time step values
    previousValues_.erase(previousValues_.begin() + (ntimeSteps_ - 1) * coarseMapping_.size(), previousValues_.end());
    // reverse inserting element to maintain indices
    for (atlas::idx_t idx = coarseMapping_.size() - 1; idx >= 0; idx--) {
        if (halo(idx) > 0) {
            previousValues_.push_front(0);  // Add values with no effect in the halo
        }

        // If several parameters are given, the magnitude is computed as the value, it is the user's responsibility
        // to ensure this quantity makes sense, and the passed fields are 2D (or run detection on level 1)
        if (arrayViews.size() > 1) {
            FIELD_TYPE_REAL squareMag = 0;
            for (const auto& field : arrayViews) {
                squareMag += field(idx, 0) * field(idx, 0);
            }
            previousValues_.push_front(std::sqrt(squareMag));
        }
        else {
            previousValues_.push_front(arrayViews[0](idx, 0));
        }
    }

    if (modelData.getParam<int>("NSTEP") < ntimeSteps_) {  // Fill the current step array but do not run detection yet
        return ee_points;
    }

    // 2. Compute temporal gradient and run detection on the time window
    std::unordered_map<int, FIELD_TYPE_REAL> cellMaxima;
    std::unordered_map<int, FIELD_TYPE_REAL> cellMinima;
    for (atlas::idx_t idx = 0; idx < coarseMapping_.size(); idx++) {
        FIELD_TYPE_REAL gradient =
            previousValues_[idx] - previousValues_[idx + (ntimeSteps_ - 1) * coarseMapping_.size()];
        if (rampUp_) {
            if (cellMaxima.find(coarseMapping_[idx]) == cellMaxima.end()) {
                cellMaxima[coarseMapping_[idx]] = gradient;
            }
            else {
                cellMaxima[coarseMapping_[idx]] = std::max(cellMaxima[coarseMapping_[idx]], gradient);
            }
        }
        if (rampDown_) {
            if (cellMinima.find(coarseMapping_[idx]) == cellMinima.end()) {
                cellMinima[coarseMapping_[idx]] = gradient;
            }
            else {
                cellMinima[coarseMapping_[idx]] = std::min(cellMinima[coarseMapping_[idx]], gradient);
            }
        }
    }

    ee_points.push_back({{}, descriptionUp_, fieldNamesStr_, levtype_, level_});
    ee_points.push_back({{}, descriptionDown_, fieldNamesStr_, levtype_, level_});
    for (const auto& [cell, max] : cellMaxima) {
        if (max > rampUpValue_) {
            ee_points[0].detectedCells.insert(cell);
        }
    }
    for (const auto& [cell, min] : cellMinima) {
        if (min < -rampDownValue_) {
            ee_points[1].detectedCells.insert(cell);
        }
    }

    return ee_points;
}

RampEvent::Registrar RampEvent::registrar;