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
#ifndef EE_BASE_H
#define EE_BASE_H
#include <optional>
#include <string>
#include <vector>

#include "plume/data/ModelDataView.h"

#include "../plugin_types.h"

/**
 * @class ExtremeEvent
 * @brief Base class for representing an extreme event.
 */
class ExtremeEvent {
private:
    const std::string type_ = "ExtremeEvent";

    /**
     * @struct RequiredModelData
     * @brief A representation of the `required_params` configuration key for validation or retrieval purposes.
     *
     * @note The current implementation of the plugin only supports height levels for wind fields. If this were to
     * evolve due to Plume field derivation capabilities expanding, this struct and the plugin setup will need to be
     * refactored.
     */
    struct RequiredModelData {
        std::vector<std::string> requiredParams_;
        std::vector<std::string> requiredFields_;
        std::optional<unsigned int> heightLevel_;
    };

    RequiredModelData requiredModelData_;

public:
    /// Default constructor.
    ExtremeEvent() = default;

    /**
     * @brief Constructs an extreme event object.
     *
     * All events should have a `required_params` key, even if empty, even though it is unlikely that an event
     * does not require anything from the model data. Each event is responsible for ensuring it accepts the
     * provided configuration.
     *
     * @param config The configuration for the extreme event. Each event has a different set of options, see
     *               their documentation for more details.
     * @param type The type of extreme event.
     */
    ExtremeEvent(const eckit::LocalConfiguration& config, const std::string& type) {
        std::optional<unsigned int> heightLevel = std::nullopt;
        for (const auto& param : config.getSubConfigurations("required_params")) {
            // Spot configuration malformations for height levels at setup time
            if (heightLevel.has_value() && param.has("height") && *heightLevel != param.getUnsigned("height")) {
                throw eckit::BadValue("Several heights found for " + config.getString("name") + " event", Here());
            }
            if (!heightLevel.has_value() && param.has("height")) {
                heightLevel = param.getUnsigned("height");
            }
            // Keep track of the required fields and parameters for validation and later retrieval during detection
            if (param.getString("type") == "ATLAS_FIELD") {
                requiredModelData_.requiredFields_.push_back(param.getString("name"));
            }
            else {
                requiredModelData_.requiredParams_.push_back(param.getString("name"));
            }
        }
        requiredModelData_.heightLevel_ = heightLevel;
        ASSERT_MSG(!requiredModelData_.requiredFields_.empty(),
                   "Event '" + type + "' has no configured required Atlas fields, detection will fail.");
    };

    /// Virtual destructor.
    virtual ~ExtremeEvent() = default;

    /**
     * @brief A structure that represents the result of detecting the extreme event.
     *
     * This information can later be used to build an Aviso request allowing the receiver to create a MARS request
     * to retrieve the relevant data regarding the detected event.
     */
    struct DetectionData {
        std::set<int> detectedCells;
        std::string description, param, levtype, levelist;
    };

    /**
     * @brief Runs the detection algorithm on the provided model data (once per model internal time step).
     *
     * Each extreme event has to fetch in the model data the parameters and fields it requires.
     * No filtering is done prior to calling this method. Each event class is only responsible for maintaining
     * their detection algorithm. The Plume plugin is responsible for any other steps, such as notifying Aviso
     * if applicable. The result of the detection is a vector of `DetectionData` objects.
     * An extreme event object represents a single event type, however, it can be used to run multiple
     * configurations of the same event, e.g., extreme winds above 25m/s and extreme winds between 0 and 1m/s.
     * The indices can later be used by the plugin to extract the extreme event polygon, but the event objects
     * themselves only return raw detection on the original model fields.
     *
     * @param modelData The model data offered through Plume (parameters and Atlas fields).
     *
     * @return The result of the detection.
     */
    virtual std::vector<DetectionData> detect(plume::data::ModelDataView& modelData) = 0;

    /// Getters
    const std::vector<std::string>& requiredParams() const { return requiredModelData_.requiredParams_; }
    const std::vector<std::string>& requiredFields() const { return requiredModelData_.requiredFields_; }
    const std::optional<unsigned int>& heightLevel() const { return requiredModelData_.heightLevel_; }
    virtual const std::string& type() const = 0;
};

#endif  // EE_BASE_H