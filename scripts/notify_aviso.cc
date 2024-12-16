#include "notification.h"
#include <cstdlib>
#include <ctime>
#include <iostream>

#include "atlas/util/Point.h"

using namespace ExtremeEventPlugin;

int setEnvironmentVariables() {
  // Variables related to model run used for sending aviso notifications
  time_t now = time(0);
  struct tm t;
  localtime_r(&now, &t);
  char dateStr[9];
  strftime(dateStr, sizeof(dateStr), "%Y%m%d", &t);
  std::map<std::string, std::string> vars = {{"CLASS", "d1"},
                                             {"TYPE", "fc"},
                                             {"EXPVER", "0001"},
                                             {"DATE", dateStr},
                                             {"TIME", "0000"}};
  for (const auto &[key, value] : vars) {
    if (setenv(key.c_str(), value.c_str(), 1) != 0) {
      return 1;
    }
  }
  return 0;
}

int main() {

  // Set environment variables
  if (setEnvironmentVariables() != 0) {
    std::cout << "Something went wrong with your environment variables, please "
                 "check..."
              << std::endl;
    return 1;
  }

  AvisoNotificationHandler notificationHandler(
      "https://geoaviso-dev.lumi.apps.dte.destination-earth.eu",
      "/notify/mars");

  std::string data = R"({"hello": "Clara"})";
  std::vector<atlas::PointLonLat> polygon = {
      atlas::PointLonLat{16.9, 250.3}, atlas::PointLonLat{14.4, 247.4},
      atlas::PointLonLat{14.4, 253.1}, atlas::PointLonLat{12.0, 250.3}};

  std::cout << notificationHandler.send(data, polygon) << std::endl;

  return 0;
}