#ifndef EE_BASE_H
#define EE_BASE_H
#include <string>

#include "plume/data/ModelData.h"
#include "../plugin_types.h"

class ExtremeEvent {
private:
  const std::string type = "ExtremeEvent";

public:
  ExtremeEvent(){};
  ExtremeEvent(const eckit::LocalConfiguration &config){};
  virtual ~ExtremeEvent() = default;
  // The event classes are only responsible for maintaining their detection
  // algorithm The ee plugin is responsible for notifying aviso if applicable

  // Returns the EE detected as a vector of tuple(description, [indices])
  // The indices can later be used by the plugin to extract the EE polygon
  virtual std::vector<std::tuple<std::string, std::vector<int>>>
  detect(plume::data::ModelData &modelData) = 0;
};

#endif // EE_BASE_H