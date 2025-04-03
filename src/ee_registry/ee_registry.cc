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
#include <functional>
#include <memory>

#include "ee_registry.h"

void ExtremeEventRegistry::registerEvent(const std::string& eventName, ExtremeEventFactory factory) {
    registry[eventName] = factory;
}

std::unique_ptr<ExtremeEvent> ExtremeEventRegistry::createEvent(const std::string& eventName,
                                                                const eckit::LocalConfiguration& config) {
    auto it = registry.find(eventName);
    ASSERT_MSG(it != registry.end(), "Event '" + eventName + "' is not in the registry, please fix or remove.");
    return it->second(config);
}

ExtremeEventRegistry& ExtremeEventRegistry::instance() {
    static ExtremeEventRegistry instance;
    return instance;
}
