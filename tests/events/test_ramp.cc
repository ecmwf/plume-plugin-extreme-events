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

#include "ee_registry/ramp.h"
#include "plugin_types.h"

using namespace eckit::testing;

namespace {

struct RampFixture {
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
    atlas::Field field;
    std::vector<int> coarseMapping;

    RampFixture() :
        grid("O1"),
        fs(grid, atlas::option::halo(0)),
        field(fs.createField<FIELD_TYPE_REAL>(atlas::option::name("2t") | atlas::option::levels(1))) {
        ASSERT(field.shape(0) > 2);
        coarseMapping.assign(field.shape(0), 0);
        coarseMapping[0] = 1;
        coarseMapping[1] = 1;
        coarseMapping[2] = 2;

        data.provideParam("2t", &field);
        data.createParam("TSTEP", 300.0);
        data.createParam("NSTEP", 0);
    }

    void setValues(const std::array<FIELD_TYPE_REAL, 3>& values) {
        auto view = atlas::array::make_view<FIELD_TYPE_REAL, 2>(field);
        for (atlas::idx_t i = 0; i < view.shape(0); ++i) {
            view(i, 0) = 0;
        }
        view(0, 0) = values[0];
        view(1, 0) = values[1];
        view(2, 0) = values[2];
    }
};

eckit::LocalConfiguration rampConfig(double rampUp, std::optional<double> rampDown, size_t timeWindow) {
    eckit::LocalConfiguration config;
    config.set("name", "ramp");
    config.set("time_window", timeWindow);
    config.set("ramp_up_value", rampUp);
    if (rampDown.has_value()) {
        config.set("ramp_down_value", *rampDown);
    }

    eckit::LocalConfiguration param;
    param.set("name", "2t");
    param.set("type", "ATLAS_FIELD");
    config.set("required_params", std::vector<eckit::LocalConfiguration>{param});

    return config;
}

}  // namespace

namespace test {

CASE("ramp setup rejects missing thresholds") {
    RampFixture fixture;
    eckit::LocalConfiguration config;
    config.set("name", "ramp");
    config.set("time_window", 10);
    eckit::LocalConfiguration param;
    param.set("name", "2t");
    param.set("type", "ATLAS_FIELD");
    config.set("required_params", std::vector<eckit::LocalConfiguration>{param});

    EXPECT_THROWS_AS(RampEvent(config, fixture.modelDataView(), fixture.coarseMapping), eckit::BadValue);
}

CASE("ramp setup rejects negative thresholds") {
    RampFixture fixture;
    auto config = rampConfig(-1.0, std::nullopt, 10);
    EXPECT_THROWS_AS(RampEvent(config, fixture.modelDataView(), fixture.coarseMapping), eckit::BadValue);
}

CASE("ramp detection for up and down") {
    RampFixture fixture;
    auto config = rampConfig(10.0, 8.0, 10);
    RampEvent event(config, fixture.modelDataView(), fixture.coarseMapping);

    fixture.setValues({10.0, 10.0, 10.0});
    fixture.data.updateParam("NSTEP", 1);
    auto first = event.detect(fixture.modelDataView());
    EXPECT(first.empty());

    fixture.setValues({25.0, 5.0, 0.0});
    fixture.data.updateParam("NSTEP", 2);
    auto second = event.detect(fixture.modelDataView());

    EXPECT_EQUAL(second.size(), 2);
    const auto& rampUp   = second[0].detectedCells;
    const auto& rampDown = second[1].detectedCells;
    EXPECT(rampUp.find(1) != rampUp.end());
    EXPECT(rampUp.find(2) == rampUp.end());
    EXPECT(rampDown.find(1) == rampDown.end());
    EXPECT(rampDown.find(2) != rampDown.end());
}

}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
