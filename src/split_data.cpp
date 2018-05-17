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
#include <json.hpp>
#include "network_compiler.h"

using structured_bn::Network;
int main(int argc, const char *argv[]) {
  const char* sbn_filename = argv[1];
  const char* google_data_filename = argv[2];
  const char* cab_data_filename = argv[3];
  BinaryData* google_data = BinaryData::ReadSparseDataJsonFile(google_data_filename);
  BinaryData* cab_data = BinaryData::ReadSparseDataJsonFile(cab_data_filename);
  Network* network = Network::GetNetworkFromSpecFile(sbn_filename);
  auto google_hm_data = new BinaryData();
  google_hm_data->set_variable_size((uint32_t) google_data->variable_size());
  auto google_non_hm_data = new BinaryData();
  google_non_hm_data->set_variable_size((uint32_t) google_data->variable_size());
  auto cab_hm_data = new BinaryData();
  cab_hm_data->set_variable_size((uint32_t) cab_data->variable_size());
  auto cab_non_hm_data = new BinaryData();
  cab_non_hm_data->set_variable_size((uint32_t) cab_data->variable_size());
  std::bitset<MAX_VAR> mask;
  mask.set();
  auto status = 0;
  std::cout << "Loading google routes " << std::endl;
  for (auto example : google_data->data()){
    status += example.second;
    if (network->IsModel(mask, example.first)){
      for (auto i = 0; i < example.second; ++i){
        google_hm_data->AddRecord(example.first);
      }
    }else{
      google_non_hm_data->AddRecord(example.first);
    }
    std::cout << "Status " << status << " / " << google_data->data_size() << std::endl;
  }
  status = 0;
  std::cout << "Loading cab routes " << std::endl;
  for (auto example : cab_data->data()){
    status += example.second;
    if (network->IsModel(mask, example.first)){
      for (auto i = 0; i < example.second; ++i){
        cab_hm_data->AddRecord(example.first);
      }
    }else{
      cab_non_hm_data->AddRecord(example.first);
    }
    std::cout << "Status " << status << " / " << cab_data->data_size() << std::endl;
  }
  std::string google_hm_data_filename = std::string(google_data_filename)+ "_ok";
  std::string google_non_hm_data_filename = std::string(google_data_filename) + "_bad";
  std::string cab_hm_data_filename = std::string(cab_data_filename) + "_ok";
  std::string cab_non_hm_data_filename = std::string(cab_data_filename) + "_bad";
  google_hm_data->WriteFile(google_hm_data_filename.c_str());
  google_non_hm_data->WriteFile(google_non_hm_data_filename.c_str());
  cab_hm_data->WriteFile(cab_hm_data_filename.c_str());
  cab_non_hm_data->WriteFile(cab_non_hm_data_filename.c_str());
}
