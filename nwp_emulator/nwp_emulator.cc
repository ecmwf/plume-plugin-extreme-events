#include "eckit/config/LocalConfiguration.h"
#include "eckit/config/YAMLConfiguration.h"
#include "eckit/log/Log.h"

#include "plume/Manager.h"
#include "plume/data/ModelData.h"
#include "plume/data/ParameterCatalogue.h"

#include "nwp_emulator.h"
#include "nwp_utils.h"

using eckit::Log;

namespace nwp_utils {
NWPEmulator::NWPEmulator(int argc, char **argv)
    : atlas::AtlasTool(argc, argv), plumeConfigPath_(argv[1]){};

int NWPEmulator::execute(const Args &args) {

  Log::info() << "*** Running NWP emulator ***" << std::endl;

  // Plume manager configuration
  eckit::YAMLConfiguration plumeConfig(eckit::PathName{plumeConfigPath_});
  plume::Manager::configure(plumeConfig);

  // Define data offered by Plume
  plume::Protocol offers;

  // Example of parameters to pass from the model to plugins

  // Note: parameters might be available "always"
  // (i.e. regardless of whether plugins require them or not)
  offers.offerInt("NSTEP", "always", "Simulation Step");

  // NWP Atlas Fields passed to plugins. They are passed "on-request"
  // (i.e. passed to plugins only if required by active plugins)
  offers.offerAtlasField("u", "on-request", "Dummy field representing u");
  offers.offerAtlasField("v", "on-request", "Dummy field representing v");
  offers.offerAtlasField("100u", "on-request", "Dummy field representing 100u");
  offers.offerAtlasField("100v", "on-request", "Dummy field representing 100v");

  // Negotiate with plugins.
  // At this point, plugins that succeed in the negotiation
  // are "activated" (i.e. they are eligible to run)
  plume::Manager::negotiate(offers);

  // Setup field generator (to generate dummy NWP fields)
  FieldGenerator field_gen;
  field_gen.setupFunctionSpace();

  atlas::Field fieldU = field_gen.createField3D("u");
  atlas::Field fieldV = field_gen.createField3D("v");
  atlas::Field field100u =
      field_gen.createField2D("100u"); // Could use fieldSet ?
  atlas::Field field100v = field_gen.createField2D("100v");

  // Setup Plume data
  plume::data::ModelData data;

  // Initialise parameter
  data.createInt("NSTEP", 0);

  // Give Plume only the NWP fields requested by activated plugins
  if (plume::Manager::isParamRequested("u")) {
    data.provideAtlasFieldShared("u", fieldU.get());
  }

  if (plume::Manager::isParamRequested("v")) {
    data.provideAtlasFieldShared("v", fieldV.get());
  }

  if (plume::Manager::isParamRequested("100u")) {
    data.provideAtlasFieldShared("100u", field100u.get());
  }

  if (plume::Manager::isParamRequested("100v")) {
    data.provideAtlasFieldShared("100v", field100v.get());
  }

  // Feed plugins with the data
  plume::Manager::feedPlugins(data);

  // Run the model for 10 iterations and run Plume
  for (int iStep = 0; iStep < 10; iStep++) {

    // Update parameters as necessary
    data.updateInt("NSTEP", iStep);
    // Update wind fields for extreme wind demo (low and high wind regions)
    updateField(field100u, iStep);

    plume::Manager::run();
  }

  // Teardown Plume and plugins
  plume::Manager::teardown();

  Log::info() << "*** NWP emulator has completed. ***" << std::endl;
  Log::info() << std::endl;

  return 0;
};
} // namespace nwp_utils
