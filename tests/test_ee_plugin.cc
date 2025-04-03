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
#include <stdlib.h>
#include <ctime>

#include "atlas/library.h"
#include "atlas/util/Point.h"
#include "eckit/config/LocalConfiguration.h"
#include "eckit/testing/Test.h"

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
    EXPECT_EQUAL(notificationHandler.send(data, polygon), 999);

    // Unset environment variables
    for (const auto& var : vars) {
        unsetenv(var.first.c_str());
    }

    EXPECT_THROWS_AS(notificationHandler.setSchemaData(), eckit::BadParameter);
}
}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}