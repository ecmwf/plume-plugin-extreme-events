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
#include <sstream>

#include "atlas/array.h"
#include "atlas/field.h"
#include "atlas/functionspace.h"
#include "eckit/exception/Exceptions.h"

#include "extreme_wave.h"

const std::string ExtremeWave::type_ = "extreme_wave";

ExtremeWave::ExtremeWave(const eckit::LocalConfiguration& config, plume::data::ModelData& modelData,
                         const std::vector<int>& coarseMapping) :
    ExtremeEvent(config, type_),
    coarseMapping_(coarseMapping),
    missingValue_(static_cast<FIELD_TYPE_REAL>(config.getInt("missing_value", 9999))) {
    // Validate that the required field is swh
    if (requiredFields_[0] != "swh" || requiredFields_.size() > 1) {
        throw eckit::BadValue("The 'extreme_wave' event requires the significant wave height field", Here());
    }

    // Retrieve the thresholds to run detection on
    for (const auto& instance : config.getSubConfigurations("instances")) {
        if (instance.getDouble("threshold") < 1e-6) {
            throw eckit::BadValue("Extreme wave thresholds must be positive", Here());
        }
        FIELD_TYPE_REAL value = static_cast<FIELD_TYPE_REAL>(instance.getDouble("threshold"));
        std::ostringstream description;
        description << "Extreme waves";
        if (instance.has("description")) {
            description << ": " << instance.getString("description");
        }
        description << " (swh > " << instance.getString("threshold") << "m)";

        thresholds_.push_back({value, description.str()});
    }

    // Ensure there is at least one instance to run detection on
    if (thresholds_.empty()) {
        throw eckit::BadValue("No valid instance found for 'extreme_wave'", Here());
    }

    // Sort thresholds by decreasing values (as high thresholds should automatically trigger small thresholds)
    std::sort(thresholds_.begin(), thresholds_.end(),
              [](const Threshold& a, const Threshold& b) { return a.value > b.value; });
}

std::vector<ExtremeEvent::DetectionData> ExtremeWave::detect(plume::data::ModelData& modelData) {
    std::vector<DetectionData> ee_points;
    for (const auto& threshold : thresholds_) {
        // Wave fields are 2D but level or levtype are not necessary to retrieve them
        ee_points.push_back({{}, threshold.description, "swh", "", ""});
    }

    auto fieldSwh = atlas::array::make_view<const FIELD_TYPE_REAL, 2>(modelData.getAtlasFieldShared("swh"));
    auto halo     = atlas::array::make_view<int, 1>(modelData.getAtlasFieldShared("swh").functionspace().ghost());
    for (atlas::idx_t idx = 0; idx < coarseMapping_.size(); idx++) {
        // Skip the halo and missing values (over land)
        if (halo(idx) > 0 || fieldSwh(idx, 0) == missingValue_) {
            continue;
        }

        bool higherThresholdFired = false;
        for (size_t idx_thsld = 0; idx_thsld < thresholds_.size(); idx_thsld++) {
            if (higherThresholdFired) {
                ee_points[idx_thsld].detectedCells.insert(coarseMapping_[idx]);
                continue;
            }
            if (fieldSwh(idx, 0) > thresholds_[idx_thsld].value) {
                ee_points[idx_thsld].detectedCells.insert(coarseMapping_[idx]);
                higherThresholdFired = true;
            }
        }
    }

    return ee_points;
}

ExtremeWave::Registrar ExtremeWave::registrar;