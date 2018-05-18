//
// Created by Jason Shen on 5/17/18.
//
extern "C" {
#include <sddapi.h>
}
#include <iostream>
#include <network.h>
#include <src/optionparser.h>
#include <cassert>
#include <chrono>
#include <fstream>
#include <algorithm>
#include <json.hpp>
#include "network_compiler.h"

using structured_bn::Network;
int main(int argc, const char *argv[]) {
  const char *sbn_filename = argv[1];
  Network * network = Network::GetNetworkFromSpecFile(sbn_filename);
  std::cout << "SBN Size : " << network->GetParameterCount() << std::endl;
}