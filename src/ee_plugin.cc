#include "ee_plugin.h"
#include "healpix_utils.h"

#include "atlas/field/Field.h"

// Can be swapped to use another point to Extreme Event cell mapping
using namespace HEALPixUtils;

namespace ExtremeEventPlugin {

EEPluginCore::EEPluginCore(const eckit::Configuration &conf)
    : PluginCore(conf) {
  healpixRes = conf.getInt("healpix_res");
  enableNotification_ = conf.getBool("enable_notification");
  if (enableNotification_) {
    notificationHandler = AvisoNotificationHandler(
        conf.getString("aviso_url"), conf.getString("notify_endpoint"));
  }

  extremeEventConfig = conf.getSubConfigurations("events");
}

EEPluginCore::~EEPluginCore() {}

void EEPluginCore::setup() {
  // initialize extremeEventList from config
  for (auto &ee : extremeEventConfig) {
    if (!ee.getBool("enabled")) {
      continue;
    }
    extremeEvents.push_back(
        ExtremeEventRegistry::createInstance(ee.getString("name"), ee));
  }
  // Healpix - grid points & polygon mapping matrix
  setHEALPixMapping();
}

void EEPluginCore::run() {
  for (auto &ee : extremeEvents) {
    // Run the detection for each extreme event suite
    auto results = ee->detect(modelData());
    for (size_t idx = 0; idx < results.size(); ++idx) {
      if (std::get<1>(results[idx]).empty()) {
        // No actual points were detected for that instance of the event
        continue;
      }
      auto ee_polygon_points = cellToPolygons(std::get<1>(results[idx]),
                                              Point2HPcell_, HPcell2polygon_);
      if (enableNotification_) {
        // Send notification for each polygon individually if enabled
        for (auto &polygon : ee_polygon_points) {
          // TODO: refine the payload
          std::string payload = R"({
            "NSTEP": ")" + std::to_string(modelData().getInt("NSTEP")) +
                                R"(",
            "description": ")" + std::get<0>(results[idx]) +
                                R"("
          })";
          notificationHandler.send(payload, polygon);
        }
      } else {
        // TODO what do we do with the results of notifications are disabled ?
      }
    }
  }
}

void EEPluginCore::setHEALPixMapping() {
  // lon lat of grid points (from the input field)
  // TODO: input fields might use different grids (e.g., wave vs. atmospheric
  // models) so we might need to be more subtle about how we retrieve the lonlat
  // grid maybe it can be deferred to the ModelData class in Plume ?
  atlas::Field u_field = modelData().getAtlasFieldShared("100u");
  mapLonLatToHEALPixCell(healpixRes, u_field, Point2HPcell_, HPcell2polygon_);
}

// ------------------------------------------------------

// ------------------------------------------------------

EEPlugin::EEPlugin() : Plugin("EEPlugin"){};

EEPlugin::~EEPlugin(){};

const EEPlugin &EEPlugin::instance() {
  static EEPlugin instance;
  return instance;
}
// ------------------------------------------------------
} // namespace ExtremeEventPlugin