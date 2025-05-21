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
#include <sstream>

#include "atlas/field/Field.h"

#include "ee_plugin.h"
#include "healpix_utils.h"

// Can be swapped to use another point to Extreme Event cell mapping
using namespace HEALPixUtils;

namespace ExtremeEventPlugin {

EEPluginCore::EEPluginCore(const eckit::Configuration& conf) : PluginCore(conf) {
    healpixRes_         = conf.getInt("healpix_res", 2);
    enableNotification_ = conf.getBool("enable_notification", false);
    if (enableNotification_) {
        notificationHandler_ = AvisoNotificationHandler(conf.getString("aviso_url"), conf.getString("notify_endpoint"));
    }

    extremeEventConfig_ = conf.getSubConfigurations("events");
}

void EEPluginCore::setup() {
    // Healpix - grid points & polygon mapping matrix
    setHEALPixMapping();
    // initialize extremeEventList from config
    eckit::Log::info() << "Extreme event detection Plume plugin loading events... ";
    for (auto& ee : extremeEventConfig_) {
        if (!ee.getBool("enabled", true)) {
            continue;
        }
        // Load only the extreme events that require offered parameters
        bool hasRequiredParams = true;
        for (const auto& param : ee.getSubConfigurations("required_params")) {
            if (!modelData().hasParameter(param.getString("name"))) {
                hasRequiredParams = false;
                break;
            }
        }
        if (hasRequiredParams) {
            extremeEvents_.push_back(ExtremeEventRegistry::instance().createEvent(ee, modelData(), Point2HPcell_));
            eckit::Log::info() << ee.getString("name") << " ";
        }
    }
    if (extremeEvents_.empty()) {  // This should not happen if the negotiation is done properly
        eckit::Log::error() << "No extreme events loaded, the EE plugin will error, check configuration" << std::endl;
    }
    eckit::Log::info() << std::endl;
}

void EEPluginCore::run() {
    // Determine the elapsed time in the simulation in minutes
    std::string elapsedTime = modelStepStr();
    for (auto& ee : extremeEvents_) {
        // Run the detection for each extreme event suite
        auto results = ee->detect(modelData());
        for (size_t idx = 0; idx < results.size(); ++idx) {
            if (results[idx].detectedCells.empty()) {
                // No actual points were detected for that instance of the event
                continue;
            }
            auto ee_polygon_points = cellToPolygons(results[idx].detectedCells, HPcell2polygon_);
            if (enableNotification_) {
                // Send notification for each polygon individually if enabled
                for (auto& polygon : ee_polygon_points) {
                    // TODO: move the payload building responsibility to the aviso handler after payload is agreed on
                    std::ostringstream payload;
                    payload << "{\"step\":\"" << elapsedTime << "\",\"description\":\"" << results[idx].description
                            << "\",\"param\":\"" << results[idx].param << "\",\"levtype\":\"" << results[idx].levtype
                            << "\",\"levelist\":\"" << results[idx].levelist << "\"}";
                    int status = notificationHandler_.send(payload.str(), polygon);
                    if (status != 200 && status != 999) {
                        eckit::Log::error() << "Could not send Aviso notification, error code " << status << std::endl;
                    }
                }
            }
            else {
                // TODO what do we do with the results of notifications are disabled ?
            }
        }
    }
}

void EEPluginCore::setHEALPixMapping() {
    // TODO: Should this plugin handle multiple functionspaces if fields passed are not all on the same mesh?
    // Retrieve the function space from the model data
    auto fs = modelData().getAtlasFieldShared(modelData().listAvailableParameters("ATLAS_FIELD")[0]).functionspace();
    mapLonLatToHEALPixCell(healpixRes_, fs, Point2HPcell_, HPcell2polygon_);
}

std::string EEPluginCore::modelStepStr() {
    if (modelData().getInt("NSTEP") == 0) {
        return "0s";
    }
    int seconds = static_cast<int>(std::round(modelData().getInt("NSTEP") * modelData().getDouble("TSTEP")));
    // Sub-hourly supported time units (except for seconds)
    std::vector<std::pair<int, std::string>> timeUnits = {{86400, "d"}, {3600, "h"}, {60, "m"}};
    for (const auto& unit : timeUnits) {
        if (seconds % unit.first == 0) {
            int quotient = seconds / unit.first;
            return std::to_string(quotient) + unit.second;
        }
    }
    return std::to_string(seconds) + "s";
}

// ------------------------------------------------------

// ------------------------------------------------------

EEPlugin::EEPlugin() : Plugin("EEPlugin"){};

const EEPlugin& EEPlugin::instance() {
    static EEPlugin instance;
    return instance;
}
// ------------------------------------------------------
}  // namespace ExtremeEventPlugin