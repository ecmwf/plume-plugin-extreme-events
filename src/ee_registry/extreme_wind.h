#include "eckit/config/LocalConfiguration.h"
#include "ee_registry.h"
#include "plume/data/ModelData.h"
#include <vector>

// Represent the wind thresholds to run detection on
// A description can be provided for communicating results
struct Interval {
  double lBound;
  double uBound;
  std::string description;
};

class ExtremeWind : public ExtremeEvent {
private:
  std::vector<Interval> intervals;
  static std::vector<std::string> requiredParams;

public:
  ExtremeWind(const eckit::LocalConfiguration &config);
  static const std::string type;
  std::vector<std::tuple<std::string, std::vector<int>>>
  detect(plume::data::ModelData &modelData) override;
  static struct Registrar {
    Registrar() {
      ExtremeEventRegistry::registerEvent(
          type, [](const eckit::LocalConfiguration &config) {
            return std::make_unique<ExtremeWind>(config);
          });
    }
  } registrar;
};
