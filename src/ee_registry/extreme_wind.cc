#include "extreme_wind.h"
#include "atlas/array.h"
#include "atlas/field.h"
#include "atlas/functionspace.h"
#include "eckit/exception/Exceptions.h"

const std::string ExtremeWind::type = "extreme_wind";
std::vector<std::string> ExtremeWind::requiredParams{"100u", "100v"};

// Construct the ExtremeWind object from the configuration
ExtremeWind::ExtremeWind(const eckit::LocalConfiguration &config)
    : ExtremeEvent(config) {
  // First perform a sanity check to make sure the params in the config
  // match the params in the implementation
  std::vector<std::string> configRequiredParams =
      config.getStringVector("required_params");
  std::sort(configRequiredParams.begin(), configRequiredParams.end());
  std::sort(requiredParams.begin(), requiredParams.end());
  if (configRequiredParams != requiredParams) {
    throw eckit::BadValue(
        "Configuration and " + type +
            " implementation have different required parameters",
        Here());
  }

  // Retrieve the wind intervals to run detection on
  // and their description if applicable
  std::vector<eckit::LocalConfiguration> windEventList =
      config.getSubConfigurations("instances");
  for (const auto &eventConfig : windEventList) {
    intervals.push_back(
        {eventConfig.getDouble("lower_bound"),
         eventConfig.getDouble("upper_bound"),
         eventConfig.getString("description") + " (lower bound : " +
             std::to_string(eventConfig.getDouble("lower_bound")) +
             " , upper bound : " +
             std::to_string(eventConfig.getDouble("upper_bound")) + " )"});
  }
}

std::vector<std::tuple<std::string, std::vector<int>>>
ExtremeWind::detect(plume::data::ModelData &modelData) {
  std::vector<std::tuple<std::string, std::vector<int>>> ee_points;
  for (size_t idx_int = 0; idx_int < intervals.size(); idx_int++) {
    ee_points.push_back({intervals[idx_int].description, {}});
  }

  // Retrieve wind fields
  // TODO: could the level be configured to potentially run at multiple heights
  // ?
  atlas::Field u_field = modelData.getAtlasFieldShared("100u");
  atlas::Field v_field = modelData.getAtlasFieldShared("100v");

  auto ifs_halo =
      atlas::array::make_view<int, 1>(u_field.functionspace().ghost());
  atlas::array::ArrayView<const FIELD_TYPE_REAL, 2> arrU =
      atlas::array::make_view<const FIELD_TYPE_REAL, 2>(u_field);
  atlas::array::ArrayView<const FIELD_TYPE_REAL, 2> arrV =
      atlas::array::make_view<const FIELD_TYPE_REAL, 2>(v_field);

  for (atlas::idx_t idx = 0; idx < arrU.size(); idx++) {

    // Skip the halo
    if (ifs_halo(idx) > 0) {
      continue;
    }

    FIELD_TYPE_REAL valU = *(arrU.data() + idx);
    FIELD_TYPE_REAL valV = *(arrV.data() + idx);
    FIELD_TYPE_REAL windMagnitude = std::sqrt(valU * valU + valV * valV);

    for (size_t idx_int = 0; idx_int < intervals.size(); idx_int++) {
      if (windMagnitude < intervals[idx_int].lBound) {
        continue;
      }
      if (intervals[idx_int].lBound > intervals[idx_int].uBound ||
          windMagnitude < intervals[idx_int].uBound) {
        //  /!\ if the upper bound is lower than the lower bound then we check
        //  only if the wind exceeds the lower bound
        // if the upper bound is higher, then we check for belonging
        std::get<1>(ee_points[idx_int]).push_back(idx);
      }
    }
  }
  return ee_points;
}

ExtremeWind::Registrar ExtremeWind::registrar;
