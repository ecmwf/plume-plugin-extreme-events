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
#include <cstdint>
#include <deque>
#include <memory>
#include <string>

#include "eckit/config/LocalConfiguration.h"
#include "plume/data/ModelData.h"

#include "ee_registry.h"

/**
 * @class Storm
 * @brief This event represents storm from wind components over a time window.
 *
 * See README for configuration guidelines.
 */
class Storm final : public ExtremeEvent {
private:
    static const std::string type_;
    std::string description_;

    unsigned int timeWindow_;
    size_t ntimeSteps_;

    FIELD_TYPE_REAL windSpeedCutout_;
    /**
     * This array stores the wind speeds of the original grid points.
     * Time steps have contiguous indices: `{W_i,t, W_j,t, W_i,t-1, W_j,t-1...}`.
     * This allows to easily slide the values for entire time steps.
     */
    std::deque<FIELD_TYPE_REAL> windSpeeds_;

    const std::vector<int>& coarseMapping_;  ///< Reference to the points to cells mapping


public:
    /**
     * @brief Constructs a storm event.
     * 
     * This event stores the wind speed for a certain number of time steps as its detection is based on temporal trends.
     * The values are stored as doubles or floats to maintain the full precision, but there is a commit with a version
     * of this event using 2-byte integers, as the wind can safely be represented on uint16_t if only two decimals of
     * precision are kept.
     * @warning Only GRIB1 fields 100u and 100v are used in this event for now. It will be migrated to GRIB2 later on.
     *
     * @param config The configuration of the event, mainly consisting of parameters for cutout speed and time window.
     * @param modelData The model data passed through Plume.
     * @param coarseMapping The mapping to use to coarsen the detection data.
     */
    Storm(const eckit::LocalConfiguration& config, plume::data::ModelData& modelData,
          const std::vector<int>& coarseMapping);

    /**
     * @brief Detects storms using the definition below.
     *
     * This event checks if the 100m wind speed exceeds a configured threshold over a configured time window.
     * Each coarse cell value is the maximum value of the temporal average of its original grid points.
     *
     * @param modelData The model data that contains the wind fields to run detection on.
     *
     * @return The detection result.
     *         n.b.: single element as the event allows to configure a single time window and wind speed cutout.
     */
    std::vector<ExtremeEvent::DetectionData> detect(plume::data::ModelData& modelData) override;

    /// Register the storm event into the registry so it can be used in the plugin.
    static struct Registrar {
        Registrar() {
            ExtremeEventRegistry::instance().registerEvent(
                type_, [](const eckit::LocalConfiguration& config, plume::data::ModelData& modelData,
                          const std::vector<int>& coarseMapping) {
                    return std::make_unique<Storm>(config, modelData, coarseMapping);
                });
        }
    } registrar;
};