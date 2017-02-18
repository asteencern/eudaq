#ifndef HGCAL_parameters
#define HGCAL_parameters

#include <vector>
#include <string>

namespace hgcalParameters {
  // std::map<std::string, int> ipMap = {{"192.168.0.3", 1}, {"192.168.0.4", 2}, {"192.168.0.5", 3}};
  std::vector<std::string> RPi_ipMap = {"128.141.89.196", "128.141.89.191", "128.141.89.139"};
  const uint32_t nRPis = 3;
}

#endif
