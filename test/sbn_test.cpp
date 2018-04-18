//
// Created by Yujia Shen on 4/11/18.
//

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <cluster.h>
#include <network_compiler.h>
#include <network.h>
extern "C" {
#include <sddapi.h>
}
#include <json.hpp>
#include <fstream>
using structured_bn::Network;
using structured_bn::Cluster;
using structured_bn::NetworkCompiler;
using nlohmann::json;
namespace {
SddNode *CardinalityK(uint32_t variable_size_usign,
                      uint32_t k,
                      SddManager *manager,
                      std::unordered_map<uint32_t, std::unordered_map<uint32_t, SddNode *>> *cache) {
  auto variable_size = (SddLiteral) variable_size_usign;
  auto cache_it = cache->find(variable_size_usign);
  if (cache_it != cache->end()) {
    auto second_cache_it = cache_it->second.find(k);
    if (second_cache_it != cache_it->second.end()) {
      return second_cache_it->second;
    }
  } else {
    cache->insert({variable_size, {}});
    cache_it = cache->find(variable_size_usign);
  }
  if (variable_size == 1) {
    if (k == 0) {
      SddNode *result = sdd_manager_literal(-1, manager);
      cache_it->second.insert({0, result});
      return result;
    } else {
      SddNode *result = sdd_manager_literal(1, manager);
      cache_it->second.insert({1, result});
      return result;
    }
  } else {
    if (k == 0) {
      SddNode *remaining = CardinalityK(variable_size_usign - 1, 0, manager, cache);
      SddNode *result = sdd_conjoin(sdd_manager_literal(-variable_size, manager), remaining, manager);
      cache_it->second.insert({0, result});
      return result;
    } else if (k == variable_size) {
      SddNode *remaining = CardinalityK(variable_size_usign - 1, k - 1, manager, cache);
      SddNode *result = sdd_conjoin(sdd_manager_literal(variable_size, manager), remaining, manager);
      cache_it->second.insert({k, result});
      return result;
    } else {
      SddNode *remaining_positive = CardinalityK(variable_size_usign - 1, k - 1, manager, cache);
      SddNode *remaining_negative = CardinalityK(variable_size_usign - 1, k, manager, cache);
      SddNode *positive_case = sdd_conjoin(sdd_manager_literal(variable_size, manager), remaining_positive, manager);
      SddNode *negative_case = sdd_conjoin(sdd_manager_literal(-variable_size, manager), remaining_negative, manager);
      SddNode *result = sdd_disjoin(positive_case, negative_case, manager);
      cache_it->second.insert({k, result});
      return result;
    }
  }
}

std::pair<SddNode *, SddNode *> OddEvenFunction(uint32_t variable_size, SddManager *manager) {
  std::vector<std::pair<SddNode *, SddNode *>> odd_even_cache(variable_size + 1);
  for (auto i = 1; i <= variable_size; ++i) {
    if (i == 1) {
      odd_even_cache[i] = {sdd_manager_literal(-1, manager), sdd_manager_literal(1, manager)};
    } else {
      SddNode *even_first = sdd_conjoin(sdd_manager_literal(i, manager), odd_even_cache[i - 1].second, manager);
      SddNode *even_second =
          sdd_conjoin(sdd_manager_literal(-(SddLiteral) i, manager), odd_even_cache[i - 1].first, manager);
      SddNode *odd_first = sdd_conjoin(sdd_manager_literal(i, manager), odd_even_cache[i - 1].first, manager);
      SddNode *odd_second =
          sdd_conjoin(sdd_manager_literal(-(SddLiteral) i, manager), odd_even_cache[i - 1].second, manager);
      odd_even_cache[i] = {sdd_disjoin(even_first, even_second, manager), sdd_disjoin(odd_first, odd_second, manager)};
    }
  }
  return odd_even_cache[variable_size];
}

Network *ConstructNetwork1() {
  // root cluster contains 5 variables
  // leave cluster contains 5 variables
  Vtree *parent_vtree = sdd_vtree_new(5, "right");
  SddManager *sdd_manager = sdd_manager_new(parent_vtree);
  sdd_manager_auto_gc_and_minimize_off(sdd_manager);
  sdd_vtree_free(parent_vtree);
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, SddNode *>> cache;
  SddNode *card3 = CardinalityK(5, 3, sdd_manager, &cache);
  SddNode *card4 = CardinalityK(5, 4, sdd_manager, &cache);
  SddNode *card2 = CardinalityK(5, 2, sdd_manager, &cache);
  SddNode *root_constraint = sdd_disjoin(card2, sdd_disjoin(card3, card4, sdd_manager), sdd_manager);
  sdd_save("/tmp/root_constraint.sdd", root_constraint);
  sdd_vtree_save("/tmp/rl_vtree.vtree", sdd_manager_vtree(sdd_manager));
  std::string root_succedent_vtree = "/tmp/rl_vtree.vtree";
  std::vector<std::string> root_succedent = {"/tmp/root_constraint.sdd"};
  std::unordered_map<std::string, uint32_t> root_variable_map;
  for (auto i = 1; i <= 5; ++i) {
    root_variable_map[std::to_string(i)] = (uint32_t) i;
  }
  std::string leaf_antecedent_vtree = "/tmp/rl_vtree.vtree";
  std::unordered_map<std::string, uint32_t> leaf_antecedent_variable_mapping = root_variable_map;
  auto parity_sdd = OddEvenFunction(5, sdd_manager);
  sdd_save("/tmp/even.sdd", parity_sdd.first);
  sdd_save("/tmp/odd.sdd", parity_sdd.second);
  std::vector<std::string> leaf_antecedents = {"/tmp/even.sdd", "/tmp/odd.sdd"};
  sdd_save("/tmp/card2.sdd", card2);
  sdd_save("/tmp/card3.sdd", card3);
  std::vector<std::string> leaf_succedents = {"/tmp/card2.sdd", "/tmp/card3.sdd"};
  std::string leaf_succedent_vtree = "/tmp/rl_vtree.vtree";
  std::unordered_map<std::string, uint32_t> leaf_succedent_variable_map;
  for (auto i = 1; i <= 5; ++i) {
    leaf_succedent_variable_map[std::to_string(i+5)] = (uint32_t) i;
  }
  std::unordered_map<std::string, json> root_cluster_spec;
  root_cluster_spec["cluster_name"] = "root";
  root_cluster_spec["parents"] = std::vector<std::string>();
  root_cluster_spec["constraint"]["then"] = root_succedent;
  root_cluster_spec["constraint"]["then_vtree"] = root_succedent_vtree;
  root_cluster_spec["constraint"]["then_variable_mapping"] = root_variable_map;
  std::unordered_map<std::string, json> leaf_cluster_spec;
  leaf_cluster_spec["parents"] = {"root"};
  leaf_cluster_spec["cluster_name"] = "leaf";
  leaf_cluster_spec["constraint"]["if"] = leaf_antecedents;
  leaf_cluster_spec["constraint"]["if_vtree"] = leaf_antecedent_vtree;
  leaf_cluster_spec["constraint"]["if_variable_mapping"] = leaf_antecedent_variable_mapping;
  leaf_cluster_spec["constraint"]["then"] = leaf_succedents;
  leaf_cluster_spec["constraint"]["then_vtree"] = leaf_succedent_vtree;
  leaf_cluster_spec["constraint"]["then_variable_mapping"] = leaf_succedent_variable_map;
  json spec;
  spec["variables"] = {"1","2","3","4","5","6","7","8","9","10"};
  spec["clusters"] = {root_cluster_spec, leaf_cluster_spec};
  std::ofstream of ("/tmp/sbn_test_network1.json");
  of << std::setw(2) << spec << std::endl;
  Network* result_network = Network::GetNetworkFromSpecFile("/tmp/sbn_test_network1.json");
  sdd_manager_free(sdd_manager);
  return result_network;
}

Network *ConstructNetwork2() {
  // root cluster contains 5 variables
  // leave cluster contains 5 variables
  Vtree *parent_vtree = sdd_vtree_new(5, "right");
  SddManager *sdd_manager = sdd_manager_new(parent_vtree);
  sdd_manager_auto_gc_and_minimize_off(sdd_manager);
  sdd_vtree_free(parent_vtree);
  std::unordered_map<uint32_t, std::unordered_map<uint32_t, SddNode *>> cache;
  SddNode *card3 = CardinalityK(5, 3, sdd_manager, &cache);
  SddNode *card4 = CardinalityK(5, 4, sdd_manager, &cache);
  SddNode *card2 = CardinalityK(5, 2, sdd_manager, &cache);
  SddNode *root_constraint = sdd_disjoin(card2, sdd_disjoin(card3, card4, sdd_manager), sdd_manager);
  sdd_save("/tmp/root_constraint.sdd", root_constraint);
  sdd_vtree_save("/tmp/rl_vtree.vtree", sdd_manager_vtree(sdd_manager));
  std::string root_succedent_vtree = "/tmp/rl_vtree.vtree";
  std::vector<std::string> root_succedent = {"/tmp/root_constraint.sdd"};
  std::unordered_map<std::string, uint32_t> root_variable_map;
  for (auto i = 1; i <= 5; ++i) {
    root_variable_map[std::to_string(i)] = (uint32_t) i;
  }
  std::string leaf_antecedent_vtree = "/tmp/rl_vtree.vtree";
  std::unordered_map<std::string, uint32_t> leaf_antecedent_variable_mapping = root_variable_map;
  auto parity_sdd = OddEvenFunction(5, sdd_manager);
  sdd_save("/tmp/even.sdd", parity_sdd.first);
  sdd_save("/tmp/odd.sdd", parity_sdd.second);
  std::vector<std::string> leaf_antecedents = {"/tmp/even.sdd", "/tmp/odd.sdd"};
  sdd_save("/tmp/card2.sdd", card2);
  sdd_save("/tmp/card234.sdd", root_constraint);
  std::vector<std::string> leaf_succedents = {"/tmp/card2.sdd", "/tmp/card234.sdd"};
  std::string leaf_succedent_vtree = "/tmp/rl_vtree.vtree";
  std::unordered_map<std::string, uint32_t> leaf_succedent_variable_map;
  for (auto i = 1; i <= 5; ++i) {
    leaf_succedent_variable_map[std::to_string(i+5)] = (uint32_t) i;
  }
  std::unordered_map<std::string, uint32_t> lowest_leaf_antecedent_variable_map;
  std::unordered_map<std::string, uint32_t> lowest_leaf_succedent_variable_map;
  for (auto i = 1; i <= 5; ++i){
    lowest_leaf_antecedent_variable_map[std::to_string(i+5)] = (uint32_t)i;
    lowest_leaf_succedent_variable_map[std::to_string(i+10)] = (uint32_t)i;
  }
  std::unordered_map<std::string, json> root_cluster_spec;
  root_cluster_spec["cluster_name"] = "root";
  root_cluster_spec["parents"] = std::vector<std::string>();
  root_cluster_spec["constraint"]["then"] = root_succedent;
  root_cluster_spec["constraint"]["then_vtree"] = root_succedent_vtree;
  root_cluster_spec["constraint"]["then_variable_mapping"] = root_variable_map;
  std::unordered_map<std::string, json> leaf_cluster_spec;
  leaf_cluster_spec["parents"] = {"root"};
  leaf_cluster_spec["cluster_name"] = "leaf";
  leaf_cluster_spec["constraint"]["if"] = leaf_antecedents;
  leaf_cluster_spec["constraint"]["if_vtree"] = leaf_antecedent_vtree;
  leaf_cluster_spec["constraint"]["if_variable_mapping"] = leaf_antecedent_variable_mapping;
  leaf_cluster_spec["constraint"]["then"] = leaf_succedents;
  leaf_cluster_spec["constraint"]["then_vtree"] = leaf_succedent_vtree;
  leaf_cluster_spec["constraint"]["then_variable_mapping"] = leaf_succedent_variable_map;
  std::unordered_map<std::string, json> lowest_leaf_cluster_spec;
  lowest_leaf_cluster_spec["parents"] = {"leaf"};
  lowest_leaf_cluster_spec["cluster_name"] = "lowest_leaf";
  lowest_leaf_cluster_spec["constraint"]["if"] = leaf_antecedents;
  lowest_leaf_cluster_spec["constraint"]["if_vtree"] = leaf_antecedent_vtree;
  lowest_leaf_cluster_spec["constraint"]["if_variable_mapping"] = lowest_leaf_antecedent_variable_map;
  lowest_leaf_cluster_spec["constraint"]["then"] = leaf_succedents;
  lowest_leaf_cluster_spec["constraint"]["then_vtree"] = leaf_succedent_vtree;
  lowest_leaf_cluster_spec["constraint"]["then_variable_mapping"] = lowest_leaf_succedent_variable_map;
  json spec;
  spec["variables"] = {"1","2","3","4","5","6","7","8","9","10","11","12","13","14","15"};
  spec["clusters"] = {root_cluster_spec, leaf_cluster_spec, lowest_leaf_cluster_spec};
  std::ofstream of ("/tmp/sbn_test_network2.json");
  of << std::setw(2) << spec << std::endl;
  Network* result_network = Network::GetNetworkFromSpecFile("/tmp/sbn_test_network2.json");
  sdd_manager_free(sdd_manager);
  return result_network;
}

}

