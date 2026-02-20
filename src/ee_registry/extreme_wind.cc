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
#include <optional>
#include <sstream>

#include "atlas/array.h"
#include "atlas/field.h"
#include "atlas/functionspace.h"
#include "eckit/exception/Exceptions.h"

#include "extreme_wind.h"

const std::string ExtremeWind::type_                           = "extreme_wind";
const std::array<std::string, 4> ExtremeWind::supportedFields_ = {"10u", "10v", "u", "v"};

ExtremeWind::ExtremeWind(const eckit::LocalConfiguration& config, plume::data::ModelData& modelData,
                         const std::vector<int>& coarseMapping) :
    ExtremeEvent(config, type_), coarseMapping_(coarseMapping) {
    // Validate configuration fields
    const auto& fields = requiredFields();
    auto hasField      = [&fields](const std::string& name) {
        return std::find(fields.begin(), fields.end(), name) != fields.end();
    };
    for (const auto& field : fields) {
        if (std::find(supportedFields_.begin(), supportedFields_.end(), field) == supportedFields_.end()) {
            throw eckit::BadValue(
                "The field '" + field +
                    "' is not a supported wind field, please correct 'extreme_wind' event configuration.",
                Here());
        }
    }
    const bool hasUv = hasField("u") || hasField("v");
    const bool has10 = hasField("10u") || hasField("10v");
    if (hasUv && has10) {
        throw eckit::BadValue("Mixing surface and 3D wind fields is not supported", Here());
    }

    // Build detection intervals and descriptions
    auto findField = [&hasField](const std::string& field) { return hasField(field) ? field : ""; };

    const auto height = heightLevel();
    for (const auto& eventConfig : config.getSubConfigurations("instances")) {
        if (height.has_value() && eventConfig.isIntegralList("model_levels")) {
            throw eckit::BadParameter(
                "Model levels are not supported when a height is configured in required parameters", Here());
        }

        std::ostringstream description;
        description << eventConfig.getString("description", "No description provided");
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
                throw eckit::BadParameter("The `model_levels` key can only be used when 3D fields are required",
                                          Here());
            }
            for (const auto& ml : eventConfig.getIntVector("model_levels")) {
                if (ml > modelData.getParam<int>("NFLEVG")) {
                    throw eckit::BadValue("The model has " + std::to_string(modelData.getParam<int>("NFLEVG")) +
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
                intervals_.push_back({eventConfig.getDouble("lower_bound"), eventConfig.getDouble("upper_bound"), 0u,
                                      static_cast<unsigned int>(ml), u, v, description.str() + fieldDesc.str()});
            }
        }
        else {
            if (height.has_value()) {
                std::string u = findField("u");
                std::string v = findField("v");
                if (u.empty() && v.empty()) {
                    throw eckit::BadParameter(
                        "Height-based detection requires 'u' and/or 'v' fields in required parameters", Here());
                }
                fieldDesc.str("");
                fieldDesc << ", height: " << std::to_string(*height) << "m, field";
                if (u.empty() || v.empty()) {
                    fieldDesc << " : '" << u << v << "'))";
                }
                else {
                    fieldDesc << "s : ('" << u << "','" << v << "'))";
                }
                intervals_.push_back({eventConfig.getDouble("lower_bound"), eventConfig.getDouble("upper_bound"),
                                      *height, 0, u, v, description.str() + fieldDesc.str()});
            }
            else {
                // Ensure that surface fields are provided (10m wind)
                std::string u10 = findField("10u");
                std::string v10 = findField("10v");
                if (u10.empty() && v10.empty()) {
                    throw eckit::BadParameter("The `model_levels` key or 2D field(s) is missing in the configuration",
                                              Here());
                }
                fieldDesc.str("");
                fieldDesc << ", field";
                if (u10.empty() || v10.empty()) {
                    fieldDesc << " : '" << u10 << v10 << "'))";
                }
                else {
                    fieldDesc << "s : ('" << u10 << "','" << v10 << "'))";
                }
                intervals_.push_back({eventConfig.getDouble("lower_bound"), eventConfig.getDouble("upper_bound"), 0, 0,
                                      u10, v10, description.str() + fieldDesc.str()});
            }
        }
    }

    // Final validation of interval set
    if (intervals_.empty()) {
        throw eckit::BadValue("No valid instance found for 'extreme_wind', ensure options and required fields align",
                              Here());
    }
}

std::vector<ExtremeEvent::DetectionData> ExtremeWind::detect(plume::data::ModelData& modelData) {
    std::vector<DetectionData> ee_points;
    // Prepare detection outputs
    for (const auto& interval : intervals_) {
        std::string leveltype = interval.modelLevel > 0 ? "ml" : interval.height > 0 ? "hl" : "sfc";
        std::string level     = interval.modelLevel > 0 ? std::to_string(interval.modelLevel)
                                : interval.height > 0   ? std::to_string(interval.height)
                                                        : "1";
        std::string param     = interval.u.empty()   ? interval.v
                                : interval.v.empty() ? interval.u
                                                     : interval.u + "/" + interval.v;
        ee_points.push_back({{}, interval.description, param, leveltype, level});
    }

    // Resolve wind fields once (same for all instances)
    const auto& baseInterval    = intervals_[0];
    const std::string uName     = baseInterval.u;
    const std::string vName     = baseInterval.v;
    const std::string heightStr = baseInterval.height > 0 ? std::to_string(baseInterval.height) : "";
    const bool hasU             = !uName.empty();
    const bool hasV             = !vName.empty();

    std::optional<atlas::Field> uField;
    std::optional<atlas::Field> vField;
    std::optional<atlas::array::ArrayView<const FIELD_TYPE_REAL, 2>> uView;
    std::optional<atlas::array::ArrayView<const FIELD_TYPE_REAL, 2>> vView;

    if (hasU) {
        uField = baseInterval.height > 0 ? modelData.getParam<atlas::Field>(uName, heightStr)
                                         : modelData.getParam<atlas::Field>(uName);
        uView  = atlas::array::make_view<const FIELD_TYPE_REAL, 2>(*uField);
    }
    if (hasV) {
        vField = baseInterval.height > 0 ? modelData.getParam<atlas::Field>(vName, heightStr)
                                         : modelData.getParam<atlas::Field>(vName);
        vView  = atlas::array::make_view<const FIELD_TYPE_REAL, 2>(*vField);
    }

    const atlas::Field& baseField = hasU ? *uField : *vField;
    auto halo                     = atlas::array::make_view<int, 1>(baseField.functionspace().ghost());
    int nbOfValues                = baseField.shape(0);

    // Run detection across grid points
    for (atlas::idx_t idx = 0; idx < nbOfValues; idx++) {
        // Skip the halo
        if (halo(idx) > 0) {
            continue;
        }

        for (size_t idx_int = 0; idx_int < intervals_.size(); idx_int++) {
            // Skip detection if the coarse cell has already fired
            if (ee_points[idx_int].detectedCells.find(coarseMapping_[idx]) != ee_points[idx_int].detectedCells.end()) {
                continue;
            }
            // If it is not a surface field we remove 1 from the index as model levels start at 1 and not 0
            int levelIdx                  = intervals_[idx_int].modelLevel > 0 ? intervals_[idx_int].modelLevel - 1 : 0;
            FIELD_TYPE_REAL valU          = hasU ? (*uView)(idx, levelIdx) : 0;
            FIELD_TYPE_REAL valV          = hasV ? (*vView)(idx, levelIdx) : 0;
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
