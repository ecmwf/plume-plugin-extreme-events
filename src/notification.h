#include <map>
#include <string>
#include <vector>

#include "atlas/util/Point.h"

namespace ExtremeEventPlugin {

std::string urlEncode(const std::map<std::string, std::string> &paramsMap,
                      const std::string polygon);
std::string urlEncode(const std::map<std::string, std::string> &paramsMap,
                      const std::vector<atlas::PointLonLat> &polygon);

class AvisoNotificationHandler {

private:
  std::string urlBase;
  std::string urlNotify;
  // The fields are based on Aviso schema config
  // TODO: update payload or schema to have event-specific data (type,
  // severity...)
  std::map<std::string, std::string> schemaData = {
      {"class", ""}, {"type", ""}, {"expver", ""}, {"date", ""}, {"time", ""}};

public:
  // Constructor
  AvisoNotificationHandler();
  AvisoNotificationHandler(const std::string &base, const std::string &notify);

  // Getters
  const std::map<std::string, std::string> &getSchemaData() const;

  // Setters
  void setSchemaData();
  void updateSchemaData(const std::string &key, const std::string &value);

  virtual int send(const std::string payload,
                   const std::vector<atlas::PointLonLat> &polygon);
};

} // namespace ExtremeEventPlugin
