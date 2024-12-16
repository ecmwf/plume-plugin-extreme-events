#include "eckit/config/LocalConfiguration.h"
#include "ee_base.h"

#include <map>
#include <string>

class ExtremeEventRegistry {
public:
  using ExtremeEventFactory = std::function<std::unique_ptr<ExtremeEvent>(
      const eckit::LocalConfiguration &config)>;

  static void registerEvent(const std::string &eventName,
                            ExtremeEventFactory factory);
  static std::unique_ptr<ExtremeEvent>
  createInstance(const std::string &eventName,
                 const eckit::LocalConfiguration &config);

private:
  static std::map<std::string, ExtremeEventFactory> &getRegistry();
};