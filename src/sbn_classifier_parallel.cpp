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
#include <queue>
#include <thread>
#include <mutex>
#include "network_compiler.h"

using structured_bn::Network;
template<class T>
class SafeQueue {
  std::queue<T *> q;
  std::mutex m;
 public:
  SafeQueue() {}
  void push(T *elem) {
    m.lock();
    if (elem != nullptr) {
      q.push(elem);
    }
    m.unlock();
  }
  T *next() {
    T *elem = nullptr;
    m.lock();
    if (!q.empty()) {
      elem = q.front();
      q.pop();
    }
    m.unlock();

    return elem;
  }
};
struct EvalResult {
  EvalResult(uintmax_t g_c, uintmax_t g_t, uintmax_t c_c, uintmax_t c_t)
      : google_correct(g_c), google_total(g_t), cab_correct(c_c), cab_total(c_t) {}
  uintmax_t google_correct;
  uintmax_t google_total;
  uintmax_t cab_correct;
  uintmax_t cab_total;
};
void SingleThread(Network *google_network, Network *cab_network,
                  const std::vector<std::bitset<MAX_VAR>> *google_testing_data,
                  const std::vector<std::bitset<MAX_VAR>> *cab_testing_data,
                  uintmax_t batch_size,
                  size_t offset,
                  size_t data_size, SafeQueue<EvalResult> *safe_queue) {
  std::cout << " Batch size " << batch_size << " offset " << offset << " data size " << data_size << std::endl;
  uintmax_t google_total = 0;
  uintmax_t google_correct = 0;
  for (auto i = offset; i < offset + data_size; i += batch_size) {
    Probability google_ll = Probability::CreateFromDecimal(1);
    Probability cab_ll = Probability::CreateFromDecimal(1);
    for (auto j = i; j < i + batch_size; ++j) {
      google_ll = google_ll * google_network->EvaluateCompleteInstantiation(google_testing_data->at(j));
      cab_ll = cab_ll * cab_network->EvaluateCompleteInstantiation(google_testing_data->at(j));
    }
    if (google_ll > cab_ll) {
      google_correct += 1;
    }
    google_total += 1;
    std::cout << " Google correct : " << google_correct << " Google total : " << google_total << std::endl;
  }
  uintmax_t cab_total = 0;
  uintmax_t cab_correct = 0;
  for (auto i = offset; i < offset + data_size; i += batch_size) {
    Probability google_ll = Probability::CreateFromDecimal(1);
    Probability cab_ll = Probability::CreateFromDecimal(1);
    for (auto j = i; j < i + batch_size; ++j) {
      google_ll = google_ll * google_network->EvaluateCompleteInstantiation(cab_testing_data->at(j));
      cab_ll = cab_ll * cab_network->EvaluateCompleteInstantiation(cab_testing_data->at(j));
    }
    if (google_ll < cab_ll) {
      cab_correct += 1;
    }
    cab_total += 1;
    std::cout << " Cab correct : " << cab_correct << " Cab total : " << cab_total << std::endl;
  }
  safe_queue->push(new EvalResult(google_correct, google_total, cab_correct, cab_total));
}
int main(int argc, const char *argv[]) {
  const char *google_training_filename = argv[1];
  const char *google_testing_filename = argv[2];
  const char *cab_training_filename = argv[3];
  const char *cab_testing_filename = argv[4];
  const char *sbn_filename = argv[5];
  int min_batch_size = atoi(argv[6]);
  int max_batch_size = atoi(argv[7]);
  int cores = 1 << atoi(argv[8]);
  BinaryData *google_training_data = BinaryData::ReadSparseDataJsonFile(google_training_filename);
  BinaryData *cab_training_data = BinaryData::ReadSparseDataJsonFile(cab_training_filename);
  Network *google_network = Network::GetNetworkFromSpecFile(sbn_filename);
  google_network->LearnParametersUsingLaplacianSmoothing(google_training_data, PsddParameter::CreateFromDecimal(1));
  Network *cab_network = Network::GetNetworkFromSpecFile(sbn_filename);
  cab_network->LearnParametersUsingLaplacianSmoothing(cab_training_data, PsddParameter::CreateFromDecimal(1));
  BinaryData *google_testing_data = BinaryData::ReadSparseDataJsonFile(google_testing_filename);
  BinaryData *cab_testing_data = BinaryData::ReadSparseDataJsonFile(cab_testing_filename);
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
  for (auto i = min_batch_size; i <= max_batch_size; ++i) {
    std::random_shuffle(google_test_data.begin(), google_test_data.end());
    std::random_shuffle(cab_test_data.begin(), cab_test_data.end());
    int batch_size = 1 << i;
    auto data_size = google_test_data.size() / cores;
    std::vector<std::thread> threads;
    SafeQueue<EvalResult> result;
    for (auto j = 0; j < cores; ++j) {
      threads.emplace_back(std::thread(SingleThread,
                                       google_network,
                                       cab_network,
                                       &google_test_data,
                                       &cab_test_data,
                                       batch_size,
                                       j * data_size,
                                       data_size,
                                       &result));
    }
    std::cout << "Threads all created" << std::endl;
    for (auto &cur_thread : threads) {
      cur_thread.join();
    }
    EvalResult *single_result = result.next();
    uintmax_t google_correct = 0;
    uintmax_t google_total = 0;
    uintmax_t cab_correct = 0;
    uintmax_t cab_total = 0;
    while (single_result) {
      google_correct += single_result->google_correct;
      google_total += single_result->google_total;
      cab_correct += single_result->cab_correct;
      cab_total += single_result->cab_total;
      delete (single_result);
      single_result = result.next();
    }
    std::cout << "=========== Result ==============" << std::endl;
    std::cout << "Batch Size : " << batch_size << ";" << "Google Correct : " << google_correct << ";"
              << "Google Total : " << google_total << "; Cab Correct : " << cab_correct << "; Cab Total : " << cab_total
              << std::endl;
  }
}
