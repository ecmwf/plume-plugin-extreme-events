#include <functional>
#include <memory>

#include "ee_registry.h"

void ExtremeEventRegistry::registerEvent(const std::string &eventName,
                                         ExtremeEventFactory factory) {
  getRegistry()[eventName] = factory;
}

std::unique_ptr<ExtremeEvent>
ExtremeEventRegistry::createInstance(const std::string &eventName,
                                     const eckit::LocalConfiguration &config) {
  auto it = getRegistry().find(eventName);
  if (it != getRegistry().end()) {
    return it->second(config);
  }
  return nullptr;
}

std::map<std::string, ExtremeEventRegistry::ExtremeEventFactory> &
ExtremeEventRegistry::getRegistry() {
  static std::map<std::string, ExtremeEventFactory> registry;
  return registry;
}
