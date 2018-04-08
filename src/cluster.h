//
// Created by Yujia Shen on 4/6/18.
//

#ifndef PROJECT_CLUSTER_H
#define PROJECT_CLUSTER_H
#include <src/psdd_node.h>
#include <src/psdd_manager.h>
extern "C" {
#include <sddapi.h>
};

namespace structured_bn {
class Cluster {
 public:
  static Cluster *GetClusterFromLocalConstraint(uint32_t cluster_index,
                                                const std::vector<Cluster*>& parent_clusters,
                                                const char *antecedent_vtree_filename,
                                                const std::vector<const char *> &antecedent_constraints,
                                                const std::unordered_map<uint32_t, uint32_t> &antecedent_variable_map,
                                                const char *succedent_vtree_filename,
                                                const std::vector<const char *> &succedent_constraints,
                                                const std::unordered_map<uint32_t, uint32_t> &succedent_variable_map);
  uint32_t cluster_index() const;
  const std::vector<Cluster*> parent_clusters() const;
  Vtree* succedent_vtree() const;
  Vtree* antecedent_vtree() const;
 private:
  Cluster(uint32_t cluster_index,
          const std::vector<Cluster*>& parent_clusters,
          PsddManager *antecedent_manager,
          const std::vector<PsddNode *>& antecedent_sdds,
          PsddManager *succedent_manager,
          const std::vector<PsddNode *>& succedent_psdds,
          uintmax_t psdd_flag_index);
  uint32_t cluster_index_;
  std::vector<Cluster*> parent_clusters_;
  PsddManager *antecedent_manager_;
  std::vector<PsddNode *> antecedents_;
  PsddManager *succedent_manager_;
  std::vector<PsddNode *> succedents_;
  uintmax_t psdd_flag_index_;
};
}
#endif //PROJECT_CLUSTER_H
