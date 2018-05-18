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
  const char *google_training_filename = argv[2];
  const char *google_testing_filename = argv[3];
  const char *cab_training_filename = argv[4];
  const char *cab_testing_filename = argv[5];
  int batch_size = atoi(argv[6]);
  BinaryData *google_training_data = BinaryData::ReadSparseDataJsonFile(google_training_filename);
  BinaryData *cab_training_data = BinaryData::ReadSparseDataJsonFile(cab_training_filename);
  Network *google_network = Network::GetNetworkFromSpecFile(sbn_filename);
  google_network->LearnParametersUsingLaplacianSmoothing(google_training_data, PsddParameter::CreateFromDecimal(1));
  Network *cab_network = Network::GetNetworkFromSpecFile(sbn_filename);
  cab_network->LearnParametersUsingLaplacianSmoothing(cab_training_data, PsddParameter::CreateFromDecimal(1));
  BinaryData *google_testing_data = BinaryData::ReadSparseDataJsonFile(google_testing_filename);
  BinaryData *cab_testing_data = BinaryData::ReadSparseDataJsonFile(cab_testing_filename);
  uintmax_t correct_prediction = 0;
  uintmax_t total_prediction = 0;
  uintmax_t status = 0;
  std::vector<std::bitset<MAX_VAR>> google_test_data;
  for (const auto &data_pair : google_testing_data->data()) {
    for (auto i = 0; i < data_pair.second; ++i) {
      google_test_data.push_back(data_pair.first);
    }
  }
  std::vector<std::bitset<MAX_VAR>> cab_test_data;
  for (const auto &data_pair : cab_testing_data->data()) {
    for (auto i = 0; i < data_pair.second; ++i) {
      cab_test_data.push_back(data_pair.first);
    }
  }
  std::random_shuffle(google_test_data.begin(), google_test_data.end());
  std::random_shuffle(cab_test_data.begin(), cab_test_data.end());
  uintmax_t accurate = 0;
  uintmax_t total = 0;
  uintmax_t wrong_google = 0;
  uintmax_t wrong_cab = 0;
  for (auto i = 0; i < google_test_data.size(); i += batch_size) {
    /*
    BinaryData cur_data;
    cur_data.set_variable_size(google_testing_data->variable_size());
    for (auto j = i; j < i + batch_size; ++j) {
      cur_data.AddRecord(google_test_data[j]);
    }
    Probability google_ll = google_network->CalculateProbability(&cur_data);
    Probability cab_ll = cab_network->CalculateProbability(&cur_data);*/
    Probability google_ll = Probability::CreateFromDecimal(1);
    Probability cab_ll = Probability::CreateFromDecimal(1);
    for (auto j = i ; j < i + batch_size; ++ j){
      google_ll = google_ll + google_network->EvaluateCompleteInstantiation(google_test_data[j]);
      cab_ll = cab_ll + cab_network->EvaluateCompleteInstantiation(google_test_data[j]);
    }
    if (google_ll > cab_ll) {
      accurate += 1;
    } else {
      wrong_google += 1;
    }
    total += 1;
    std::cout << "Correct : " << accurate << " Wrong Google : " << wrong_google << " Wrong Cab : " << wrong_cab
              << " Total : " << total << std::endl;
  }

  for (auto i = 0; i < cab_test_data.size(); i += batch_size) {
    /*
    BinaryData cur_data;
    cur_data.set_variable_size(google_testing_data->variable_size());
    for (auto j = i; j < i + batch_size; ++j) {
      cur_data.AddRecord(cab_test_data[j]);
    }
    Probability google_ll = google_network->CalculateProbability(&cur_data);
    Probability cab_ll = cab_network->CalculateProbability(&cur_data);*/
    Probability google_ll = Probability::CreateFromDecimal(1);
    Probability cab_ll = Probability::CreateFromDecimal(1);
    for (auto j = i ; j < i + batch_size; ++ j){
      google_ll = google_ll + google_network->EvaluateCompleteInstantiation(cab_test_data[j]);
      cab_ll = cab_ll + cab_network->EvaluateCompleteInstantiation(cab_test_data[j]);
    }
    if (google_ll < cab_ll) {
      accurate += 1;
    } else {
      wrong_cab += 1;
    }
    total += 1;
    std::cout << "Correct : " << accurate << " Wrong Google : " << wrong_google << " Wrong Cab : " << wrong_cab
              << " Total : " << total << std::endl;
  }
  std::cout << "Final Result : " << "Correct : " << accurate << " Wrong Google : " << wrong_google << " Wrong Cab : "
            << wrong_cab << " Total : " << total << std::endl;


  /*
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
   */
}