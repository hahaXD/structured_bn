//
// Created by Yujia Shen on 4/6/18.
//

#ifndef STRUCTURED_BN_NETWORK_H
#define STRUCTURED_BN_NETWORK_H
#include <string>
#include <vector>
#include <psdd/random_double_generator.h>
#include "cluster.h"
namespace structured_bn {
class Network {
 public:
  static Network *GetNetworkFromSpecFile(const char *filename);
  ~Network();
  // returns an arbitrary topological order. The root shows before leaves
  std::vector<Cluster *> ArbitraryTopologicalOrder() const;
  std::vector<Cluster *> RootClustersAfterCondition(const std::vector<Cluster *> &clusters_conditioned) const;
  const std::vector<Cluster *> &clusters() const;
  Probability CalculateProbability(BinaryData *data) const;
  void LearnParametersUsingLaplacianSmoothing(BinaryData *data, const PsddParameter &alpha);
  void SampleParameters(RandomDoubleGenerator* generator);
  bool IsModel(const std::bitset<MAX_VAR> &variable_mask, const std::bitset<MAX_VAR> &instantiation) const;
  std::unordered_map<std::string, uint32_t> GetVariableIndexMap() const;
  uint32_t cluster_size() const;
  const std::vector<std::string>& variable_names() const;
  const std::vector<std::string>& cluster_names() const;
 private:
  Network(std::vector<std::string> variable_names,
          std::vector<Cluster *> clusters,
          std::vector<std::string> cluster_names);
  // indexed by 1
  std::vector<std::string> variable_names_;
  // indexed by 0
  std::vector<Cluster *> clusters_;
  std::vector<std::string> cluster_names_;
};
}
#endif //STRUCTURED_BN_NETWORK_H
