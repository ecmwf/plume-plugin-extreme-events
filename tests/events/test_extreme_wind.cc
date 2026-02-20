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

#include "ee_registry/extreme_wind.h"
#include "plugin_types.h"

using namespace eckit::testing;

namespace {

struct ExtremeWindFixture {
    plume::data::ModelData data;
    atlas::Grid grid;
    atlas::functionspace::NodeColumns fs;
    std::optional<std::string> height;
    atlas::Field u;
    atlas::Field v;
    std::vector<int> coarseMapping;

    explicit ExtremeWindFixture(int nflevg = 1, std::optional<std::string> height = std::nullopt) :
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
        data.createParam("TSTEP", 300.0);
        data.createParam("NSTEP", 0);
    }

    void setLevelValues(const std::array<FIELD_TYPE_REAL, 3>& uValues,
                        const std::array<FIELD_TYPE_REAL, 3>& vValues, int level = 0) {
        auto setValues = [&](auto& uView, auto& vView, auto mutator) {
            for (atlas::idx_t i = 0; i < uView.shape(0); ++i) {
                mutator(uView, i, 0, level);
                mutator(vView, i, 0, level);
            }
            mutator(uView, 0, uValues[0], level);
            mutator(uView, 1, uValues[1], level);
            mutator(uView, 2, uValues[2], level);
            mutator(vView, 0, vValues[0], level);
            mutator(vView, 1, vValues[1], level);
            mutator(vView, 2, vValues[2], level);
        };

        auto uView = atlas::array::make_view<FIELD_TYPE_REAL, 2>(u);
        auto vView = atlas::array::make_view<FIELD_TYPE_REAL, 2>(v);
        setValues(uView, vView,
                  [](auto& field, atlas::idx_t i, FIELD_TYPE_REAL value, int level) { field(i, level) = value; });
    }
};

eckit::LocalConfiguration extremeWindConfig(double lowerBound, double upperBound,
                                            std::optional<std::string> height           = std::nullopt,
                                            std::optional<std::vector<int>> modelLevels = std::nullopt) {
    eckit::LocalConfiguration config;
    config.set("name", "extreme_wind");

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

    std::vector<eckit::LocalConfiguration> reqParams{uParam, vParam};
    config.set("required_params", reqParams);

    eckit::LocalConfiguration instance;
    instance.set("lower_bound", lowerBound);
    instance.set("upper_bound", upperBound);
    instance.set("description", "test extreme wind");
    if (modelLevels.has_value()) {
        instance.set("model_levels", *modelLevels);
    }
    config.set("instances", std::vector<eckit::LocalConfiguration>{instance});

    return config;
}

}  // namespace

namespace test {

CASE("extreme wind rejects unsupported fields") {
    ExtremeWindFixture fixture;
    auto config       = extremeWindConfig(10.0, 0.0);
    std::string uPath =
        std::string("required_params") + config.separator() + "0" + config.separator() + "name";
    config.set(uPath, "bad_field");

    EXPECT_THROWS_AS(ExtremeWind(config, fixture.data, fixture.coarseMapping), eckit::BadValue);
}

CASE("extreme wind rejects model level above NFLEVG") {
    ExtremeWindFixture fixture(1);
    auto config = extremeWindConfig(20.0, 0.0, std::nullopt, std::vector<int>{2});
    EXPECT_THROWS_AS(ExtremeWind(config, fixture.data, fixture.coarseMapping), eckit::BadValue);
}

CASE("extreme wind detection on model levels") {
    ExtremeWindFixture fixture(2);
    auto config = extremeWindConfig(18.0, 0.0, std::nullopt, std::vector<int>{1});
    ExtremeWind event(config, fixture.data, fixture.coarseMapping);

    fixture.setLevelValues({10.0, 20.0, 5.0}, {5.0, 10.0, 2.5}, 0);
    auto detected = event.detect(fixture.data);

    EXPECT_EQUAL(detected.size(), 1);
    const auto& cells = detected[0].detectedCells;
    EXPECT(cells.find(0) == cells.end());
    EXPECT(cells.find(1) != cells.end());
    EXPECT(cells.find(2) == cells.end());
}

CASE("extreme wind detection on height levels") {
    std::string height = "100";
    ExtremeWindFixture fixture(0, height);
    auto config = extremeWindConfig(18.0, 0.0, height, std::nullopt);
    ExtremeWind event(config, fixture.data, fixture.coarseMapping);

    fixture.setLevelValues({10.0, 20.0, 5.0}, {5.0, 10.0, 2.5});
    auto detected = event.detect(fixture.data);

    EXPECT_EQUAL(detected.size(), 1);
    const auto& cells = detected[0].detectedCells;
    EXPECT(cells.find(0) == cells.end());
    EXPECT(cells.find(1) != cells.end());
    EXPECT(cells.find(2) == cells.end());
}

}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
