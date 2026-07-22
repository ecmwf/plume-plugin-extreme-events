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

#include "ee_registry/wind_drought.h"
#include "plugin_types.h"

using namespace eckit::testing;

namespace {

struct WindDroughtFixture {
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
    std::optional<std::string> height;
    atlas::Field u;
    atlas::Field v;
    std::vector<int> coarseMapping;

    explicit WindDroughtFixture(int nflevg = 1, std::optional<std::string> height = std::nullopt,
                                double tstep = 300.0) :
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

    void setValues(const std::array<FIELD_TYPE_REAL, 3>& uValues, const std::array<FIELD_TYPE_REAL, 3>& vValues,
                   int level = 0) {
        auto setField = [&](auto& uView, auto& vView, auto mutator) {
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
        setField(uView, vView,
                 [](auto& field, atlas::idx_t i, FIELD_TYPE_REAL value, int level) { field(i, level) = value; });
    }
};

eckit::LocalConfiguration windDroughtConfig(double cutout, size_t timeWindow,
                                            std::optional<std::string> height = std::nullopt, size_t modelLevel = 1) {
    eckit::LocalConfiguration config;
    config.set("name", "wind_drought");
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

CASE("wind drought setup rejects negative cutout") {
    WindDroughtFixture fixture(1, std::string("1"));
    auto config = windDroughtConfig(-1.0, 10, std::string("1"));
    EXPECT_THROWS_AS(WindDrought(config, fixture.modelDataView(), fixture.coarseMapping), eckit::BadValue);
}

CASE("wind drought detection on height levels") {
    std::string height = "1";
    WindDroughtFixture fixture(1, height);
    auto config = windDroughtConfig(2.0, 10, height);
    WindDrought event(config, fixture.modelDataView(), fixture.coarseMapping);

    fixture.setValues({1.0, 1.0, 3.0}, {0.5, 0.5, 1.5});
    auto first = event.detect(fixture.modelDataView());
    EXPECT_EQUAL(first.size(), 1);
    EXPECT(first[0].detectedCells.empty());

    fixture.setValues({1.0, 1.0, 3.0}, {0.5, 0.5, 1.5});
    auto second = event.detect(fixture.modelDataView());
    EXPECT(second[0].detectedCells.empty());

    fixture.setValues({1.0, 1.0, 3.0}, {0.5, 0.5, 1.5});
    auto third        = event.detect(fixture.modelDataView());
    const auto& cells = third[0].detectedCells;
    EXPECT(cells.find(1) != cells.end());
    EXPECT(cells.find(2) == cells.end());
}

CASE("wind drought detection on model levels") {
    size_t modelLevel = 1;
    WindDroughtFixture fixture(2);
    auto config = windDroughtConfig(2.0, 10, std::nullopt, modelLevel);
    WindDrought event(config, fixture.modelDataView(), fixture.coarseMapping);

    fixture.setValues({1.0, 1.0, 3.0}, {0.5, 0.5, 1.5}, modelLevel - 1);
    auto first = event.detect(fixture.modelDataView());
    EXPECT_EQUAL(first.size(), 1);
    EXPECT(first[0].detectedCells.empty());

    fixture.setValues({1.0, 1.0, 3.0}, {0.5, 0.5, 1.5}, modelLevel - 1);
    auto second = event.detect(fixture.modelDataView());
    EXPECT(second[0].detectedCells.empty());

    fixture.setValues({1.0, 1.0, 3.0}, {0.5, 0.5, 1.5}, modelLevel - 1);
    auto third        = event.detect(fixture.modelDataView());
    const auto& cells = third[0].detectedCells;
    EXPECT(cells.find(1) != cells.end());
    EXPECT(cells.find(2) == cells.end());
}

}  // namespace test

int main(int argc, char** argv) {
    return run_tests(argc, argv);
}
