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
#include <cstdlib>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "atlas/field/Field.h"
#include "eckit/mpi/Comm.h"

#include "ee_emulator_layer_writer.h"
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
        const auto& requiredParams = ee.getSubConfigurations("required_params");
        const bool hasRequiredParams = std::all_of(
            requiredParams.begin(),
            requiredParams.end(),
            [this](const eckit::Configuration& param) {
                // Any other derivation than height levels is currently not supported
                const std::string heightStr = param.getString("height", "");
                const std::string name = param.getString("name");
                return heightStr.empty() ? modelData().hasParameter(name)
                                         : modelData().hasParameter(name, heightStr);
            });
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
#ifdef EE_PLUGIN_EMULATOR_LAYER_OUTPUT
    std::vector<EEEmulatorLayerEvent> emulatorLayerEvents;
#endif

    for (auto& ee : extremeEvents_) {
        // Determine whether or not the event should run
        const auto& requiredParams = ee->requiredParams();
        const auto& requiredFields = ee->requiredFields();
        const auto& heightLevel = ee->heightLevel();
        const bool paramUpdated = std::any_of(requiredParams.begin(), requiredParams.end(),
                                              [this](const std::string& name) { return modelData().isUpdated(name); });
        const bool fieldUpdated = std::any_of(
            requiredFields.begin(), requiredFields.end(),
            [this, &heightLevel](const std::string& name) {
                return heightLevel.has_value() ? modelData().isUpdated(name, std::to_string(*heightLevel))
                                               : modelData().isUpdated(name);
            });
        if (!paramUpdated && !fieldUpdated) {
            continue;  // A single updated field or param is enough to allow the detection
        }
        // Run the detection for each extreme event suite
        auto results = ee->detect(modelData());
        for (size_t idx = 0; idx < results.size(); ++idx) {
            if (results[idx].detectedCells.empty()) {
                // No actual points were detected for that instance of the event
                continue;
            }
            auto ee_polygon_points = cellToPolygons(results[idx].detectedCells, HPcell2polygon_);

#ifdef EE_PLUGIN_EMULATOR_LAYER_OUTPUT
            if (!ee_polygon_points.empty()) {
                emulatorLayerEvents.push_back(EEEmulatorLayerEvent{
                    ee->type(),
                    results[idx].description,
                    results[idx].param,
                    results[idx].levtype,
                    results[idx].levelist,
                    ee_polygon_points,
                });
            }
#endif

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

#ifdef EE_PLUGIN_EMULATOR_LAYER_OUTPUT
    const int stepNumber = modelData().getParam<int>("NSTEP");
    maybeWriteEmulatorLayer(elapsedTime, stepNumber, emulatorLayerEvents);
#endif
}

#ifdef EE_PLUGIN_EMULATOR_LAYER_OUTPUT
void EEPluginCore::maybeWriteEmulatorLayer(const std::string& elapsedTime, int stepNumber,
                                           const std::vector<EEEmulatorLayerEvent>& events) {

    const char* emulatorMode = std::getenv("PLUME_EMULATOR_MODE");
    if (emulatorMode == nullptr || std::string(emulatorMode) != "1") {
        return;
    }

    const char* runTmpDir = std::getenv("PLUME_RUN_TMPDIR");
    if (runTmpDir == nullptr || std::string(runTmpDir).empty()) {
        eckit::Log::warning() << "EEPlugin emulator layer output skipped: PLUME_RUN_TMPDIR unset"
                              << std::endl;
        return;
    }

    namespace fs = std::filesystem;
    const fs::path pluginDir = fs::path(runTmpDir) / "plugin_layers" / "EEPlugin";

    // Each rank writes its events to a temporary per-rank file
    if (!events.empty()) {
        const fs::path rankEventFile = pluginDir / ("rank_" + std::to_string(eckit::mpi::comm().rank()) + 
                                                    "_step_" + std::to_string(stepNumber) + ".tmp");
        try {
            EEEmulatorLayerWriter::writeEEPluginLayer(pluginDir, elapsedTime, stepNumber, events, rankEventFile);
        }
        catch (const std::exception& ex) {
            eckit::Log::warning() << "EEPlugin failed to write rank event file: " << ex.what() << std::endl;
        }
    }

    // Synchronize: ensure all ranks finished writing
    eckit::mpi::comm().barrier();

    // Only rank 0 aggregates and writes final consolidated output
    if (eckit::mpi::comm().rank() == 0) {
        std::vector<EEEmulatorLayerEvent> aggregatedEvents;

        // Iterate through all rank files and aggregate events
        for (int r = 0; r < eckit::mpi::comm().size(); ++r) {
            const fs::path rankFile = pluginDir / ("rank_" + std::to_string(r) + 
                                                  "_step_" + std::to_string(stepNumber) + ".tmp");
            if (fs::exists(rankFile)) {
                try {
                    auto rankEvents = EEEmulatorLayerWriter::readEEPluginLayer(rankFile);
                    aggregatedEvents.insert(aggregatedEvents.end(), rankEvents.begin(), rankEvents.end());
                }
                catch (const std::exception& ex) {
                    eckit::Log::warning() << "EEPlugin failed to read rank event file " << rankFile.string() 
                                         << ": " << ex.what() << std::endl;
                }
            }
        }

        // Write consolidated output
        if (!aggregatedEvents.empty()) {
            try {
                EEEmulatorLayerWriter::writeEEPluginLayer(pluginDir, elapsedTime, stepNumber, aggregatedEvents);
                eckit::Log::info() << "EEPlugin wrote aggregated emulator event layer step " << stepNumber
                                  << " to " << pluginDir.string() << std::endl;
            }
            catch (const std::exception& ex) {
                eckit::Log::warning() << "EEPlugin failed to write aggregated emulator event layer: " << ex.what() << std::endl;
            }
        }

        // Clean up temporary per-rank files
        for (int r = 0; r < eckit::mpi::comm().size(); ++r) {
            const fs::path rankFile = pluginDir / ("rank_" + std::to_string(r) + 
                                                  "_step_" + std::to_string(stepNumber) + ".tmp");
            if (fs::exists(rankFile)) {
                try {
                    fs::remove(rankFile);
                }
                catch (const std::exception& ex) {
                    eckit::Log::warning() << "EEPlugin failed to remove temporary file " << rankFile.string() 
                                         << ": " << ex.what() << std::endl;
                }
            }
        }
    }

    // Synchronize: ensure rank 0 finished cleanup before any rank continues
    eckit::mpi::comm().barrier();
}
#endif

void EEPluginCore::setHEALPixMapping() {
    // TODO: Should this plugin handle multiple functionspaces if fields passed are not all on the same mesh?
    // Retrieve the function space from the model data
    auto fs = modelData().getParam<atlas::Field>(modelData().listAvailableParameters("ATLAS_FIELD")[0]).functionspace();
    mapLonLatToHEALPixCell(healpixRes_, fs, Point2HPcell_, HPcell2polygon_);
}

std::string EEPluginCore::modelStepStr() {
    if (modelData().getParam<int>("NSTEP") == 0) {
        return "0s";
    }
    int seconds = static_cast<int>(std::round(modelData().getParam<int>("NSTEP") * modelData().getParam<double>("TSTEP")));
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

EEPlugin::EEPlugin() : Plugin("EEPlugin") {};

const EEPlugin& EEPlugin::instance() {
    static EEPlugin instance;
    return instance;
}
// ------------------------------------------------------
}  // namespace ExtremeEventPlugin