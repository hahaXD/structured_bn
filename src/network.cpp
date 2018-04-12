//
// Created by Yujia Shen on 4/6/18.
//

#include "network.h"
#include <json.hpp>
#include <fstream>

using nlohmann::json;
namespace structured_bn {
Network::Network(std::vector<std::string> variable_names,
                 std::vector<structured_bn::Cluster *> clusters,
                 std::vector<std::string> cluster_names)
    : variable_names_(std::move(variable_names)),
      clusters_(std::move(clusters)),
      cluster_names_(std::move(cluster_names)) {}

std::vector<Cluster *> Network::ArbitraryTopologicalOrder() const {
  std::vector<Cluster *> sorted_clusters;
  uint32_t cluster_size = this->cluster_size();
  while (sorted_clusters.size() < cluster_size) {
    std::vector<Cluster *> root_clusters = RootClustersAfterCondition(sorted_clusters);
    sorted_clusters.insert(sorted_clusters.end(), root_clusters.begin(), root_clusters.end());
  }
  return sorted_clusters;
}

uint32_t Network::cluster_size() const {
  return (uint32_t) clusters_.size();
}
std::vector<Cluster *> Network::RootClustersAfterCondition(const std::vector<Cluster *> &clusters_conditioned) const {
  std::vector<Cluster *> roots;
  std::bitset<MAX_VAR> cluster_indicators;
  for (Cluster *conditioned_cluster : clusters_conditioned) {
    cluster_indicators.set(conditioned_cluster->cluster_index());
  }
  for (Cluster *cur_cluster : clusters_) {
    if (cluster_indicators[cur_cluster->cluster_index()]) {
      continue;
    }
    const auto &parent_clusters = cur_cluster->parent_clusters();
    bool is_root = true;
    for (Cluster *cur_parent_cluster : parent_clusters) {
      if (!cluster_indicators[cur_parent_cluster->cluster_index()]) {
        is_root = false;
        break;
      }
    }
    if (is_root) {
      roots.push_back(cur_cluster);
    }
  }
  return roots;
}
const std::vector<Cluster *> &Network::clusters() const {
  return clusters_;
}
Network *Network::GetNetworkFromSpecFile(const char *filename) {
  std::ifstream spec_file(filename);
  json network_spec;
  spec_file >> network_spec;
  assert(network_spec.find("variables") != network_spec.end());
  const auto &variables = network_spec["variables"];
  assert(network_spec.find("clusters") != network_spec.end());
  const auto &clusters = network_spec["clusters"];
  auto cluster_size = clusters.size();
  auto variable_size = variables.size();
  // Necessary arguments for network construction
  std::vector<std::string> variable_names;
  std::vector<std::string> cluster_names;
  std::vector<Cluster *> network_clusters(cluster_size, nullptr);
  std::unordered_map<std::string, uint32_t> variable_network_index_lookup;
  variable_names.emplace_back("");
  for (auto i = 0; i < variable_size; i++) {
    variable_names.push_back(variables[i]);
    variable_network_index_lookup[variables[i]] = (uint32_t) i + 1;
  }
  // update cluster name with the corresponding index
  std::unordered_set<uint32_t> unprocessed_cluster_indexes;
  std::unordered_map<std::string, uint32_t> cluster_names_lookup;
  for (auto cluster_index = 0; cluster_index < cluster_size; ++cluster_index) {
    const auto &cur_cluster_spec = clusters[cluster_index];
    assert(cur_cluster_spec.find("cluster_name") != cur_cluster_spec.end());
    std::string cluster_name = cur_cluster_spec["cluster_name"];
    cluster_names.emplace_back(cluster_name);
    cluster_names_lookup[cluster_names.back()] = (uint32_t)cluster_index;
    unprocessed_cluster_indexes.insert((uint32_t) (cluster_index));
  }
  std::unordered_set<uint32_t> processed_cluster_indexes;
  while (!unprocessed_cluster_indexes.empty()) {
    auto unprocessed_cluster_indexes_it = unprocessed_cluster_indexes.begin();
    while (unprocessed_cluster_indexes_it != unprocessed_cluster_indexes.end()) {
      uint32_t cur_cluster_index = *unprocessed_cluster_indexes_it;
      const auto &cur_cluster_info = clusters[cur_cluster_index];
      assert(cur_cluster_info.find("parents") != cur_cluster_info.end());
      const auto &cur_cluster_parents = cur_cluster_info["parents"];
      bool parent_processed = true;
      for (const auto &parent_name_json : cur_cluster_parents) {
        std::string parent_name = parent_name_json;
        uint32_t parent_index = cluster_names_lookup[parent_name];
        if (processed_cluster_indexes.find(parent_index) == processed_cluster_indexes.end()) {
          parent_processed = false;
          break;
        }
      }
      if (parent_processed) {
        unprocessed_cluster_indexes_it = unprocessed_cluster_indexes.erase(unprocessed_cluster_indexes_it);
        processed_cluster_indexes.insert(cur_cluster_index);
        std::string antecedent_vtree_filename;
        std::vector<std::string> antecedent_sdd_filenames;
        std::unordered_map<uint32_t, uint32_t> antecedent_variable_map;
        std::string succedent_vtree_filename;
        std::vector<std::string> succedent_sdd_filenames;
        std::unordered_map<uint32_t, uint32_t> succedent_variable_map;
        std::vector<Cluster *> parent_clusters;
        // populate parent clusters
        for (const auto &parent_name_json : cur_cluster_parents) {
          std::string parent_name = parent_name_json;
          uint32_t parent_index = cluster_names_lookup[parent_name];
          assert(network_clusters[parent_index] != nullptr);
          parent_clusters.push_back(network_clusters[parent_index]);
        }
        // populate antecedents
        assert(cur_cluster_info.find("constraint") != cur_cluster_info.end());
        const auto &constraint = cur_cluster_info["constraint"];
        if (!parent_clusters.empty()) {
          assert(constraint.find("if_vtree") != constraint.end());
          antecedent_vtree_filename = constraint["if_vtree"];
          assert(constraint.find("if") != constraint.end());
          const auto &if_json = constraint["if"];
          for (const auto &if_sdd : if_json) {
            std::string if_filename = if_sdd;
            antecedent_sdd_filenames.push_back(if_filename);
          }
          assert(constraint.find("if_variable_mapping") != constraint.end());
          const auto &if_variable_mapping = constraint["if_variable_mapping"];
          for (auto variable_mapping_it = if_variable_mapping.begin(); variable_mapping_it != if_variable_mapping.end();
               ++variable_mapping_it) {
            std::string cur_varaible_name = variable_mapping_it.key();
            uint32_t sdd_variable_index = variable_mapping_it.value();
            assert(variable_network_index_lookup.find(cur_varaible_name) != variable_network_index_lookup.end());
            uint32_t psdd_variable_index = variable_network_index_lookup[cur_varaible_name];
            antecedent_variable_map[sdd_variable_index] = psdd_variable_index;
          }
        }
        assert(constraint.find("then_vtree") != constraint.end());
        succedent_vtree_filename = constraint["then_vtree"];
        assert(constraint.find("then") != constraint.end());
        const auto &then_json = constraint["then"];
        for (const auto &then_sdd: then_json) {
          std::string then_filename = then_sdd;
          succedent_sdd_filenames.push_back(then_filename);
        }
        assert(constraint.find("then_variable_mapping") != constraint.end());
        const auto &then_variable_mapping = constraint["then_variable_mapping"];
        for (auto variable_mapping_it = then_variable_mapping.begin();
             variable_mapping_it != then_variable_mapping.end();
             ++variable_mapping_it) {
          std::string cur_varaible_name = variable_mapping_it.key();
          uint32_t sdd_variable_index = variable_mapping_it.value();
          assert(variable_network_index_lookup.find(cur_varaible_name) != variable_network_index_lookup.end());
          uint32_t psdd_variable_index = variable_network_index_lookup[cur_varaible_name];
          succedent_variable_map[sdd_variable_index] = psdd_variable_index;
        }
        Cluster *cur_cluster = Cluster::GetClusterFromLocalConstraint(cur_cluster_index,
                                                                      parent_clusters,
                                                                      antecedent_vtree_filename,
                                                                      antecedent_sdd_filenames,
                                                                      antecedent_variable_map,
                                                                      succedent_vtree_filename,
                                                                      succedent_sdd_filenames,
                                                                      succedent_variable_map);
        network_clusters[cur_cluster_index] = cur_cluster;
      } else {
        unprocessed_cluster_indexes_it++;
      }
    }
  }
  for (const auto cur_cluster : network_clusters) {
    assert(cur_cluster != nullptr);
  }
  return new Network(variable_names, network_clusters, cluster_names);
}

Network::~Network() {
  for (Cluster* cur_cluster : clusters_){
    delete(cur_cluster);
  }
}
void Network::LearnParametersUsingLaplacianSmoothing(BinaryData *data, const PsddParameter& alpha) {
  for (Cluster* cur_cluster : clusters_){
    cur_cluster->LearnParametersUsingLaplacianSmoothing(data, alpha);
  }
}
bool Network::IsModel(const std::bitset<MAX_VAR> &variable_mask,
                      const std::bitset<MAX_VAR> &instantiation) const {
  for (Cluster* cur_cluster : clusters_){
    if (!cur_cluster->IsModel(variable_mask, instantiation)){
      return false;
    }
  }
  return true;
}
Probability Network::CalculateProbability(BinaryData *data) const {
  Probability total_probability = Probability::CreateFromDecimal(1);
  for (Cluster* cur_cluster : clusters_){
    total_probability = total_probability * cur_cluster->CalculateProbability(data);
  }
  return total_probability;
}

}
