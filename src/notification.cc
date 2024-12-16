#include <sstream>

#include "eckit/io/EasyCURL.h"
#include "eckit/log/Log.h"

#include "notification.h"

using namespace eckit;
namespace ExtremeEventPlugin {

std::string urlEncode(const std::map<std::string, std::string> &paramsMap,
                      const std::string polygon) {
  std::ostringstream urlStream;

  urlStream << "?";
  for (const auto &[key, value] : paramsMap) {
    if (value != "") {
      urlStream << key << "=" << value << "&";
    }
  }
  urlStream << "polygon=" << polygon;

  return urlStream.str();
}

std::string urlEncode(const std::map<std::string, std::string> &paramsMap,
                      const std::vector<atlas::PointLonLat> &polygon) {
  std::ostringstream polygonStr;
  for (size_t i = 0; i < polygon.size(); ++i) {
    polygonStr << polygon[i][0] << "," << polygon[i][1];
    if (i != polygon.size() - 1) {
      polygonStr << ",";
    }
  }
  return urlEncode(paramsMap, polygonStr.str());
}

AvisoNotificationHandler::AvisoNotificationHandler() {}
AvisoNotificationHandler::AvisoNotificationHandler(const std::string &base,
                                                   const std::string &notify)
    : urlBase(base), urlNotify(base + notify) {
  setSchemaData();
}

const std::map<std::string, std::string> &
AvisoNotificationHandler::getSchemaData() const {
  return schemaData;
}

// Method to update schema with a single key-value pair (partial update)
// Only the keys already present in the map can be updated to preserve the
// schema
void AvisoNotificationHandler::updateSchemaData(const std::string &key,
                                                const std::string &value) {
  if (schemaData.find(key) == schemaData.end()) {
    Log::info() << "'" << key << "' is not a valid field, skipping update..."
                << std::endl;
    return;
  }
  schemaData[key] = value;
}

// Populates the schema data from the environment
// TODO: For now it assumes each url field can be mapped to its environment
// variable by using upper case, but it might need to be refined depending on
// the context
void AvisoNotificationHandler::setSchemaData() {
  for (const auto &[key, value] : schemaData) {
    std::string upperKey = key.c_str();
    for (char &c : upperKey) {
      c = std::toupper(static_cast<unsigned char>(c));
    }
    const char *schemaValue = std::getenv(upperKey.c_str());
    if (!schemaValue) {
      // TODO: some error handling
    } else {
      schemaData[key] = schemaValue;
    }
  }
}

int AvisoNotificationHandler::send(
    const std::string payload, const std::vector<atlas::PointLonLat> &polygon) {

  EasyCURLHeaders headers;
  headers["content-type"] = "application/json";

  auto curl = EasyCURL();
  curl.headers(headers);

  if (atoi(std::getenv("PLUME_PLUGIN_DEV"))) {
    // For convenience to avoid sending Aviso notifications while developing
    std::cout << urlEncode(schemaData, polygon) << " " << payload << std::endl;
    return 999;
  } else {
    auto response =
        curl.POST(urlNotify + urlEncode(schemaData, polygon), payload);
    return response.code();
  }
}

} // namespace ExtremeEventPlugin
