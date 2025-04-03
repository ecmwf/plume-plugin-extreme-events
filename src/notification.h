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
#include <map>
#include <string>
#include <vector>

#include "atlas/util/Point.h"

namespace ExtremeEventPlugin {

/**
 * @class AvisoNotificationHandler
 * @brief This class handles Aviso notification sending from the extreme plugin.
 *
 * It manages the notification schema `[key,value]` pairs which should  be fixed at simulation startup, and provides
 * a public interface to send notifications to an Aviso server (no particular check is performed on the
 * configured server url).
 */
class AvisoNotificationHandler {

private:
    std::string urlBase_;    ///< The Aviso server url.
    std::string urlNotify_;  ///< The notification endpoint.

    /**
     * @brief The Aviso MARS schema required keys.
     *
     * These fields are all required for the notifications to be sent correctly to the Aviso server.
     * They are listed under the geoaviso config yaml file. It may be better in the future to gather these
     * required fields in an automatic way which would be more robust to Aviso schema modifications.
     * @todo Update the payload or schema to allow for event-specific data (type, severity etc.)
     *
     * @note As a reminder, all these fields (`class`, `type`, `expver`, `date`, and `time`) are fixed for a given
     *       model run. `date` and `time` correspond to the start time and date of the simulation. Hourly outputs
     *       are archived under the `step` MARS key, which is not used in this context since we process data on
     *       internal, not output, time steps. The simulation time is, for now, conveyed in the notification
     *       payload, and depend on the internal time step and process delta time.
     * @todo It is important that users consuming the notifications are able to retrieve the boundary condition
     *       data from the output closest to the simulation time at which extreme event signals were detected.
     *       While the plugin is able to share all the detection information in the payload, this question remains
     *       open as to how exactly it should be formatted.
     * @warning The plugin should not rely on environment variables to populate the schema, but it is not decided yet
     *          how the run information should be exposed to the plugin (likely through the Plume model data).
     */
    std::map<std::string, std::string> schemaData_ = {
        {"class", ""}, {"type", ""}, {"expver", ""}, {"date", ""}, {"time", ""}};

    /**
     * @brief Encodes the given parameters and polygon string into a valid Aviso notification payload.
     *
     * This function does not carry on any modification of the polygon string. It assumes whatever string is passed
     * describes a polygon that Aviso can support.
     *
     * @param polygon The string describing the polygon as value for the `polygon` Aviso key.
     */
    std::string urlEncode(const std::string polygon);

    /**
     * @brief Preprocess the vector of Atlas points describing a polygon, then encodes using the above method.
     *
     * This method processes a vector of Atlas points into a polygon string that follows this format:
     * `"lat1,lon1,lat2,lon2,...,lat1,lon1"`. It is anticipated that Aviso might support geo hashes to describe
     * polygons in the future, which would remove the need for this overloaded method, as HEALPix cell hashes
     * might be directly passed to the `polygon` key.
     *
     * @param polygon The verticies of the polygon as an Atlas point vector.
     */
    std::string urlEncode(const std::vector<atlas::PointLonLat>& polygon);

public:
    /// Default constructor
    AvisoNotificationHandler() = default;

    /**
     * @brief Constructs an Aviso notification handler.
     *
     * This sets the urls and experiment schemas to use for all Aviso notifications to be sent by the plugin.
     *
     * @param base The Aviso server url.
     * @param notify The notification endpoint.
     */
    AvisoNotificationHandler(const std::string& base, const std::string& notify);

    /**
     * @brief Sets the Aviso schema data from environment variables.
     *
     * @warning This function relies on the assumption that Aviso schema keys in upper case match exactly the name of
     *          the environment variable that stores this piece of information, e.g., `class` key can be found in
     *          envvar `CLASS`. This might need to be revised depending on the environment, model, or Aviso schema.
     * 
     * @throws eckit::BadParameter if one of the schema keys does not have a matching environment variable.
     */
    void setSchemaData();

    /**
     * @brief Sends a notification to the Aviso server with the given payload and polygon.
     *
     * The signature of this method requires a vector with each individual polygon vertex coordinates for now.
     * Overloads can be introduced if Aviso supports polygons defined differently in the future, such as geohashes.
     *
     * @param payload The payload of the notification. It can be metadata describing the extreme event notified.
     * @param polygon The polygon where the extreme event signal has been detected as Atlas points.
     */
    int send(const std::string payload, const std::vector<atlas::PointLonLat>& polygon);
};

}  // namespace ExtremeEventPlugin
