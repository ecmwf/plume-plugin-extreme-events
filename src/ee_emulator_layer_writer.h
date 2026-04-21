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
#pragma once

#include <filesystem>
#include <string>
#include <vector>

#include "atlas/grid.h"

namespace ExtremeEventPlugin {

struct EEEmulatorLayerEvent {
    std::string eventType;
    std::string description;
    std::string param;
    std::string levtype;
    std::string levelist;
    std::vector<std::vector<atlas::PointLonLat>> polygons;
};

class EEEmulatorLayerWriter {
public:
    static void writeEEPluginLayer(const std::filesystem::path& pluginDir,
                                   const std::string& elapsedTime,
                                   int outputStepNumber,
                                   const std::vector<EEEmulatorLayerEvent>& events);

    // Overload to write to a specific file path (used for per-rank temporary files)
    static void writeEEPluginLayer(const std::filesystem::path& pluginDir,
                                   const std::string& elapsedTime,
                                   int outputStepNumber,
                                   const std::vector<EEEmulatorLayerEvent>& events,
                                   const std::filesystem::path& outputFile);

    // Read events from a JSON layer file (inverse of writeEEPluginLayer)
    static std::vector<EEEmulatorLayerEvent> readEEPluginLayer(const std::filesystem::path& filePath);
};

}  // namespace ExtremeEventPlugin
