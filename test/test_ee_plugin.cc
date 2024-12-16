/*TODO*/
#include "atlas/library.h"
#include "eckit/config/LocalConfiguration.h"
#include "eckit/testing/Test.h"
#include "ee_plugin.h"

using namespace eckit::testing;

namespace test {
CASE("test construction") {
  eckit::LocalConfiguration localEvent;
  localEvent.set("name", "dummyEvent");
  std::vector<eckit::LocalConfiguration> events = {localEvent};
  eckit::LocalConfiguration local;
  local.set("healpix_res", 2);
  local.set("enable_notification", false);
  local.set("aviso_url", "dummy");
  local.set("notify_endpoint", "dummy");
  local.set("events", events);

  EXPECT_NO_THROW(ExtremeEventPlugin::EEPluginCore eePlugin(local));
}
} // namespace test

int main(int argc, char **argv) {
  atlas::initialize(argc,argv);
  return run_tests(argc, argv);
}