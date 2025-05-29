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
#include <sstream>
#include <unordered_map>

#include "atlas/array.h"
#include "atlas/field.h"
#include "atlas/functionspace.h"
#include "eckit/exception/Exceptions.h"

#include "extreme_wind.h"

const std::string ExtremeWind::type_                           = "extreme_wind";
const std::array<std::string, 6> ExtremeWind::supportedFields_ = {"100u", "100v", "10u", "10v", "u", "v"};

ExtremeWind::ExtremeWind(const eckit::LocalConfiguration& config, plume::data::ModelData& modelData,
                         const std::vector<int>& coarseMapping) :
    ExtremeEvent(config, type_), coarseMapping_(coarseMapping) {
    // Validate that all required fields are named like wind fields
    for (const auto& field : requiredFields_) {
        if (std::find(supportedFields_.begin(), supportedFields_.end(), field) == supportedFields_.end()) {
            throw eckit::BadValue(
                "The field '" + field +
                    "' is not a supported wind field, please correct 'extreme_wind' event configruation.",
                Here());
        }
    }

    // Retrieve the wind intervals to run detection on and their description if applicable
    auto findField = [this](const std::string& field) {
        return std::find(requiredFields_.begin(), requiredFields_.end(), field) == requiredFields_.end() ? "" : field;
    };

    for (const auto& eventConfig : config.getSubConfigurations("instances")) {
        if (eventConfig.isIntegralList("heights") && !eventConfig.getIntVector("heights").empty()) {
            throw eckit::BadParameter(
                "Detecting extreme wind at given heights is not currently supported, please remove from config.");
        }

        std::ostringstream description;
        description << eventConfig.getString("description");
        if (eventConfig.getDouble("lower_bound") > eventConfig.getDouble("upper_bound")) {
            description << " (threshold : " << std::to_string(eventConfig.getDouble("lower_bound")) << " m/s)";
        }
        else {
            description << " (lower bound : " << std::to_string(eventConfig.getDouble("lower_bound"))
                        << " m/s, upper bound : " << std::to_string(eventConfig.getDouble("upper_bound")) << " m/s";
        }

        std::ostringstream fieldDesc;
        if (eventConfig.isIntegralList("model_levels")) {
            // Ensure that `u` or `v` fields are provided
            std::string u = findField("u");
            std::string v = findField("v");
            if (u.empty() && v.empty()) {
                throw eckit::BadParameter(
                    "The `model_levels` key can only be used when non surface fields are required", Here());
            }
            for (const auto& ml : eventConfig.getIntVector("model_levels")) {
                if (ml > modelData.getInt("NFLEVG")) {
                    throw eckit::BadValue("The model has " + std::to_string(modelData.getInt("NFLEVG")) +
                                              " vertical levels, please adjust the config.",
                                          Here());
                }
                fieldDesc.str("");
                fieldDesc << ", level: " << std::to_string(ml) + ", field";
                if (u.empty() || v.empty()) {
                    fieldDesc << " : '" << u << v << "'))";
                }
                else {
                    fieldDesc << "s : ('u','v'))";
                }
                intervals_.push_back({eventConfig.getDouble("lower_bound"), eventConfig.getDouble("upper_bound"), -1,
                                      ml, u, v, description.str() + fieldDesc.str()});
            }
        }
        else {
            // Ensure that surface fields are provided
            std::string u10  = findField("10u");
            std::string v10  = findField("10v");
            std::string u100 = findField("100u");
            std::string v100 = findField("100v");
            if (u10.empty() && v10.empty() && u100.empty() && v100.empty()) {
                throw eckit::BadParameter("The `model_levels` key or surface field(s) is missing in the configuration",
                                          Here());
            }

            std::vector<std::pair<std::string, std::string>> sfcWindCpnts = {{u10, v10}, {u100, v100}};
            for (const auto& cpnt : sfcWindCpnts) {
                if (cpnt.first.empty() && cpnt.second.empty()) {
                    continue;
                }
                fieldDesc.str("");
                fieldDesc << ", field";
                if (cpnt.first.empty() || cpnt.second.empty()) {
                    fieldDesc << " : '" << cpnt.first << cpnt.second << "'))";
                }
                else {
                    fieldDesc << "s : ('" << cpnt.first << "','" << cpnt.second << "'))";
                }
                intervals_.push_back({eventConfig.getDouble("lower_bound"), eventConfig.getDouble("upper_bound"), -1, 0,
                                      cpnt.first, cpnt.second, description.str() + fieldDesc.str()});
            }
        }
    }

    // Ensure there is at least one instance to run detection on
    if (intervals_.empty()) {
        throw eckit::BadValue("No valid instance found for 'extreme_wind', ensure options and required fields align",
                              Here());
    }
}

