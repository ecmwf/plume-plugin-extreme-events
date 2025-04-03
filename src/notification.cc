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

#include "eckit/exception/Exceptions.h"
#include "eckit/io/EasyCURL.h"
#include "eckit/log/Log.h"

#include "notification.h"

using namespace eckit;
namespace ExtremeEventPlugin {

AvisoNotificationHandler::AvisoNotificationHandler(const std::string& base, const std::string& notify) :
    urlBase_(base), urlNotify_(base + notify) {
    setSchemaData();
}

std::string AvisoNotificationHandler::urlEncode(const std::string polygon) {
    std::ostringstream urlStream;

    urlStream << "?";
    for (const auto& [key, value] : schemaData_) {
        if (value != "") {
            urlStream << key << "=" << value << "&";
        }
    }
    urlStream << "polygon=" << polygon;

    return urlNotify_ + urlStream.str();
}

std::string AvisoNotificationHandler::urlEncode(const std::vector<atlas::PointLonLat>& polygon) {
    std::ostringstream polygonStr;
    for (size_t i = 0; i < polygon.size() - 1; ++i) {
        polygonStr << polygon[i].lat() << "," << polygon[i].lon() << ",";
    }
    polygonStr << polygon.back().lat() << "," << polygon.back().lon();
    return urlEncode(polygonStr.str());
}

void AvisoNotificationHandler::setSchemaData() {
    for (const auto& [key, value] : schemaData_) {
        std::string upperKey = key.c_str();
        for (char& c : upperKey) {
            c = std::toupper(static_cast<unsigned char>(c));
        }
        const char* schemaValue = std::getenv(upperKey.c_str());
        if (!schemaValue) {
            throw BadParameter("Schema key '" + upperKey + "' could not be found in the environment", Here());
        }
        schemaData_[key] = schemaValue;
    }
}

int AvisoNotificationHandler::send(const std::string payload, const std::vector<atlas::PointLonLat>& polygon) {
    EasyCURLHeaders headers;
    headers["content-type"] = "application/json";

    auto curl = EasyCURL();
    curl.headers(headers);

    if (atoi(std::getenv("PLUME_PLUGIN_DEV"))) {
        // For convenience to avoid sending Aviso notifications while developing
        std::cout << urlEncode(polygon) << " " << payload << std::endl;
        return 999;
    }
    else {
        auto response = curl.POST(urlEncode(polygon), payload);
        return response.code();
    }
}

}  // namespace ExtremeEventPlugin
