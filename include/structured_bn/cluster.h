//
// Created by Yujia Shen on 4/6/18.
//

#ifndef PROJECT_CLUSTER_H
#define PROJECT_CLUSTER_H
#include <psdd/psdd_node.h>
#include <psdd/psdd_manager.h>
extern "C" {
#include <sdd/sddapi.h>
};

namespace structured_bn {
class Cluster {
 public:
  static Cluster *GetClusterFromLocalConstraint(uint32_t cluster_index,
                                                const std::vector<Cluster *> &parent_clusters,
                                                const std::string &antecedent_vtree_filename,
                                                const std::vector<std::string> &antecedent_constraints,
                                                const std::unordered_map<uint32_t, uint32_t> &antecedent_variable_map,
                                                const std::string &succedent_vtree_filename,
                                                const std::vector<std::string> &succedent_constraints,
                                                const std::unordered_map<uint32_t, uint32_t> &succedent_variable_map);
  ~Cluster();
  uint32_t cluster_index() const;
  const std::vector<Cluster *> parent_clusters() const;
  Vtree *succedent_vtree() const;
  Vtree *antecedent_vtree() const;
  const std::vector<PsddNode *> &succedents() const;
  const std::vector<PsddNode *> &antecedents() const;
  void SampleParameters(RandomDoubleGenerator* generator);
  void LearnParametersUsingLaplacianSmoothing (BinaryData* data, const PsddParameter& alpha);
  Probability CalculateProbability(BinaryData* data) const;
  bool IsModel(const std::bitset<MAX_VAR>& variable_mask, const std::bitset<MAX_VAR>& instantiation) const;
 private:
  Cluster(uint32_t cluster_index,
          const std::vector<Cluster *> &parent_clusters,
          PsddManager *antecedent_manager,
          const std::vector<PsddNode *> &antecedent_sdds,
          PsddManager *succedent_manager,
          const std::vector<PsddNode *> &succedent_psdds,
          uintmax_t psdd_flag_index);
  // return whether data is completely satisfied
  bool CalculateDataCount(BinaryData* data) const;
  uint32_t cluster_index_;
  std::vector<Cluster *> parent_clusters_;
  PsddManager *antecedent_manager_;
  std::vector<PsddNode *> antecedents_;
  PsddManager *succedent_manager_;
  std::vector<PsddNode *> succedents_;
  uintmax_t psdd_flag_index_;
};
}
#endif //PROJECT_CLUSTER_H
