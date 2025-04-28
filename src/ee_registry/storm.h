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

#include "eckit/config/LocalConfiguration.h"
#include "plume/data/ModelData.h"

#include "ee_registry.h"

/**
 * @class Storm
 * @brief This event represents storm from wind components averaged over a time window.
 *
 * See README for configuration guidelines.
 */
class Storm final : public ExtremeEvent {
private:
    static const std::string type_;
    std::string description_;

    unsigned int timeWindow_;
    size_t ntimeSteps_;

    uint16_t windSpeedCutout_;
    /**
     * This array stores the average wind speed over coarse cells.
     * Time steps are contiguous in memory: `{W_i,t, W_j,t, W_i,t-1, W_j,t-1...}`.
     * This allows to easily slide the values for entire time steps.
     */
    std::unique_ptr<uint16_t[]> avgWindSpeeds_;
    
    size_t nCells_;
    /**
     * [{4, 10},{7, 11}, {19,10}] means that global cell 4 corresponds to index 0 of avgWindSpeeds_ and contains 10 grid
     * points. This value might be different for each cell because the current partition might be owning only a subset
     * of the points in the cells, or because of the coarsening strategy. The size of this mapping is `nCells_`.
     */
    const std::vector<std::pair<int, int>> cellMapping_;
    const std::vector<int>& coarseMapping_;  ///< Reference to the points to cells mapping to compute spatial averages
    

public:
    /**
     * @brief Constructs a storm event.
     *
     * In a partitioned environment only a subset of the coarsening cells belong to the current partition.
     * The indices in the coarse mapping are global, but only a subset of them will be found. Therefore, we can either
     * use a sparse wind speeds array that has zeros for indices that are not in the mapping (cells that are not
     * owned by the partition) and keep the reference to the coarse mapping, or use an array that has only the necessary
     * elements, but also store a mapping of the cell indices to the matrix indices. Here is a memory benchmark for
     * these choices:
     *
     * | Design choice                   | Memory Usage (kiloBytes)                                |
     * | (HEALPix res = 32)              | 1        | 2        | 4                      | 8 (jobs) |
     * |---------------------------------|---------------------------------------------------------|
     * | Index map + small array         | 497 + 98 | 250 + 50 | 126 to 157 + 25 to 26  | 78 + 13  |
     * | Array w. max cell index as size | 98       | 98       | 75 to 98               | 86 to 98 |
     * | Index vector + small array      | 49 + 98  | 25 + 50  | 13 + 26                | 7 + 13   |
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
     * This event checks if the 100m wind speed averaged on the coarse mapping exceeds a configured threshold
     * over a configured time window.
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