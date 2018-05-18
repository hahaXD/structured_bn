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
  const char* google_training_filename = argv[2];
  const char* google_testing_filename = argv[3];
  const char* cab_training_filename = argv[4];
  const char* cab_testing_filename = argv[5];
  BinaryData* google_training_data = BinaryData::ReadSparseDataJsonFile(google_training_filename);
  BinaryData* cab_training_data = BinaryData::ReadSparseDataJsonFile(cab_training_filename);
  Network* google_network = Network::GetNetworkFromSpecFile(sbn_filename);
  google_network->LearnParametersUsingLaplacianSmoothing(google_training_data, PsddParameter::CreateFromDecimal(1));
  Network* cab_network = Network::GetNetworkFromSpecFile(sbn_filename);
  cab_network->LearnParametersUsingLaplacianSmoothing(cab_training_data, PsddParameter::CreateFromDecimal(1));
  BinaryData* google_testing_data = BinaryData::ReadSparseDataJsonFile(google_testing_filename);
  BinaryData* cab_testing_data = BinaryData::ReadSparseDataJsonFile(cab_testing_filename);
  uintmax_t correct_prediction = 0;
  uintmax_t total_prediction = 0;
  uintmax_t status = 0;
  for (const auto& data_pair : google_testing_data->data()){
    status += data_pair.second;
    std::cout << "Processing " << status << " / " << google_testing_data->data_size() << std::endl;
    Probability google_prob = google_network->EvaluateCompleteInstantiation(data_pair.first);
    Probability cab_prob = cab_network->EvaluateCompleteInstantiation(data_pair.first);
    if (google_prob > cab_prob){
      correct_prediction += data_pair.second;
    }
    total_prediction += data_pair.second;
    std::cout << "Prediction " << correct_prediction << " / " << total_prediction << " correct." << std::endl;
  }
  status = 0;
  for (const auto& data_pair : cab_testing_data->data()){
    status += data_pair.second;
    std::cout << "Processing " << status << " / " << cab_testing_data->data_size() << std::endl;
    Probability google_prob = google_network->EvaluateCompleteInstantiation(data_pair.first);
    Probability cab_prob = cab_network->EvaluateCompleteInstantiation(data_pair.first);
    if (google_prob < cab_prob){
      correct_prediction += data_pair.second;
    }
    total_prediction += data_pair.second;
    std::cout << "Prediction " << correct_prediction << " / " << total_prediction << " correct." << std::endl;
  }
  std::cout << "Result : " << correct_prediction  << " / " << total_prediction << std::endl;
}