TEST(SBN_NETWORK_TEST, MODEL_CHECK_TEST){
  Network* network = ConstructNetwork1();
  std::bitset<MAX_VAR> variable_mask = ((1<<11) - 1);
  std::bitset<MAX_VAR> instantiation1;
  instantiation1.set(1);
  instantiation1.set(2);
  instantiation1.set(6);
  instantiation1.set(7);
  EXPECT_TRUE(network->IsModel(variable_mask, instantiation1));
  std::bitset<MAX_VAR> instantiation2;
  instantiation2.set(1);
  instantiation2.set(2);
  instantiation2.set(6);
  instantiation2.set(7);
  instantiation2.set(8);
  EXPECT_FALSE(network->IsModel(variable_mask, instantiation2));
  delete(network);
}

TEST(SBN_NETWORK_TEST, LEARNING_TEST){
  Network* network = ConstructNetwork1();
  auto data = new BinaryData();
  network->LearnParametersUsingLaplacianSmoothing(data, PsddParameter::CreateFromDecimal(1));
  std::bitset<MAX_VAR> instantiation1;
  instantiation1.set(1);
  instantiation1.set(2);
  instantiation1.set(6);
  instantiation1.set(7);
  data->AddRecord(instantiation1);
  Probability result = network->CalculateProbability(data);
  EXPECT_DOUBLE_EQ(result.parameter(), std::log(std::pow(0.5,7)));
  delete(network);
  delete(data);
}

