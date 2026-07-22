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
#include <string>
#include <vector>

#include "eckit/config/LocalConfiguration.h"
#include "plume/data/ModelDataView.h"

#include "ee_registry.h"

/**
 * @class ExtremeWave
 * @brief This event represents extreme waves at a given time step.
 *
 * The thresholds to use should be defined in the configuration. See README for configuration guidelines.
 */
class ExtremeWave final : public ExtremeEvent {
private:
    static const std::string type_;

    const std::vector<int>& coarseMapping_;
    const FIELD_TYPE_REAL missingValue_;

    /**
     * @brief Represents the wave thresholds to run detection on.
     *
     * A description can be provided for communicating results in a human-friendly fashion.
     */
    struct Threshold {
        FIELD_TYPE_REAL value;
        std::string description;
    };

    std::vector<Threshold> thresholds_;

public:
    /**
     * @brief Constructs an extreme wave event.
     *
     * This event is using the same coarsening mapping as atmospheric-related events as it is assumed the wave fields
     * are represented on the same grid.
     *
     * @param config The configuration of the event, more importantly the wave height thresholds and their description.
     * @param modelData The model data passed through Plume.
     * @param coarseMapping The mapping to use to coarsen the detection data.
     */
    ExtremeWave(const eckit::LocalConfiguration& config, plume::data::ModelDataView& modelData,
                const std::vector<int>& coarseMapping);

    /**
     * @brief Detects extreme waves at a given time step.
     *
     * This event checks whether the significant wave height of combined wind waves and swell exceeds certain
     * thresholds at a single time step. Several thresholds are allowed to represent different level of severity,
     * e.g., there can be a threshold for safe offshore wind farm operations, and a threshold for wind turbine
     * structural damage.
     *
     * @param modelData The model data that contains the wind fields to run detection on.
     *
     * @return The detection results for each threshold.
     */
    std::vector<ExtremeEvent::DetectionData> detect(plume::data::ModelDataView& modelData) override;

    /**
     * @brief Returns the type/name of the extreme event as used in the configuration.
     */
    const std::string& type() const override { return type_; }

    /// Register the extreme wave event into the registry so it can be used in the plugin.
    static struct Registrar {
        Registrar() {
            ExtremeEventRegistry::instance().registerEvent(
                type_, [](const eckit::LocalConfiguration& config, plume::data::ModelDataView& modelData,
                          const std::vector<int>& coarseMapping) {
                    return std::make_unique<ExtremeWave>(config, modelData, coarseMapping);
                });
        }
    } registrar;
};