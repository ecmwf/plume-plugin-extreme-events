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

#include "ee_emulator_layer_writer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>

namespace ExtremeEventPlugin {
namespace {

std::string jsonEscape(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (const char c : input) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

struct EventColor {
    const char* stroke;
    const char* fill;
};

EventColor colorForEventType(const std::string& eventType) {
    static const std::array<EventColor, 8> palette = {{
        {"#b40426", "rgba(180,4,38,0.20)"},
        {"#000734", "rgba(59,76,192,0.20)"},
        {"#1f9e89", "rgba(31,158,137,0.20)"},
        {"#ff7f0e", "rgba(255,127,14,0.20)"},
        {"#17becf", "rgba(23,190,207,0.20)"},
        {"#2ca02c", "rgba(44,160,44,0.20)"},
        {"#d62728", "rgba(214,39,40,0.20)"},
        {"#9467bd", "rgba(148,103,189,0.20)"},
    }};

    const std::size_t idx = std::hash<std::string>{}(eventType) % palette.size();
    return palette[idx];
}

std::vector<atlas::PointLonLat> ensureClosedPolygon(const std::vector<atlas::PointLonLat>& polygon) {
    if (polygon.size() < 3) {
        return {};
    }
    std::vector<atlas::PointLonLat> closed = polygon;
    const auto& first = closed.front();
    const auto& last = closed.back();
    if (std::fabs(first.lon() - last.lon()) > 1e-9 || std::fabs(first.lat() - last.lat()) > 1e-9) {
        closed.push_back(first);
    }
    return closed;
}

}  // namespace

void EEEmulatorLayerWriter::writeEEPluginLayer(const std::filesystem::path& pluginDir,
                                               const std::string& elapsedTime,
                                               int outputStepNumber,
                                               const std::vector<EEEmulatorLayerEvent>& events) {
    std::filesystem::create_directories(pluginDir);

    const std::filesystem::path stepLayerPath = pluginDir / ("layer_step_" + std::to_string(outputStepNumber) + ".json");
    std::ofstream layerFile(stepLayerPath, std::ios::out | std::ios::trunc);
    if (!layerFile.good()) {
        throw std::runtime_error("Could not open step layer file for writing: " + stepLayerPath.string());
    }

    std::ostringstream regions;
    bool firstRegion = true;
    std::unordered_map<std::string, int> eventTypeCounter;

    for (const auto& event : events) {
        const EventColor color = colorForEventType(event.eventType);
        int polygonOrdinal = 0;

        for (const auto& rawPolygon : event.polygons) {
            const auto polygon = ensureClosedPolygon(rawPolygon);
            if (polygon.empty()) {
                continue;
            }

            if (!firstRegion) {
                regions << ",\n";
            }
            firstRegion = false;

            const int eventIdx = ++eventTypeCounter[event.eventType];
            ++polygonOrdinal;
            const std::string regionName = event.eventType + "_" + std::to_string(eventIdx) + "_poly_" +
                                           std::to_string(polygonOrdinal);

            regions
                << "    {\n"
                << "      \"name\": \"" << jsonEscape(regionName) << "\",\n"
                << "      \"event_type\": \"" << jsonEscape(event.eventType) << "\",\n"
                << "      \"description\": \"" << jsonEscape(event.description) << "\",\n"
                << "      \"param\": \"" << jsonEscape(event.param) << "\",\n"
                << "      \"levtype\": \"" << jsonEscape(event.levtype) << "\",\n"
                << "      \"levelist\": \"" << jsonEscape(event.levelist) << "\",\n"
                << "      \"coordinates\": [";

            for (std::size_t i = 0; i < polygon.size(); ++i) {
                if (i > 0) {
                    regions << ", ";
                }
                const double lon = polygon[i].lon();
                const double lat = polygon[i].lat();
                regions << "[" << std::setprecision(10) << lon << ", " << lat << "]";
            }

            regions
                << "],\n"
                << "      \"style\": {\"stroke_color\": \"" << color.stroke
                << "\", \"fill_color\": \"" << color.fill << "\", \"stroke_width\": 2}\n"
                << "    }";
        }
    }

    layerFile
        << "{\n"
        << "  \"type\": \"polygon_boundary\",\n"
        << "  \"step\": \"" << jsonEscape(elapsedTime) << "\",\n"
        << "  \"step_number\": " << outputStepNumber << ",\n"
        << "  \"regions\": [\n"
        << regions.str() << "\n"
        << "  ]\n"
        << "}\n";

    std::ofstream metadataFile(pluginDir / "metadata.json", std::ios::out | std::ios::trunc);
    if (!metadataFile.good()) {
        throw std::runtime_error("Could not open metadata file for writing");
    }
    metadataFile
        << "{\n"
        << "  \"name\": \"EEPlugin\",\n"
        << "  \"type\": \"polygon_boundary\",\n"
        << "  \"description\": \"Extreme-event polygons for emulator mode\",\n"
        << "  \"schema_version\": \"1.2\",\n"
        << "  \"step_layer_pattern\": \"layer_step_<step>.json\",\n"
        << "  \"latest_step\": " << outputStepNumber << "\n"
        << "}\n";
}

void EEEmulatorLayerWriter::writeEEPluginLayer(const std::filesystem::path& pluginDir,
                                               const std::string& elapsedTime,
                                               int outputStepNumber,
                                               const std::vector<EEEmulatorLayerEvent>& events,
                                               const std::filesystem::path& outputFile) {
    // Ensure parent directory exists
    std::filesystem::create_directories(outputFile.parent_path());

    std::ofstream layerFile(outputFile, std::ios::out | std::ios::trunc);
    if (!layerFile.good()) {
        throw std::runtime_error("Could not open output file for writing: " + outputFile.string());
    }

    std::ostringstream regions;
    bool firstRegion = true;
    std::unordered_map<std::string, int> eventTypeCounter;

    for (const auto& event : events) {
        const EventColor color = colorForEventType(event.eventType);
        int polygonOrdinal = 0;

        for (const auto& rawPolygon : event.polygons) {
            const auto polygon = ensureClosedPolygon(rawPolygon);
            if (polygon.empty()) {
                continue;
            }

            if (!firstRegion) {
                regions << ",\n";
            }
            firstRegion = false;

            const int eventIdx = ++eventTypeCounter[event.eventType];
            ++polygonOrdinal;
            const std::string regionName = event.eventType + "_" + std::to_string(eventIdx) + "_poly_" +
                                           std::to_string(polygonOrdinal);

            regions
                << "    {\n"
                << "      \"name\": \"" << jsonEscape(regionName) << "\",\n"
                << "      \"event_type\": \"" << jsonEscape(event.eventType) << "\",\n"
                << "      \"description\": \"" << jsonEscape(event.description) << "\",\n"
                << "      \"param\": \"" << jsonEscape(event.param) << "\",\n"
                << "      \"levtype\": \"" << jsonEscape(event.levtype) << "\",\n"
                << "      \"levelist\": \"" << jsonEscape(event.levelist) << "\",\n"
                << "      \"coordinates\": [";

            for (std::size_t i = 0; i < polygon.size(); ++i) {
                if (i > 0) {
                    regions << ", ";
                }
                const double lon = polygon[i].lon();
                const double lat = polygon[i].lat();
                regions << "[" << std::setprecision(10) << lon << ", " << lat << "]";
            }

            regions
                << "],\n"
                << "      \"style\": {\"stroke_color\": \"" << color.stroke
                << "\", \"fill_color\": \"" << color.fill << "\", \"stroke_width\": 2}\n"
                << "    }";
        }
    }

    layerFile
        << "{\n"
        << "  \"type\": \"polygon_boundary\",\n"
        << "  \"step\": \"" << jsonEscape(elapsedTime) << "\",\n"
        << "  \"step_number\": " << outputStepNumber << ",\n"
        << "  \"regions\": [\n"
        << regions.str() << "\n"
        << "  ]\n"
        << "}\n";
}

std::vector<EEEmulatorLayerEvent> EEEmulatorLayerWriter::readEEPluginLayer(const std::filesystem::path& filePath) {
    std::ifstream file(filePath);
    if (!file.good()) {
        throw std::runtime_error("Could not open file for reading: " + filePath.string());
    }

    std::string content;
    char c;
    while (file.get(c)) {
        content.push_back(c);
    }

    // Manual JSON parsing - extract regions array.
    // The writer format is controlled by this plugin, so a lightweight parser is
    // sufficient as long as we handle nested coordinate arrays and style objects.
    std::vector<EEEmulatorLayerEvent> events;
    std::unordered_map<std::string, EEEmulatorLayerEvent> eventMap;  // Key by event_type to aggregate polygons

    size_t regionStart = content.find("\"regions\": [");
    if (regionStart == std::string::npos) {
        return events;
    }
    regionStart += std::string("\"regions\": [").length();
    size_t regionEnd = content.rfind("]");
    if (regionEnd == std::string::npos || regionEnd < regionStart) {
        return events;
    }

    std::string regionsStr = content.substr(regionStart, regionEnd - regionStart);

    // Parse individual region objects
    size_t pos = 0;
    while (pos < regionsStr.length()) {
        size_t objStart = regionsStr.find("{", pos);
        if (objStart == std::string::npos) {
            break;
        }

        // Find the matching closing brace for the region object.
        int depth = 0;
        size_t objEnd = std::string::npos;
        for (size_t i = objStart; i < regionsStr.length(); ++i) {
            if (regionsStr[i] == '{') {
                ++depth;
            }
            else if (regionsStr[i] == '}') {
                --depth;
                if (depth == 0) {
                    objEnd = i;
                    break;
                }
            }
        }
        if (objEnd == std::string::npos) {
            break;
        }

        std::string objStr = regionsStr.substr(objStart, objEnd - objStart + 1);

        // Extract fields
        auto extractField = [&objStr](const std::string& fieldName) -> std::string {
            const std::string marker = "\"" + fieldName + "\": \"";
            size_t start = objStr.find(marker);
            if (start == std::string::npos) {
                return "";
            }
            start += marker.length();
            size_t end = objStr.find("\"", start);
            if (end == std::string::npos) {
                return "";
            }
            return objStr.substr(start, end - start);
        };

        auto extractCoordinates = [&objStr]() -> std::vector<atlas::PointLonLat> {
            std::vector<atlas::PointLonLat> coords;
            size_t start = objStr.find("\"coordinates\": [");
            if (start == std::string::npos) {
                return coords;
            }
            start += std::string("\"coordinates\": [").length();

            // Find end of coordinates array with bracket-depth tracking.
            int bracketDepth = 1;
            size_t end = std::string::npos;
            for (size_t i = start; i < objStr.length(); ++i) {
                if (objStr[i] == '[') {
                    ++bracketDepth;
                }
                else if (objStr[i] == ']') {
                    --bracketDepth;
                    if (bracketDepth == 0) {
                        end = i;
                        break;
                    }
                }
            }
            if (end == std::string::npos) {
                return coords;
            }

            std::string coordStr = objStr.substr(start, end - start);
            size_t coordPos = 0;
            while (true) {
                size_t bracketStart = coordStr.find("[", coordPos);
                if (bracketStart == std::string::npos) {
                    break;
                }
                size_t bracketEnd = coordStr.find("]", bracketStart);
                if (bracketEnd == std::string::npos) {
                    break;
                }

                std::string coordPair = coordStr.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
                size_t comma = coordPair.find(",");
                if (comma != std::string::npos) {
                    try {
                        double lon = std::stod(coordPair.substr(0, comma));
                        double lat = std::stod(coordPair.substr(comma + 1));
                        coords.push_back(atlas::PointLonLat(lon, lat));
                    }
                    catch (...) {
                        // Skip malformed pairs
                    }
                }
                coordPos = bracketEnd + 1;
            }
            return coords;
        };

        std::string eventType = extractField("event_type");
        std::string description = extractField("description");
        std::string param = extractField("param");
        std::string levtype = extractField("levtype");
        std::string levelist = extractField("levelist");
        auto coordinates = extractCoordinates();

        if (!eventType.empty() && !coordinates.empty()) {
            if (eventMap.find(eventType) == eventMap.end()) {
                eventMap[eventType] = {eventType, description, param, levtype, levelist, {}};
            }
            eventMap[eventType].polygons.push_back(coordinates);
        }

        pos = objEnd + 1;
    }

    // Convert map to vector
    for (auto& entry : eventMap) {
        events.push_back(std::move(entry.second));
    }

    return events;
}

}  // namespace ExtremeEventPlugin
