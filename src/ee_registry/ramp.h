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
#include <deque>
#include <memory>
#include <string>

#include "eckit/config/LocalConfiguration.h"
#include "plume/data/ModelData.h"

#include "ee_registry.h"

/**
 * @class Ramp
 * @brief This event represents ramp-ups or ramp-downs from given parameters over a time window.
 *
 * See README for configuration guidelines.
 */
class RampEvent final : public ExtremeEvent {
private:
    static const std::string type_;
    std::string descriptionUp_;
    std::string descriptionDown_;
    std::string fieldNamesStr_;

    unsigned int timeWindow_;
    size_t ntimeSteps_;

    FIELD_TYPE_REAL rampUpValue_;  ///< the units should match the params unit (user's responsibility)
    FIELD_TYPE_REAL rampDownValue_;
    bool rampUp_   = false;
    bool rampDown_ = false;
    /**
     * This array stores the values of the original grid points from the previous time steps.
     * Time steps have contiguous indices: `{W_i,t, W_j,t, W_i,t-1, W_j,t-1...}`.
     * This allows to easily slide the values for entire time steps.
     */
    std::deque<FIELD_TYPE_REAL> previousValues_;

    const std::vector<int>& coarseMapping_;  ///< Reference to the points to cells mapping


public:
    /**
     * @brief Constructs a ramp event.
     *
     * @param config The configuration of the event, mainly consisting of parameters for ramp values and time window.
     * @param modelData The model data passed through Plume.
     * @param coarseMapping The mapping to use to coarsen the detection data.
     */
    RampEvent(const eckit::LocalConfiguration& config, plume::data::ModelData& modelData,
              const std::vector<int>& coarseMapping);

    /**
     * @brief Detects ramp-ups or ramp-downs using the definition below.
     *
     * This event checks if the given parameter(s) gradient over a configured time window varies from a given threshold.
     * The sign of the ramp can be configured if the event should detect only ups, or downs or both.
     *
     * @param modelData The model data that contains the fields to run detection on.
     *
     * @return The detection result.
     *         n.b.: two elements as the event allows to configure a single set of params and threshold for up and down.
     *               Multiple ramp events can be configured if detection is expected on various params.
     */
    std::vector<ExtremeEvent::DetectionData> detect(plume::data::ModelData& modelData) override;

    /**
     * @brief Returns the type/name of the extreme event as used in the configuration.
     */
    const std::string& type() const override {
        return type_;
    }

    /// Register the ramp event into the registry so it can be used in the plugin.
    static struct Registrar {
        Registrar() {
            ExtremeEventRegistry::instance().registerEvent(
                type_, [](const eckit::LocalConfiguration& config, plume::data::ModelData& modelData,
                          const std::vector<int>& coarseMapping) {
                    return std::make_unique<RampEvent>(config, modelData, coarseMapping);
                });
        }
    } registrar;
};