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
#include <array>
#include <optional>
#include <vector>

#include "atlas/array.h"
#include "atlas/field.h"
#include "atlas/functionspace/NodeColumns.h"
#include "atlas/grid.h"
#include "atlas/option.h"
#include "eckit/config/LocalConfiguration.h"
#include "eckit/exception/Exceptions.h"
#include "eckit/testing/Test.h"

#include "plume/data/ModelData.h"
#include "plume/data/ModelDataView.h"
#include "plume/data/ParameterValue.h"

#include "ee_registry/extreme_wave.h"
#include "plugin_types.h"

using namespace eckit::testing;

namespace {

struct ExtremeWaveFixture {
    plume::data::ModelData data;
    plume::data::ModelDataView& modelDataView() {
        if (!view_) {
            view_.emplace(data);
        }
        return *view_;
    }
    std::optional<plume::data::ModelDataView> view_;
    atlas::Grid grid;
    atlas::functionspace::NodeColumns fs;
    atlas::Field swh;
    std::vector<int> coarseMapping;

    ExtremeWaveFixture() :
        grid("O1"),
        fs(grid, atlas::option::halo(0)),
        swh(fs.createField<FIELD_TYPE_REAL>(atlas::option::name("swh") | atlas::option::levels(1))) {
        ASSERT(swh.shape(0) > 2);
        coarseMapping.assign(swh.shape(0), 0);
        coarseMapping[0] = 1;
        coarseMapping[1] = 1;
        coarseMapping[2] = 2;

        data.provideParam("swh", &swh);
        // data.createParam("TSTEP", 300.0);
        // data.createParam("NSTEP", 0);
    }

    void setValues(const std::array<FIELD_TYPE_REAL, 3>& values) {
        auto view = atlas::array::make_view<FIELD_TYPE_REAL, 2>(swh);
        for (atlas::idx_t i = 0; i < view.shape(0); ++i) {
            view(i, 0) = 0;
        }
        view(0, 0) = values[0];
        view(1, 0) = values[1];
        view(2, 0) = values[2];
    }
};

eckit::LocalConfiguration extremeWaveConfig(const std::vector<double>& thresholds, int missingValue = 9999) {
    eckit::LocalConfiguration config;
    config.set("name", "extreme_wave");
    config.set("missing_value", missingValue);

    eckit::LocalConfiguration swhParam;
    swhParam.set("name", "swh");
    swhParam.set("type", "ATLAS_FIELD");
    config.set("required_params", std::vector<eckit::LocalConfiguration>{swhParam});

    std::vector<eckit::LocalConfiguration> instances;
    for (double threshold : thresholds) {
        eckit::LocalConfiguration instance;
        instance.set("threshold", threshold);
        instance.set("description", "test waves");
        instances.push_back(instance);
    }
    config.set("instances", instances);

    return config;
}

}  // namespace

namespace test {

CASE("extreme wave rejects non swh field") {
    ExtremeWaveFixture fixture;
    auto config         = extremeWaveConfig({2.0});
    std::string swhPath = std::string("required_params") + config.separator() + "0" + config.separator() + "name";
    config.set(swhPath, "u");

    EXPECT_THROWS_AS(ExtremeWave(config, fixture.modelDataView(), fixture.coarseMapping), eckit::BadValue);
}

CASE("extreme wave detection respects thresholds and missing values") {
    ExtremeWaveFixture fixture;
    auto config = extremeWaveConfig({8.0, 3.5}, 9999);
    ExtremeWave event(config, fixture.modelDataView(), fixture.coarseMapping);

    fixture.setValues({9999.0, 5.0, 9.0});
    auto detected = event.detect(fixture.modelDataView());

    EXPECT_EQUAL(detected.size(), 2);
    const auto& high = detected[0].detectedCells;
    const auto& low  = detected[1].detectedCells;

    EXPECT(high.find(1) == high.end());
    EXPECT(high.find(2) != high.end());
    EXPECT(low.find(1) != low.end());
    EXPECT(low.find(2) != low.end());
}

}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
