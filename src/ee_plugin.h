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
#include "atlas/grid.h"
#include "eckit/config/Configuration.h"
#include "eckit/config/LocalConfiguration.h"
#include "plume/Plugin.h"
#include "plume/PluginCore.h"

#include "ee_registry/ee_registry.h"
#include "git_sha1.h"
#include "notification.h"
#include "version.h"

namespace ExtremeEventPlugin {

/**
 * @class EEPluginCore
 * @brief Core extreme event detection Plume plugin functionalities.
 *
 * This class manages a collection of extreme events that it sets up from a Plume plugin configuration.
 * The outputs of this plugin are Aviso notifications, which it either sends to an actual Aviso server
 * or prints to the console for now (if dev mode is active). It also manages the representation of the extreme events.
 * Since sending Aviso notification for each single model grid point firing for an extreme event is impractical,
 * this class determines how regions of firing points are coarsened or aggregated to send less and more meaningful
 * extreme events polygons to Aviso. This coarsening is currently based on a HEALPix mesh of configurable
 * resolution (non nested for now) for the following reasons:
 *      - ease of chosing the grain of the coarsening
 *      - polygons can be represented by strings if Aviso introduces support for geo notifications
 *      - nesting can potentially offer more complex coarsening capabilities
 */
class EEPluginCore final : public plume::PluginCore {
public:
    /**
     * @brief Constructs a EEPluginCore object.
     *
     * Retrieves the plugin options from the configuration and sets up an Aviso handler if applicable.
     *
     * @param conf The plugin configuration defining extreme events to load and their options.
     *
     * @note The constructor aborts the execution if necessary pieces of configuration are missing.
     */
    EEPluginCore(const eckit::Configuration& conf);

    /**
     * @brief Sets up the necessary variables to run the plugin.
     *
     * 1. Creates an instance of each extreme event enabled in the configuration from the extreme event registry.
     * 2. Creates the mapping between model grid points and HEALPix cells and vertices.
     *
     * @note This phase fails if the configuration does not contain the necessary information.
     */
    void setup() override;

    /**
     * @brief Runs the plugin.
     *
     * 1. Runs the detection method of each of the extreme event instances. See registry documentation for more
     *    details on the output structure.
     * 2. From the raw detection output, extract the extreme event polygons (contiguous firing HEALPix cells).
     *    n.b.: cells are considered contiguous if they have *one or more* vertices in common.
     *    See `healpix_utils` documentation for more details, and currently not handle edge cases.
     * 3. Send notifications to Aviso. A notification consists of a single polygon for a single event.
     *    If there are two events, and for each two polygons were extracted, it will result in four notifications.
     *
     * @todo Properly provide an alternative to Aviso notifications for local runs.
     * @todo Refine the content of the Aviso payload to contain more detailed information about the signal and how
     *       to retrieve the closest data for boundary conditions of downstream models.
     *
     * @warning Partitioning is not handled here. Each partition will send a separate notification, even if an event
     *          polygon spans across multiple partitions. Aggregation is expected to happen on the Aviso server side,
     *          but is not supported at the moment. This may be reconsidered in the future.
     */
    void run() override;

    /// Returns the plugin core type, for Plume usage.
    constexpr static const char* type() { return "ee-plugincore"; }

private:
    std::vector<eckit::LocalConfiguration> extremeEventConfig_;
    std::vector<std::unique_ptr<ExtremeEvent>>
        extremeEvents_;  ///< A single plugin manages all instances of different extreme events

    AvisoNotificationHandler notificationHandler_;
    bool enableNotification_;

    int healpixRes_;
    std::vector<int> Point2HPcell_;                                ///< Mapping from point index to HEALPix cell index
    std::vector<std::vector<atlas::PointLonLat>> HPcell2polygon_;  ///< Mapping from HEALPix cell index to vertices

    /**
     * @brief Fills out the mapping matrices for coarsening regions where an extreme event is detected.
     *
     * These mapping matrices are contain only the subset of points managed by the partition. But each partition
     * creates a global HEALPix mesh to perform the mapping.
     */
    void setHEALPixMapping();

    /**
     * @brief Returns the MARS value string representing the sub-hourly model step.
     *
     * @warning It is important to note that this step is only internal, there is no guarantee that it will
     *          correspond to an actual output step. Data from this step may not be retrievable after the run.
     */
    std::string modelStepStr();
};


/**
 * @class EEPlugin
 * @brief Plume plugin for extreme event detection.
 */
class EEPlugin : public plume::Plugin {
public:
    EEPlugin();

    /**
     * @brief Negotiates with the Plume managers to determine whether this plugin can be loaded.
     *
     * This negotiation only requires the internal model step number and time explicitely to derive the time in the run.
     * The required parameters for each extreme event are negotiated through the configuration. This plugin can be
     * loaded if at least one of the enabled extreme events has its required parameters offered by the model.
     *
     * @return The protocol object for the negotiation.
     */
    plume::Protocol negotiate() override {
        plume::Protocol protocol;
        protocol.requireInt("NSTEP");
        protocol.requireDouble("TSTEP");
        protocol.requireInt("NFLEVG");
        return protocol;
    }

    // Return the static instance
    static const EEPlugin& instance();
    std::string version() const override { return version(); }

    std::string gitsha1(unsigned int count) const override { return gitsha1(7); }

    virtual std::string plugincoreName() const override { return EEPluginCore::type(); }
};

}  // namespace ExtremeEventPlugin