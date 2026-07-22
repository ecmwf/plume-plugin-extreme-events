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

#include "atlas/field.h"
#include "atlas/functionspace.h"
#include "eckit/exception/Exceptions.h"

#include "wind_drought.h"

const std::string WindDrought::type_ = "wind_drought";

WindDrought::WindDrought(const eckit::LocalConfiguration& config, plume::data::ModelDataView& modelData,
                         const std::vector<int>& coarseMapping) :
    ExtremeEvent(config, type_), coarseMapping_(coarseMapping) {

    const auto& fields = requiredFields();
    const bool hasU    = std::find(fields.begin(), fields.end(), "u") != fields.end();
    const bool hasV    = std::find(fields.begin(), fields.end(), "v") != fields.end();
    if (!hasU || !hasV) {
        throw eckit::BadValue("Wind drought requires wind component fields 'u' and 'v'.", Here());
    }

    windSpeedCutout_ = static_cast<FIELD_TYPE_REAL>(config.getDouble("wind_speed_cutout"));
    if (windSpeedCutout_ < 0) {
        throw eckit::BadValue("The cutout wind magnitude for the wind drought event should be greater than 0", Here());
    }

    windDroughtSteps_.assign(coarseMapping_.size(), 0);
    timeWindow_                   = config.getUnsigned("time_window");
    ntimeSteps_                   = std::ceil(timeWindow_ * 60 / modelData.getParam<double>("TSTEP"));
    const bool useHeight          = heightLevel().has_value();
    const unsigned int modelLevel = config.getUnsigned("model_level", 1);
    levtype_                      = useHeight ? "hl" : "ml";
    level_                        = useHeight ? std::to_string(*heightLevel()) : std::to_string(modelLevel);
    if (!useHeight && modelLevel > modelData.getParam<int>("NFLEVG")) {
        throw eckit::BadValue(
            "The specified level for the wind drought event is higher than the number of levels in the model", Here());
    }
    const std::string levelStr = useHeight ? level_ + "m" : "ml " + level_;
    description_ = "Wind drought (wind speed remains below " + config.getString("wind_speed_cutout") + "m/s for over " +
                   config.getString("time_window") + "minutes at " + levelStr + ")";
}

std::vector<ExtremeEvent::DetectionData> WindDrought::detect(plume::data::ModelDataView& modelData) {
    std::vector<DetectionData> ee_points;
    const bool useHeight = levtype_ == "hl";

    auto uField = useHeight ? modelData.getParam<atlas::Field>("u", level_) : modelData.getParam<atlas::Field>("u");
    auto vField = useHeight ? modelData.getParam<atlas::Field>("v", level_) : modelData.getParam<atlas::Field>("v");
    auto arrayU = atlas::array::make_view<const FIELD_TYPE_REAL, 2>(uField);
    auto arrayV = atlas::array::make_view<const FIELD_TYPE_REAL, 2>(vField);
    auto halo   = atlas::array::make_view<int, 1>(uField.functionspace().ghost());
    const int levelIdx = useHeight ? 0 : std::stoi(level_) - 1;

    // 1. Compute spatial wind speed average & update count for each cell
    for (atlas::idx_t idx = 0; idx < coarseMapping_.size(); idx++) {
        if (halo(idx) > 0) {
            continue;  // Halo counters stay at 0 so they have no effect
        }

        FIELD_TYPE_REAL valU          = arrayU(idx, levelIdx);
        FIELD_TYPE_REAL valV          = arrayV(idx, levelIdx);
        FIELD_TYPE_REAL windMagnitude = std::sqrt(valU * valU + valV * valV);
        if (windMagnitude < windSpeedCutout_) {
            ++windDroughtSteps_[idx];
        }
        else {
            windDroughtSteps_[idx] = 0;  // This resets the counter if the wind finally exceeds the threshold
        }
    }

    // 2. Detect the cells that have not exceeded the wind threshold during the time window
    ee_points.push_back({{}, description_, "u/v", levtype_, level_});
    for (atlas::idx_t idx = 0; idx < coarseMapping_.size(); idx++) {
        if (ntimeSteps_ < windDroughtSteps_[idx]) {
            ee_points[0].detectedCells.insert(coarseMapping_[idx]);
        }
    }
    return ee_points;
}

WindDrought::Registrar WindDrought::registrar;