std::vector<ExtremeEvent::DetectionData> ExtremeWind::detect(plume::data::ModelData& modelData) {
    std::vector<DetectionData> ee_points;
    for (const auto& interval : intervals_) {
        std::string level = interval.modelLevel > 0 ? "ml" : "sfc";
        std::string param = interval.u.empty()   ? interval.v
                            : interval.v.empty() ? interval.u
                                                 : interval.u + "/" + interval.v;
        ee_points.push_back({{}, interval.description, param, level, std::to_string(interval.modelLevel)});
    }
    std::unordered_map<std::string, std::unique_ptr<atlas::array::ArrayView<const FIELD_TYPE_REAL, 2>>> windFields;
    auto halo =
        atlas::array::make_view<int, 1>(modelData.getAtlasFieldShared(requiredFields_[0]).functionspace().ghost());
    int nbOfValues = modelData.getAtlasFieldShared(requiredFields_[0]).shape(0);
    for (const auto& windField : requiredFields_) {
        windFields[windField] = std::make_unique<atlas::array::ArrayView<const FIELD_TYPE_REAL, 2>>(
            atlas::array::make_view<const FIELD_TYPE_REAL, 2>(modelData.getAtlasFieldShared(windField)));
    }

    for (atlas::idx_t idx = 0; idx < nbOfValues; idx++) {
        // Skip the halo
        if (halo(idx) > 0) {
            continue;
        }

        for (size_t idx_int = 0; idx_int < intervals_.size(); idx_int++) {
            // Skip detection if the coarse cell has already fired
            // TODO: is this condition worth adding because it adds an operation for each non firing point ?
            if (ee_points[idx_int].detectedCells.find(coarseMapping_[idx]) != ee_points[idx_int].detectedCells.end()) {
                continue;
            }
            // If it is not a surface field we remove 1 from the index as model levels start at 1 and not 0
            int levelIdx = intervals_[idx_int].modelLevel > 0 ? intervals_[idx_int].modelLevel - 1 : 0;
            FIELD_TYPE_REAL valU =
                intervals_[idx_int].u.empty() ? 0 : (*windFields[intervals_[idx_int].u])(idx, levelIdx);
            FIELD_TYPE_REAL valV =
                intervals_[idx_int].v.empty() ? 0 : (*windFields[intervals_[idx_int].v])(idx, levelIdx);
            FIELD_TYPE_REAL windMagnitude = std::sqrt(valU * valU + valV * valV);
            if (windMagnitude < intervals_[idx_int].lBound) {
                continue;
            }
            if (intervals_[idx_int].lBound > intervals_[idx_int].uBound || windMagnitude < intervals_[idx_int].uBound) {
                // /!\ if the upper bound is lower than the lower bound then we check
                // only if the wind exceeds the lower bound
                // if the upper bound is higher, then we check for belonging
                ee_points[idx_int].detectedCells.insert(coarseMapping_[idx]);
            }
        }
    }
    return ee_points;
}

ExtremeWind::Registrar ExtremeWind::registrar;
