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
#include "plume/data/ParameterValue.h"

#include "ee_registry/storm.h"
#include "plugin_types.h"

using namespace eckit::testing;

namespace {

/**
 * @brief A test fixture for the Storm event tests.
 */
struct StormFixture {
    plume::data::ModelData data;
    atlas::Grid grid;
    atlas::functionspace::NodeColumns fs;
    std::optional<std::string> height;
    atlas::Field u;
    atlas::Field v;
    std::vector<int> coarseMapping;

    /**
     * @brief Constructs the fixture with a grid and fields of the specified number of levels and time step.
     */
    explicit StormFixture(int nflevg = 1, std::optional<std::string> height = std::nullopt, double tstep = 300.0) :
        grid("O1"),
        fs(grid, atlas::option::halo(0)),
        height(height),
        u(fs.createField<FIELD_TYPE_REAL>(atlas::option::name("u") |
                                          atlas::option::levels(height.has_value() ? 1 : nflevg))),
        v(fs.createField<FIELD_TYPE_REAL>(atlas::option::name("v") |
                                          atlas::option::levels(height.has_value() ? 1 : nflevg))) {
        ASSERT(u.shape(0) > 2);
        coarseMapping.assign(u.shape(0), 0);
        coarseMapping[0] = 1;
        coarseMapping[1] = 1;
        coarseMapping[2] = 2;

        if (height.has_value()) {
            nflevg = 1;
        }
        std::string uStr =
            height.has_value() ? plume::data::IParameterObserver::deriveParamName("u", "hl", *height) : "u";
        std::string vStr =
            height.has_value() ? plume::data::IParameterObserver::deriveParamName("v", "hl", *height) : "v";
        data.provideParam(uStr, &u);
        data.provideParam(vStr, &v);
        data.createParam("NFLEVG", nflevg);
        data.createParam("TSTEP", tstep);
        data.createParam("NSTEP", 0);
    }

    /**
     * @brief Sets the first three values of u and v to the provided values and rest to zero at a given level.
     */
    void setLevelValues(const std::array<FIELD_TYPE_REAL, 3>& values, int level = 0) {
        auto setValues = [&](auto& uView, auto& vView, auto mutator) {
            for (atlas::idx_t i = 0; i < uView.shape(0); ++i) {
                mutator(uView, i, 0, level);
                mutator(vView, i, 0, level);
            }
            mutator(uView, 0, values[0], level);
            mutator(uView, 1, values[1], level);
            mutator(uView, 2, values[2], level);
        };

        auto uView = atlas::array::make_view<FIELD_TYPE_REAL, 2>(u);
        auto vView = atlas::array::make_view<FIELD_TYPE_REAL, 2>(v);
        setValues(uView, vView,
                  [](auto& field, atlas::idx_t i, FIELD_TYPE_REAL value, int level) { field(i, level) = value; });
    }
};

/**
 * @brief Helper function to create a storm event configuration with the specified parameters.
 */
eckit::LocalConfiguration stormConfig(double cutout, size_t timeWindow,
                                      std::optional<std::string> height = std::nullopt, size_t modelLevel = 1) {
    eckit::LocalConfiguration config;
    config.set("name", "storm");
    config.set("wind_speed_cutout", cutout);
    config.set("time_window", timeWindow);

    eckit::LocalConfiguration uParam;
    uParam.set("name", "u");
    uParam.set("type", "ATLAS_FIELD");
    eckit::LocalConfiguration vParam;
    vParam.set("name", "v");
    vParam.set("type", "ATLAS_FIELD");

    if (height.has_value()) {
        uParam.set("height", *height);
        vParam.set("height", *height);
    }
    else {
        config.set("model_level", modelLevel);
    }

    std::vector<eckit::LocalConfiguration> reqParams{uParam, vParam};
    config.set("required_params", reqParams);
    return config;
}

}  // namespace

namespace test {

CASE("storm setup rejects missing wind fields") {
    StormFixture fixture;
    auto config       = stormConfig(20.0, 10, std::nullopt, 1);
    std::string vPath = std::string("required_params") + config.separator() + "1" + config.separator() + "name";
    config.set(vPath, "not_v");
    EXPECT_THROWS_AS(Storm(config, fixture.data, fixture.coarseMapping), eckit::BadValue);
}

CASE("storm setup rejects negative cutout") {
    StormFixture fixture;
    auto config = stormConfig(-1.0, 10, std::nullopt, 1);
    EXPECT_THROWS_AS(Storm(config, fixture.data, fixture.coarseMapping), eckit::BadValue);
}

CASE("storm setup rejects model level above NFLEVG") {
    StormFixture fixture;
    auto config = stormConfig(20.0, 10, std::nullopt, 2);
    EXPECT_THROWS_AS(Storm(config, fixture.data, fixture.coarseMapping), eckit::BadValue);
}

CASE("storm detection on model levels") {
    StormFixture fixture;
    size_t modelLevel = 1;
    auto config       = stormConfig(18.0, 10, std::nullopt, modelLevel);
    Storm storm(config, fixture.data, fixture.coarseMapping);

    fixture.setLevelValues({10.0, 20.0, 5.0}, modelLevel - 1);
    fixture.data.updateParam("NSTEP", 1);
    auto first = storm.detect(fixture.data);
    EXPECT(first.empty());

    fixture.setLevelValues({30.0, 10.0, 25.0}, modelLevel - 1);
    fixture.data.updateParam("NSTEP", 2);
    auto second = storm.detect(fixture.data);

    EXPECT_EQUAL(second.size(), 1);
    const auto& detected = second[0].detectedCells;
    EXPECT(detected.find(0) == detected.end());
    EXPECT(detected.find(1) != detected.end());
    EXPECT(detected.find(2) == detected.end());
}

CASE("storm detection on height levels") {
    std::string height = "10";
    StormFixture fixture(0, height);
    auto config = stormConfig(18.0, 10, height);
    Storm storm(config, fixture.data, fixture.coarseMapping);

    fixture.setLevelValues({10.0, 20.0, 5.0});
    fixture.data.updateParam("NSTEP", 1);
    auto first = storm.detect(fixture.data);
    EXPECT(first.empty());

    fixture.setLevelValues({30.0, 10.0, 25.0});
    fixture.data.updateParam("NSTEP", 2);
    auto second = storm.detect(fixture.data);

    EXPECT_EQUAL(second.size(), 1);
    const auto& detected = second[0].detectedCells;
    EXPECT(detected.find(0) == detected.end());
    EXPECT(detected.find(1) != detected.end());
    EXPECT(detected.find(2) == detected.end());
}

}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