TEST(SBN_NETWORK_TEST, COMPILATION_TEST){
  Network* network = ConstructNetwork1();
  auto data = new BinaryData();
  network->LearnParametersUsingLaplacianSmoothing(data, PsddParameter::CreateFromDecimal(1));
  NetworkCompiler* compiler = NetworkCompiler::GetDefaultNetworkCompiler(network);
  auto result = compiler->Compile();
  std::bitset<MAX_VAR> variable_mask = (1<<11)-1;
  std::bitset<MAX_VAR> instantiation1;
  instantiation1.set(1);
  instantiation1.set(2);
  instantiation1.set(6);
  instantiation1.set(7);
  Probability pr_result = psdd_node_util::Evaluate(variable_mask,instantiation1, result.first);
  EXPECT_DOUBLE_EQ(pr_result.parameter(), std::log(std::pow(0.5,7)));
  delete(compiler);
  delete(network);
  delete(data);

  Network* network2 = ConstructNetwork2();
  data = new BinaryData();
  network2->LearnParametersUsingLaplacianSmoothing(data, PsddParameter::CreateFromDecimal(1));
  compiler = NetworkCompiler::GetDefaultNetworkCompiler(network2);
  result = compiler->Compile();
  variable_mask = (1<<16)-1;
  instantiation1.reset();
  instantiation1.set(1);
  instantiation1.set(2);
  instantiation1.set(6);
  instantiation1.set(7);
  instantiation1.set(11);
  instantiation1.set(12);
  pr_result = psdd_node_util::Evaluate(variable_mask,instantiation1, result.first);
  EXPECT_DOUBLE_EQ(pr_result.parameter(), std::log(std::pow(0.5,9)));
  delete(compiler);
  delete(network2);
  delete(data);
}