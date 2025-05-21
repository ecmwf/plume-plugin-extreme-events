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

#include "eckit/config/LocalConfiguration.h"
#include "plume/data/ModelData.h"

#include "ee_base.h"

/**
 * @class ExtremeEventRegistry
 * @brief This class is a registry of the extreme event classes (singleton). It allows the client to instantiate extreme
 * event objects from their name. Each extreme event is responsible for registering itself to the registry.
 */
class ExtremeEventRegistry {
public:
    using ExtremeEventFactory = std::function<std::unique_ptr<ExtremeEvent>(
        const eckit::LocalConfiguration& config, plume::data::ModelData&, const std::vector<int>& coarseMapping)>;

    void registerEvent(const std::string& eventName, ExtremeEventFactory factory);
    std::unique_ptr<ExtremeEvent> createEvent(const eckit::LocalConfiguration& config,
                                              plume::data::ModelData& modelData, const std::vector<int>& coarseMapping);
    static ExtremeEventRegistry& instance();

    ExtremeEventRegistry() = default;
    /// Singletons should not be cloneable.
    ExtremeEventRegistry(const ExtremeEventRegistry&) = delete;
    /// Singletons should not be assignable.
    ExtremeEventRegistry& operator=(const ExtremeEventRegistry&) = delete;
    /// Explicitly delete the moving operations.
    ExtremeEventRegistry(ExtremeEventRegistry&&)            = delete;
    ExtremeEventRegistry& operator=(ExtremeEventRegistry&&) = delete;

private:
    std::map<std::string, ExtremeEventFactory> registry;
};