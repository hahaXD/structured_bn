//
// Created by Yujia Shen on 4/6/18.
//

#include <cassert>
#include <stack>
#include "cluster.h"

namespace structured_bn {
Cluster *Cluster::GetClusterFromLocalConstraint(uint32_t cluster_index,
                                                const std::vector<Cluster *> &parent_clusters,
                                                const std::string &antecedent_vtree_filename,
                                                const std::vector<std::string> &antecedent_constraints,
                                                const std::unordered_map<uint32_t, uint32_t> &antecedent_variable_map,
                                                const std::string &succedent_vtree_filename,
                                                const std::vector<std::string> &succedent_constraints,
                                                const std::unordered_map<uint32_t, uint32_t> &succedent_variable_map) {
  SddManager *antecedent_sdd_manager = nullptr;
  PsddManager *antecedent_manager = nullptr;
  if (!antecedent_vtree_filename.empty()) {
    Vtree *antecedent_vtree = sdd_vtree_read(antecedent_vtree_filename.c_str());
    antecedent_sdd_manager = sdd_manager_new(antecedent_vtree);
    sdd_manager_auto_gc_and_minimize_off(antecedent_sdd_manager);
    antecedent_manager = PsddManager::GetPsddManagerFromSddVtree(antecedent_vtree, antecedent_variable_map);
    sdd_vtree_free(antecedent_vtree);
  }
  std::vector<PsddNode *> antecedent_sdds;
  if (antecedent_manager != nullptr) {
    for (const auto &sdd_filaname : antecedent_constraints) {
      SddNode *cur_constraint = sdd_read(sdd_filaname.c_str(), antecedent_sdd_manager);
      PsddNode *cur_antecedent_psdd_constraint = antecedent_manager->ConvertSddToPsdd(cur_constraint,
                                                                                      sdd_manager_vtree(
                                                                                          antecedent_sdd_manager),
                                                                                      0,
                                                                                      antecedent_variable_map);
      antecedent_sdds.push_back(cur_antecedent_psdd_constraint);
      sdd_manager_garbage_collect(antecedent_sdd_manager);
    }
  }
  if (antecedent_manager != nullptr) {
    sdd_manager_free(antecedent_sdd_manager);
  }
  Vtree *succedent_vtree = sdd_vtree_read(succedent_vtree_filename.c_str());
  SddManager *succedent_sdd_manager = sdd_manager_new(succedent_vtree);
  PsddManager *succedent_manager = PsddManager::GetPsddManagerFromSddVtree(succedent_vtree, succedent_variable_map);
  sdd_vtree_free(succedent_vtree);
  uintmax_t flag_index = 1;
  std::vector<PsddNode *> succedent_psdds;
  for (const auto &sdd_filename : succedent_constraints) {
    SddNode *cur_constraint = sdd_read(sdd_filename.c_str(), succedent_sdd_manager);
    PsddNode *cur_succedent_psdd_constraint = succedent_manager->ConvertSddToPsdd(cur_constraint,
                                                                                  sdd_manager_vtree(
                                                                                      succedent_sdd_manager),
                                                                                  flag_index,
                                                                                  succedent_variable_map);
    succedent_psdds.push_back(cur_succedent_psdd_constraint);
    flag_index += 1;
    sdd_manager_garbage_collect(succedent_sdd_manager);
  }
  sdd_manager_free(succedent_sdd_manager);
  assert(
      antecedent_sdds.size() == succedent_psdds.size() || (antecedent_sdds.empty() && succedent_psdds.size() == 1));
  return new Cluster(cluster_index,
                     parent_clusters,
                     antecedent_manager,
                     antecedent_sdds,
                     succedent_manager,
                     succedent_psdds,
                     flag_index);
}
Cluster::~Cluster(){
  delete(antecedent_manager_);
  delete(succedent_manager_);
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
const std::vector<PsddNode *> &Cluster::succedents() const {
  return succedents_;
}
const std::vector<PsddNode *> &Cluster::antecedents() const {
  return antecedents_;
}
void Cluster::LearnParametersUsingLaplacianSmoothing(BinaryData *data, const PsddParameter& alpha) {
  const auto &data_map = data->data();
  CalculateDataCount(data);
  std::vector<PsddNode *> new_succedents;
  for (PsddNode *succedent: succedents_) {
    std::vector<PsddNode *> serialized_succedent = psdd_node_util::SerializePsddNodes(succedent);
    for (auto it = serialized_succedent.rbegin(); it != serialized_succedent.rend(); ++it) {
      PsddNode *cur_node = *it;
      if (cur_node->node_type() == LITERAL_NODE_TYPE) {
        cur_node->SetUserData((uintmax_t) cur_node);
      } else if (cur_node->node_type() == TOP_NODE_TYPE) {
        PsddTopNode *cur_top_node = cur_node->psdd_top_node();
        auto positive_data_count = cur_top_node->true_data_count();
        auto negative_data_count = cur_top_node->false_data_count();
        auto total_data_count = positive_data_count + negative_data_count;
        PsddParameter smoothed_partition = PsddParameter::CreateFromDecimal(total_data_count) + alpha + alpha;
        PsddParameter smoothed_positive = PsddParameter::CreateFromDecimal(positive_data_count) + alpha;
        PsddParameter smoothed_negative = PsddParameter::CreateFromDecimal(negative_data_count) + alpha;
        PsddNode *new_top_node = succedent_manager_->GetPsddTopNode(cur_top_node->variable_index(),
                                                                    cur_top_node->flag_index(),
                                                                    smoothed_positive / smoothed_partition,
                                                                    smoothed_negative / smoothed_partition);
        cur_top_node->SetUserData((uintmax_t) new_top_node);
      } else {
        PsddDecisionNode *cur_decision_node = cur_node->psdd_decision_node();
        const auto &data_counts = cur_decision_node->data_counts();
        const auto &primes = cur_decision_node->primes();
        const auto &subs = cur_decision_node->subs();
        auto element_size = primes.size();
        std::vector<PsddNode *> next_primes;
        std::vector<PsddNode *> next_subs;
        std::vector<PsddParameter> next_parameters;
        PsddParameter partition;
        for (auto i = 0; i < element_size; ++i) {
          PsddNode *cur_prime = primes[i];
          PsddNode *cur_sub = subs[i];
          auto new_prime = (PsddNode *) cur_prime->user_data();
          auto new_sub = (PsddNode *) cur_sub->user_data();
          next_primes.push_back(new_prime);
          next_subs.push_back(new_sub);
          next_parameters.push_back(PsddParameter::CreateFromDecimal(data_counts[i]) + alpha);
          partition = partition + next_parameters.back();
        }
        for (auto &cur_param : next_parameters) {
          cur_param = cur_param / partition;
        }
        PsddNode *new_decn_node = succedent_manager_->GetConformedPsddDecisionNode(next_primes,
                                                                                   next_subs,
                                                                                   next_parameters,
                                                                                   cur_node->flag_index());
        cur_decision_node->SetUserData((uintmax_t) new_decn_node);
      }
    }
    auto new_succedent = (PsddNode *) succedent->user_data();
    new_succedents.push_back(new_succedent);
    for (PsddNode *old_node : serialized_succedent) {
      old_node->SetUserData(0);
    }
  }
  succedent_manager_->DeleteUnusedPsddNodes(new_succedents);
  succedents_ = new_succedents;
}

bool Cluster::CalculateDataCount(BinaryData *data) const {
  const auto &training_data = data->data();
  std::vector<std::vector<PsddNode *>> serialized_succedent_psdd_nodes;
  std::vector<PsddNode *> serialized_antecedent_psdd_nodes;
  if (!antecedents_.empty()) {
    serialized_antecedent_psdd_nodes = std::move(psdd_node_util::SerializePsddNodes(antecedents_));
  }
  for (PsddNode *cur_succedent : succedents_) {
    serialized_succedent_psdd_nodes.emplace_back(psdd_node_util::SerializePsddNodes(cur_succedent));
    for (PsddNode *cur_node : serialized_succedent_psdd_nodes.back()) {
      cur_node->ResetDataCount();
    }
  }
  bool satisfied = true;
  uintmax_t total_data = 0;
  uintmax_t valid_data = 0;
  for (const auto &data_pair : training_data) {
    const auto &cur_data = data_pair.first;
    uintmax_t cur_data_freq = data_pair.second;
    uintmax_t succedent_index = 0;
    total_data += cur_data_freq;
    if (!parent_clusters_.empty()) {
      auto element_size = succedents_.size();
      succedent_index = (uintmax_t) element_size;
      psdd_node_util::SetActivationFlag(cur_data, serialized_antecedent_psdd_nodes);
      for (auto i = 0; i < element_size; ++i) {
        if (antecedents_[i]->activation_flag()) {
          succedent_index = (uintmax_t) i;
          break;
        }
      }
      for (PsddNode *antecedent_node : serialized_antecedent_psdd_nodes) {
        antecedent_node->ResetActivationFlag();
      }
      if (succedent_index == element_size) {
        // cannot find an active condition, not a model
        satisfied = false;
        continue;
      }
    }
    const auto &serialized_succedents = serialized_succedent_psdd_nodes[succedent_index];
    psdd_node_util::SetActivationFlag(cur_data, serialized_succedents);
    if (!succedents_[succedent_index]->activation_flag()) {
      for (PsddNode *cur_node : serialized_succedents) {
        cur_node->ResetActivationFlag();
      }
      satisfied = false;
      continue;
    } else {
      // increment data count
      valid_data += cur_data_freq;
      std::stack<PsddNode *> explore_list;
      explore_list.push(succedents_[succedent_index]);
      while (!explore_list.empty()) {
        PsddNode *cur_node = explore_list.top();
        explore_list.pop();
        if (cur_node->node_type() == LITERAL_NODE_TYPE) {
          // literal node
          continue;
        } else if (cur_node->node_type() == DECISION_NODE_TYPE) {
          // decision node
          auto cur_decn_node = cur_node->psdd_decision_node();
          const auto &primes = cur_decn_node->primes();
          const auto &subs = cur_decn_node->subs();
          auto element_size = primes.size();
          for (auto i = 0; i < element_size; i++) {
            if (primes[i]->activation_flag() && subs[i]->activation_flag()) {
              explore_list.push(primes[i]);
              explore_list.push(subs[i]);
              cur_decn_node->IncrementDataCount((uintmax_t) i, cur_data_freq);
              break;
            }
          }
        } else {
          // top node
          assert(cur_node->node_type() == TOP_NODE_TYPE);
          auto cur_top_node = cur_node->psdd_top_node();
          if (cur_data[cur_top_node->variable_index()]) {
            cur_top_node->IncrementTrueDataCount(cur_data_freq);
          } else {
            cur_top_node->IncrementFalseDataCount(cur_data_freq);
          }
        }
      }
      for (PsddNode *cur_node : serialized_succedents) {
        cur_node->ResetActivationFlag();
      }
    }
  }
  return satisfied;
}

bool Cluster::IsModel(const std::bitset<MAX_VAR> &variable_mask,
                      const std::bitset<MAX_VAR> &instantiation) const {
  if (parent_clusters_.empty()) {
    return psdd_node_util::IsConsistent(succedents_[0], variable_mask, instantiation);
  } else {
    auto condition_size = antecedents_.size();
    for (auto i = 0; i < condition_size; ++i) {
      PsddNode *cur_antecedent = antecedents_[i];
      PsddNode *cur_succedent = succedents_[i];
      if (psdd_node_util::IsConsistent(cur_antecedent, variable_mask, instantiation)
          && psdd_node_util::IsConsistent(cur_succedent, variable_mask, instantiation)) {
        return true;
      }
    }
    return false;
  }
}
Probability Cluster::CalculateProbability(BinaryData* data) const{
  bool satisfied = CalculateDataCount(data);
  if (!satisfied){
    return Probability::CreateFromDecimal(0);
  }
  std::vector<PsddNode*> serialized_succedents = psdd_node_util::SerializePsddNodes(succedents_);
  Probability probability = Probability::CreateFromDecimal(1);
  for (PsddNode* cur_node : serialized_succedents){
    probability = probability * cur_node->CalculateLocalProbability();
  }
  return probability;
}
void Cluster::SampleParameters(RandomDoubleGenerator *generator) {
  uintmax_t new_flag_index = psdd_flag_index_++;
  std::vector<PsddNode*> sampled_succedents = succedent_manager_->SampleParametersForMultiplePsdds(generator, succedents_, new_flag_index);
  succedent_manager_->DeleteUnusedPsddNodes(sampled_succedents);
  succedents_ = sampled_succedents;
}

}

