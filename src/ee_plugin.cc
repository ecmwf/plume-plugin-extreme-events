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
#include <string>
#include <fstream>


#include "atlas/field/Field.h"
#include "atlas/parallel/mpi/mpi.h"

#include "ee_plugin.h"
#include "healpix_utils.h"

// Can be swapped to use another point to Extreme Event cell mapping
using namespace HEALPixUtils;

namespace ExtremeEventPlugin {

EEPluginCore::EEPluginCore(const eckit::Configuration& conf) : PluginCore(conf) {
    healpixRes_                = conf.getInt("healpix_res", 2);
    enableNotification_        = conf.getBool("enable_notification", false);
    enableLog_                 = conf.getBool("enable_log", false);
    runEvery_                  = conf.getInt("run_every", 1);
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
        // Since we are using Plume 0.2 all the params have already been negotiated
        /*for (const auto& param : ee.getSubConfigurations("required_params")) {
            if (!modelData().hasParameter(param.getString("name"))) {
                hasRequiredParams = false;
                break;
            }
        }*/
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

    // Run only every N steps if specified
    int step = modelData().getInt("NSTEP");
    if ( step % runEvery_ != 0 ) {
        return;
    }

    eckit::Log::info() << "Running extreme event detection plugin at step " << step << std::endl;

    // if logging is enabled, open a log file with proc number and step number
    std::ofstream logFile;

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
                std::ostringstream message;
                message << "[EE Plume Plugin] >>> event: " << results[idx].description << "; step: " << elapsedTime
                        << "; param: " << results[idx].param << "; levtype: " << results[idx].levtype
                        << "; levelist: " << results[idx].levelist;
                std::cout << message.str() << std::endl;
            }

            // Write payload and polygons to log file
            if (enableLog_) {

                if (!logFile.is_open()) {

                    std::stringstream ss;
                    ss << "ee_plugin_proc-" << atlas::mpi::rank() << "_step-" << modelData().getInt("NSTEP") << ".log";
                    std::string logFileName{ss.str()};

                    // check if the envaronment variable PLUME_PLUGINS_OUTPUT_DIR is set,
                    // is so, prepend it to the filename
                    const char* outputDir = std::getenv("PLUME_PLUGINS_OUTPUT_DIR");
                    if (outputDir) {
                        logFileName = std::string(outputDir) + "/" + logFileName;
                    }

                    logFile.open(logFileName, std::ios::out);
                    if (!logFile.is_open()) {
                        eckit::Log::error() << "Could not open log file: " << logFileName << std::endl;
                        enableLog_ = false;  // disable logging if we cannot open the file
                    }
                }

                // Overall message that each process writes to file at each step
                std::string proc_step_logstring;

                // Common part of the message
                std::ostringstream message;
                message << "[EE Plume Plugin] >>> event: " << results[idx].description << "; step: " << elapsedTime
                        << "; param: " << results[idx].param << "; levtype: " << results[idx].levtype
                        << "; levelist: " << results[idx].levelist
                        << "; polygons: [";

                auto write_polygon = [](std::ostringstream& ss, const std::vector<atlas::PointLonLat>& polygon) {
                    ss << "(";
                    for (size_t i = 0; i < polygon.size() - 1; ++i) {
                        ss << polygon[i].lat() << "," << polygon[i].lon() << ",";
                    }
                    ss << polygon.back().lat() << "," << polygon.back().lon();
                    ss << ")";
                };

                // Write polygons
                std::ostringstream polygonSS;
                for (size_t iPol=0; iPol<ee_polygon_points.size() - 1; ++iPol) {
                    auto polygon = ee_polygon_points[iPol];
                    write_polygon(polygonSS, polygon);
                    polygonSS << ",";
                }

                // Last polygon without trailing comma
                auto polygon = ee_polygon_points.back();
                write_polygon(polygonSS, polygon);

                message << polygonSS.str() << "]";

                // write message to file
                proc_step_logstring = message.str();
                logFile << proc_step_logstring << std::endl;
            }
        }
    }

    // close log file, if opened
    if (enableLog_) {
        if (logFile.is_open()) {
            logFile.close();
        }
    }

}

void EEPluginCore::setHEALPixMapping() {
    // TODO: Should this plugin handle multiple functionspaces if fields passed are not all on the same mesh?
    // Retrieve the function space from the model data
    // Since the plugin has negotiated hardcoded fields to comply with Plume 0.2 we can use one of these
    auto fs = modelData().getAtlasFieldShared("100u").functionspace();
    mapLonLatToHEALPixCell(healpixRes_, fs, Point2HPcell_, HPcell2polygon_);
}

std::string EEPluginCore::modelStepStr() {
    if (modelData().getInt("NSTEP") == 0) {
        return "0s";
    }
    int seconds = static_cast<int>(std::round(modelData().getInt("NSTEP") * modelData().getDouble("TSTEP")));
    // Sub-hourly supported time units (except for seconds)
    const std::vector<std::pair<int, std::string>> timeUnits = {{86400, "d"}, {3600, "h"}, {60, "m"}};
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