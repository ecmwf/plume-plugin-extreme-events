#include "atlas/grid.h"
#include "eckit/config/Configuration.h"
#include "eckit/config/LocalConfiguration.h"
#include "ee_registry/ee_registry.h"
#include "notification.h"
#include "plume/Plugin.h"
#include "plume/PluginCore.h"

namespace ExtremeEventPlugin {

class EEPluginCore : public plume::PluginCore {
public:
  EEPluginCore(const eckit::Configuration &conf);
  ~EEPluginCore();
  void setup() override;
  void run() override;
  constexpr static const char *type() { return "ee-plugincore"; }

private:
  // Extreme event product list
  std::vector<eckit::LocalConfiguration> extremeEventConfig;
  std::vector<std::unique_ptr<ExtremeEvent>> extremeEvents;

  // Aviso handler
  AvisoNotificationHandler notificationHandler;
  bool enableNotification_;

  // HEALPix
  int healpixRes = 2;
  // Mapping from point index to HEALPix cell index
  std::vector<int> Point2HPcell_;
  // Mapping from HEALPix cell index to polygon
  std::vector<std::vector<atlas::PointLonLat>> HPcell2polygon_;
  void setHEALPixMapping();
};

class EEPlugin : public plume::Plugin {
public:
  EEPlugin();
  ~EEPlugin();

  plume::Protocol negotiate() override {
    plume::Protocol protocol;
    // Until we decide on the negotiation the fields needed are hardcoded here
    // TODO: read ee conf how ? plume config ? ee config ?
    protocol.requireInt("NSTEP");
    protocol.requireAtlasField("100u");
    protocol.requireAtlasField("100v");
    return protocol;
  }

  // Return the static instance
  static const EEPlugin &instance();
  std::string version() const override { return "0.0.1-EE-plugin"; }

  std::string gitsha1(unsigned int count) const override { return "undefined"; }

  virtual std::string plugincoreName() const override {
    return EEPluginCore::type();
  }
};

} // namespace ExtremeEventPlugin