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
#include <memory>
#include <string>
#include <unordered_map>

#include "eckit/config/LocalConfiguration.h"
#include "plume/data/ModelData.h"

#include "ee_registry.h"

/**
 * @class WindDrought
 * @brief This event represents wind drought (long period of no wind) from wind components averaged over a time window.
 *
 * See README for configuration guidelines.
 */
class WindDrought final : public ExtremeEvent {
private:
    static const std::string type_;
    std::string description_;

    unsigned int timeWindow_;
    uint16_t ntimeSteps_;

    FIELD_TYPE_REAL windSpeedCutout_;
    /**
     * This map stores the number of time steps with low wind speed over coarse cells.
     * The first element of the values is the step count, and the second is the number of points in the cells to help
     * with spatial averages.
     */
    std::unordered_map<int, std::pair<uint16_t, size_t>> windDroughtSteps_;
    const std::vector<int>& coarseMapping_;  ///< Reference to the points to cells mapping to compute spatial averages


public:
    /**
     * @brief Constructs a wind drought event (or prolonged period of no wind event).
     *
     * This event does not need to store wind values across steps because it is not computing a temporal average.
     * It stores only a counter of the number of time steps below the threshold, which it compares to the number of
     * time steps required to trigger the detection.
     *
     * @param config The configuration of the event, mainly consisting of parameters for cutout speed and time window.
     * @param modelData The model data passed through Plume.
     * @param coarseMapping The mapping to use to coarsen the detection data.
     */
    WindDrought(const eckit::LocalConfiguration& config, plume::data::ModelData& modelData,
                const std::vector<int>& coarseMapping);

    /**
     * @brief Detects wind droughts using the definition below.
     *
     * This event checks if the 100m wind speed averaged on the coarse mapping remains under a given threshold
     * for a configured time window.
     *
     * @param modelData The model data that contains the wind fields to run detection on.
     *
     * @return The detection result.
     *         n.b.: single element as the event allows to configure a single time window and wind speed cutout.
     */
    std::vector<ExtremeEvent::DetectionData> detect(plume::data::ModelData& modelData) override;

    /// Register the wind drought event into the registry so it can be used in the plugin.
    static struct Registrar {
        Registrar() {
            ExtremeEventRegistry::instance().registerEvent(
                type_, [](const eckit::LocalConfiguration& config, plume::data::ModelData& modelData,
                          const std::vector<int>& coarseMapping) {
                    return std::make_unique<WindDrought>(config, modelData, coarseMapping);
                });
        }
    } registrar;
};