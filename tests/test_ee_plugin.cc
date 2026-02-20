/*
 * (C) Copyright 2025- ECMWF.
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 *
 * In applying this licence, ECMWF does not waive the privileges and immunities
 * granted to it by virtue of its status as an intergovernmental organisation nor
 * does it submit to any jurisdiction.
 */
#include <ctime>
#include <map>
#include <sstream>
#include <stdlib.h>
#include <vector>

#include "atlas/array.h"
#include "atlas/field/detail/FieldImpl.h"
#include "atlas/functionspace/NodeColumns.h"
#include "atlas/grid.h"
#include "atlas/library.h"
#include "atlas/option.h"
#include "atlas/util/Point.h"
#include "eckit/config/LocalConfiguration.h"
#include "eckit/config/YAMLConfiguration.h"
#include "eckit/testing/Test.h"
#include "plume/data/ModelData.h"

#include "ee_plugin.h"

using namespace eckit::testing;

namespace test {
CASE("test_construction") {
    eckit::LocalConfiguration localEvent;
    localEvent.set("name", "dummyEvent");
    std::vector<eckit::LocalConfiguration> events = {localEvent};
    eckit::LocalConfiguration local;
    local.set("healpix_res", 2);
    local.set("enable_notification", false);
    local.set("aviso_url", "dummy");
    local.set("notify_endpoint", "dummy");
    local.set("events", events);

    EXPECT_NO_THROW(ExtremeEventPlugin::EEPluginCore eePlugin(local););
}

CASE("test_aviso_notification") {
    // Set environment variables
    time_t now = time(0);
    struct tm t;
    localtime_r(&now, &t);
    char dateStr[9];
    strftime(dateStr, sizeof(dateStr), "%Y%m%d", &t);
    std::map<std::string, std::string> vars = {{"CLASS", "test"}, {"TYPE", "test"}, {"EXPVER", "0001"},
                                               {"DATE", dateStr}, {"TIME", "0000"}, {"PLUME_PLUGIN_DEV", "1"}};
    for (const auto& [key, value] : vars) {
        ASSERT(setenv(key.c_str(), value.c_str(), 1) == 0);
    }

    ExtremeEventPlugin::AvisoNotificationHandler notificationHandler("test", "/test");
    std::string data                        = R"({"hello": "world"})";
    std::vector<atlas::PointLonLat> polygon = {atlas::PointLonLat{250.3, 16.9}, atlas::PointLonLat{247.4, 14.4},
                                               atlas::PointLonLat{253.1, 14.4}, atlas::PointLonLat{250.3, 12.0}};
    std::ostringstream capturedOutput;
    std::streambuf* oldCout = std::cout.rdbuf(capturedOutput.rdbuf());
    EXPECT_EQUAL(notificationHandler.send(data, polygon), 999);
    std::cout.rdbuf(oldCout);

    std::string expectedLog =
        "test/test?class=dev_class&date=dev_date&expver=dev_expver&time=dev_time&type=dev_type&"
        "polygon=16.9,250.3,14.4,247.4,14.4,253.1,12,250.3 {\"hello\": \"world\"}\n";
    EXPECT_EQUAL(capturedOutput.str(), expectedLog);

    // Unset environment variables
    for (const auto& var : vars) {
        unsetenv(var.first.c_str());
    }

    EXPECT_NO_THROW(notificationHandler.setSchemaData());

    EXPECT_THROWS_AS(ExtremeEventPlugin::AvisoNotificationHandler notificationHandlerBadEnv("test", "/test"),
                     eckit::BadParameter);
}

CASE("test_setup_skips_missing_params") {
    eckit::LocalConfiguration event;
    event.set("name", "extreme_wind");
    event.set("enabled", true);
    eckit::LocalConfiguration uParam;
    uParam.set("name", "u");
    uParam.set("type", "ATLAS_FIELD");
    eckit::LocalConfiguration vParam;
    vParam.set("name", "v");
    vParam.set("type", "ATLAS_FIELD");
    std::vector<eckit::LocalConfiguration> reqParams{uParam, vParam};
    event.set("required_params", reqParams);
    event.set("instances", std::vector<eckit::LocalConfiguration>{});

    eckit::LocalConfiguration conf;
    conf.set("healpix_res", 2);
    conf.set("enable_notification", false);
    conf.set("aviso_url", "dummy");
    conf.set("notify_endpoint", "dummy");
    conf.set("events", std::vector<eckit::LocalConfiguration>{event});

    ExtremeEventPlugin::EEPluginCore eePlugin(conf);

    atlas::Grid grid("O1");
    atlas::functionspace::NodeColumns fs(grid, atlas::option::halo(0));
    auto swh = fs.createField<double>(atlas::option::name("swh") | atlas::option::levels(1));
    plume::data::ModelData data;
    data.provideParam("swh", &swh);
    data.createParam("NSTEP", 0);
    data.createParam("WSTEP", 0);
    data.createParam("TSTEP", 900.0);
    data.createParam("NFLEVG", 1);

    eePlugin.grabData(data);
    EXPECT_NO_THROW(eePlugin.setup());
    EXPECT_NO_THROW(eePlugin.run());
}

CASE("test_run_extreme_wave_without_notifications") {
    eckit::LocalConfiguration event;
    event.set("name", "extreme_wave");
    event.set("enabled", true);
    eckit::LocalConfiguration swhParam;
    swhParam.set("name", "swh");
    swhParam.set("type", "ATLAS_FIELD");
    event.set("required_params", std::vector<eckit::LocalConfiguration>{swhParam});

    eckit::LocalConfiguration instance;
    instance.set("threshold", 3.5);
    instance.set("description", "test waves");
    event.set("instances", std::vector<eckit::LocalConfiguration>{instance});

    eckit::LocalConfiguration conf;
    conf.set("healpix_res", 2);
    conf.set("enable_notification", false);
    conf.set("aviso_url", "dummy");
    conf.set("notify_endpoint", "dummy");
    conf.set("events", std::vector<eckit::LocalConfiguration>{event});

    ExtremeEventPlugin::EEPluginCore eePlugin(conf);

    atlas::Grid grid("O1");
    atlas::functionspace::NodeColumns fs(grid, atlas::option::halo(0));
    auto swh  = fs.createField<double>(atlas::option::name("swh") | atlas::option::levels(1));
    auto view = atlas::array::make_view<double, 2>(swh);
    for (atlas::idx_t i = 0; i < view.shape(0); ++i) {
        view(i, 0) = 4.0;
    }

    plume::data::ModelData data;
    data.provideParam("swh", &swh);
    data.createParam("NSTEP", 1);
    data.createParam("WSTEP", 0);
    data.createParam("TSTEP", 900.0);
    data.createParam("NFLEVG", 1);
    data.setUpdated({"swh"});

    eePlugin.grabData(data);
    eePlugin.setup();
    EXPECT_NO_THROW(eePlugin.run());
}
}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}