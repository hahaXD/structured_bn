//
// Created by Yujia Shen on 4/6/18.
//

#include "network.h"
namespace structured_bn {
Network::Network(std::vector<std::string> variable_names,
                 std::vector<structured_bn::Cluster *> clusters,
                 std::vector<std::string> cluster_names)
    : variable_names_(std::move(variable_names)),
      clusters_(std::move(clusters)),
      cluster_names_(std::move(cluster_names)) {}

std::vector<Cluster *> Network::ArbitraryTopologicalOrder() const {
  std::vector<Cluster*> sorted_clusters;
  uint32_t cluster_size = this->cluster_size();
  while(sorted_clusters.size() < cluster_size){
    std::vector<Cluster*> root_clusters = RootClustersAfterCondition(sorted_clusters);
    sorted_clusters.insert(sorted_clusters.end(), root_clusters.begin(), root_clusters.end());
  }
  return sorted_clusters;
}

uint32_t Network::cluster_size() const {
  return (uint32_t) clusters_.size();
}
std::vector<Cluster *> Network::RootClustersAfterCondition(const std::vector<Cluster *> &clusters_conditioned) const {
  std::vector<Cluster*> roots;
  std::bitset<MAX_VAR> cluster_indicators;
  for (Cluster* conditioned_cluster : clusters_conditioned){
    cluster_indicators.set(conditioned_cluster->cluster_index());
  }
  for (Cluster* cur_cluster : clusters_){
    if (cluster_indicators[cur_cluster->cluster_index()]){
      continue;
    }
    const auto& parent_clusters = cur_cluster->parent_clusters();
    bool is_root = true;
    for (Cluster* cur_parent_cluster : parent_clusters){
      if (!cluster_indicators[cur_parent_cluster->cluster_index()]){
        is_root = false;
        break;
      }
    }
    if (is_root){
      roots.push_back(cur_cluster);
    }
  }
  return roots;
}
const std::vector<Cluster *> &Network::clusters() const {
  return clusters_;
}

}
