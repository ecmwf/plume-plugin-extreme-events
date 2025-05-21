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
#include <string>
#include <vector>

#include "eckit/config/LocalConfiguration.h"
#include "plume/data/ModelData.h"

#include "ee_registry.h"

/**
 * @class ExtremeWind
 * @brief This event represents extreme winds at a given time step.
 *
 * The thresholds to use should be defined in the configuration. See README for configuration guidelines.
 */
class ExtremeWind final : public ExtremeEvent {
private:
    static const std::string type_;
    static const std::array<std::string, 6> supportedFields_;

    /**
     * @brief Represents the wind thresholds to run detection on.
     *
     * A description can be provided for communicating results in a human-friendly fashion.
     * @todo Implement support for height.
     */
    struct Interval {
        double lBound, uBound;
        int height, modelLevel;
        std::string u, v, description;
    };

    std::vector<Interval> intervals_;
    const std::vector<int>& coarseMapping_;

public:
    /**
     * @brief Constructs an extreme wind event.
     *
     * The plugin does not validate that the configured fields are indeed representing winds, but it enforces
     * the use of either the surface fields {10,100}{u,v} or the leveled fields {u,v}.
     * @warning Detection at given heights when levels are provided is not supported yet.
     *
     * @param config The configuration of the event, mainly consisting of parameters for bounds, description, and height
     *        for several instances.
     * @param modelData The model data passed through Plume.
     * @param coarseMapping The mapping to use to coarsen the detection data.
     */
    ExtremeWind(const eckit::LocalConfiguration& config, plume::data::ModelData& modelData,
                const std::vector<int>& coarseMapping);

    /**
     * @brief Detects extreme winds at a given time step.
     *
     * This event checks whether the wind exceeds a certain threshold, or is between bounds, at a single time step.
     *
     * @param modelData The model data that contains the wind fields to run detection on.
     *
     * @return The detection results for each set of options (intervals).
     */
    std::vector<ExtremeEvent::DetectionData> detect(plume::data::ModelData& modelData) override;

    /// Register the extreme wind event into the registry so it can be used in the plugin.
    static struct Registrar {
        Registrar() {
            ExtremeEventRegistry::instance().registerEvent(
                type_, [](const eckit::LocalConfiguration& config, plume::data::ModelData& modelData,
                          const std::vector<int>& coarseMapping) {
                    return std::make_unique<ExtremeWind>(config, modelData, coarseMapping);
                });
        }
    } registrar;
};
