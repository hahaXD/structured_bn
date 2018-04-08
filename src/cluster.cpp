//
// Created by Yujia Shen on 4/6/18.
//

#include "cluster.h"

namespace structured_bn {
Cluster *Cluster::GetClusterFromLocalConstraint(uint32_t cluster_index,
                                                const std::vector<Cluster *> &parent_clusters,
                                                const char *antecedent_vtree_filename,
                                                const std::vector<const char *> &antecedent_constraints,
                                                const std::unordered_map<uint32_t, uint32_t> &antecedent_variable_map,
                                                const char *succedent_vtree_filename,
                                                const std::vector<const char *> &succedent_constraints,
                                                const std::unordered_map<uint32_t, uint32_t> &succedent_variable_map) {
  Vtree *antecedent_vtree = sdd_vtree_read(antecedent_vtree_filename);
  SddManager *antecedent_sdd_manager = sdd_manager_new(antecedent_vtree);
  sdd_manager_auto_gc_and_minimize_off(antecedent_sdd_manager);
  PsddManager *antecedent_manager = PsddManager::GetPsddManagerFromSddVtree(antecedent_vtree, antecedent_variable_map);
  sdd_vtree_free(antecedent_vtree);
  std::vector<PsddNode *> antecedent_sdds;
  for (auto sdd_filaname : antecedent_constraints) {
    SddNode *cur_constraint = sdd_read(sdd_filaname, antecedent_sdd_manager);
    PsddNode *cur_antecedent_psdd_constraint = antecedent_manager->ConvertSddToPsdd(cur_constraint,
                                                                                    sdd_manager_vtree(
                                                                                        antecedent_sdd_manager),
                                                                                    0,
                                                                                    antecedent_variable_map);
    antecedent_sdds.push_back(cur_antecedent_psdd_constraint);
    sdd_manager_garbage_collect(antecedent_sdd_manager);
  }
  sdd_manager_free(antecedent_sdd_manager);
  Vtree *succedent_vtree = sdd_vtree_read(succedent_vtree_filename);
  SddManager *succedent_sdd_manager = sdd_manager_new(succedent_vtree);
  PsddManager *succedent_manager = PsddManager::GetPsddManagerFromSddVtree(succedent_vtree, succedent_variable_map);
  sdd_vtree_free(succedent_vtree);
  uintmax_t flag_index = 1;
  std::vector<PsddNode *> succedent_psdds;
  for (auto sdd_filename : succedent_constraints) {
    SddNode *cur_constraint = sdd_read(sdd_filename, succedent_sdd_manager);
    PsddNode *cur_succedent_psdd_constraint = succedent_manager->ConvertSddToPsdd(cur_constraint,
                                                                                  sdd_manager_vtree(
                                                                                      antecedent_sdd_manager),
                                                                                  flag_index,
                                                                                  succedent_variable_map);
    succedent_psdds.push_back(cur_succedent_psdd_constraint);
    flag_index += 1;
    sdd_manager_garbage_collect(succedent_sdd_manager);
  }
  sdd_manager_free(succedent_sdd_manager);
  return new Cluster(cluster_index,
                     parent_clusters,
                     antecedent_manager,
                     antecedent_sdds,
                     succedent_manager,
                     succedent_psdds,
                     flag_index);
}
uint32_t Cluster::cluster_index() const {
  return cluster_index_;
}
Cluster::Cluster(uint32_t cluster_index,
                 const std::vector<Cluster *> &parent_clusters,
                 PsddManager *antecedent_manager,
                 const std::vector<PsddNode *> &antecedent_sdds,
                 PsddManager *succedent_manager,
                 const std::vector<PsddNode *> &succedent_psdds,
                 uintmax_t psdd_flag_index) : cluster_index_(cluster_index),
                                              parent_clusters_(parent_clusters),
                                              antecedent_manager_(antecedent_manager),
                                              antecedents_(antecedent_sdds),
                                              succedent_manager_(succedent_manager),
                                              succedents_(succedent_psdds),
                                              psdd_flag_index_(psdd_flag_index) {}
const std::vector<Cluster *> Cluster::parent_clusters() const {
  return parent_clusters_;
}
Vtree *Cluster::succedent_vtree() const {
  return succedent_manager_->vtree();
}
Vtree *Cluster::antecedent_vtree() const {
  return antecedent_manager_->vtree();
}
}

