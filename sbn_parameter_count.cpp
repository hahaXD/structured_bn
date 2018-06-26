//
// Created by Jason Shen on 5/17/18.
//
extern "C" {
#include <sdd/sddapi.h>
}
#include <algorithm>
#include <cassert>
#include <chrono>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <structured_bn/network.h>
#include <structured_bn/network_compiler.h>
#include <util/optionparser.h>

using structured_bn::Network;
int main(int argc, const char *argv[]) {
  const char *sbn_filename = argv[1];
  Network *network = Network::GetNetworkFromSpecFile(sbn_filename);
  std::cout << "SBN Size : " << network->GetParameterCount() << std::endl;
}
