//
// Created by Yujia Shen on 4/6/18.
//

#include <cassert>
#include <unordered_map>
#include <vector>
#include <queue>
#include "network_compiler.h"
namespace {
using structured_bn::Cluster;
// find the connected cluster for the first root
std::unordered_map<uint32_t, Cluster *> ConnectedComponents(const std::vector<Cluster *> &sub_network,
                                                            Cluster *center_cluster) {
  std::unordered_map<uint32_t, std::vector<Cluster *>> edges;
  std::bitset<MAX_VAR> cluster_bitset;
  for (Cluster *cur_cluster : sub_network) {
    cluster_bitset.set(cur_cluster->cluster_index());
    edges[cur_cluster->cluster_index()] = std::vector<Cluster *>();
  }
  for (Cluster *cur_cluster : sub_network) {
    const std::vector<Cluster *> &cur_parents = cur_cluster->parent_clusters();
    auto cur_edge_it = edges.find(cur_cluster->cluster_index());
    for (Cluster *cur_parent : cur_parents) {
      if (cluster_bitset[cur_parent->cluster_index()]) {
        cur_edge_it->second.push_back(cur_parent);
        edges[cur_parent->cluster_index()].push_back(cur_cluster);
      }
    }
  }
  std::queue<Cluster *> open_list;
  open_list.push(center_cluster);
  std::unordered_map<uint32_t, Cluster *> connected_clusters;
  connected_clusters[center_cluster->cluster_index()] = center_cluster;
  while (!open_list.empty()) {
    Cluster *cur_cluster = open_list.front();
    open_list.pop();
    const auto &neighboring_clusters = edges[cur_cluster->cluster_index()];
    for (Cluster *cur_neighbor_cluster : neighboring_clusters) {
      auto explore_it = connected_clusters.find(cur_neighbor_cluster->cluster_index());
      if (explore_it == connected_clusters.end()) {
        connected_clusters[cur_neighbor_cluster->cluster_index()] = cur_neighbor_cluster;
        open_list.push(cur_neighbor_cluster);
      }
    }
  }
  return connected_clusters;
}
// input are topological sorted clusters and new vtrees for each cluster
Vtree *ConstructVtree(const std::vector<Cluster *> &clusters, const std::vector<Vtree *> &vtree_per_cluster) {
  if (clusters.size() == 1) {
    return vtree_per_cluster[clusters[0]->cluster_index()];
  }
  std::unordered_map<uint32_t, Cluster *> connected_component = ConnectedComponents(clusters, clusters[0]);
  if (connected_component.size() == clusters.size()) {
    // completely connected.
    std::vector<Cluster *> remaining_clusters(clusters.begin() + 1, clusters.end());
    Vtree *right_child = ConstructVtree(clusters, vtree_per_cluster);
    Vtree *new_internal_node = new_internal_vtree(vtree_per_cluster[clusters[0]->cluster_index()], right_child);
    return new_internal_node;
  } else {
    std::vector<Cluster *> left_clusters;
    std::vector<Cluster *> right_clusters;
    for (Cluster *orig_cluster : clusters) {
      if (connected_component.find(orig_cluster->cluster_index()) != connected_component.end()) {
        // connected with center cluster;
        left_clusters.push_back(orig_cluster);
      } else {
        right_clusters.push_back(orig_cluster);
      }
    }
    Vtree *left_child = ConstructVtree(left_clusters, vtree_per_cluster);
    Vtree *right_child = ConstructVtree(right_clusters, vtree_per_cluster);
    Vtree *new_internal_node = new_internal_vtree(left_child, right_child);
    return new_internal_node;
  }
}
}
namespace structured_bn {
NetworkCompiler::NetworkCompiler(structured_bn::Network *network, PsddManager *joint_psdd_manager)
    : network_(network), joint_psdd_manager_(joint_psdd_manager) {}
NetworkCompiler *NetworkCompiler::GetDefaultNetworkCompiler(Network *network) {
  std::vector<Cluster *> arbitrary_topological_order = network->ArbitraryTopologicalOrder();
  auto cluster_size = network->cluster_size();
  // copy succedent vtrees
  std::vector<Vtree *> succedent_vtree_per_cluster(cluster_size, nullptr);
  const auto &clusters = network->clusters();
  for (auto i = 0; i < cluster_size; ++i) {
    Cluster *cur_cluster = clusters[i];
    Vtree *copied_succedent_vtree = vtree_util::CopyVtree(cur_cluster->succedent_vtree());
    succedent_vtree_per_cluster[i] = copied_succedent_vtree;
  }
  Vtree *joint_vtree = ConstructVtree(arbitrary_topological_order, succedent_vtree_per_cluster);
  set_vtree_properties(joint_vtree);
  PsddManager *joint_psdd_manager = PsddManager::GetPsddManagerFromVtree(joint_vtree);
  sdd_vtree_free(joint_vtree);
  return new NetworkCompiler(network, joint_psdd_manager);
}

PsddNode *NetworkCompiler::ConformLocalPsdd(Cluster *target_cluster, PsddManager *target_manager) {
  Vtree *old_antecedent_vtree = target_cluster->antecedent_vtree();
  std::vector<SddLiteral> variables = vtree_util::VariablesUnderVtree(old_antecedent_vtree);
  std::unordered_map<SddLiteral, SddLiteral> network_variable_index_to_sdd_index;
  auto variable_size = (SddLiteral)variables.size();
  for (SddLiteral i = 1; i <= variable_size; ++i) {
    network_variable_index_to_sdd_index[variables[i - 1]] = i;
  }
  // Construct auxilary variables for conditions
  // Construct a SDD manager with desired vtree with auxilary variable
  // Convert antecedent PSDD to SDDs
  // Design auxilary variables for each condition
  // Convert the result SDD to normalized PSDD.
  // Construct the result PSDD (PsddManager need to implement new node function with normalization)
}
}
