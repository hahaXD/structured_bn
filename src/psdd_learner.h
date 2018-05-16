//
// Created by Jason Shen on 5/16/18.
//

#ifndef STRUCTURED_BN_PSDD_LEARNER_H
#define STRUCTURED_BN_PSDD_LEARNER_H

#include <src/psdd_node.h>
#include <src/psdd_manager.h>
#include <stack>

PsddNode *LearnPsdd(PsddNode *root_node, BinaryData *data, PsddManager *manager, PsddParameter alpha) {
  std::vector<PsddNode *> serialized_nodes = psdd_node_util::SerializePsddNodes(root_node);
  const auto &training_data = data->data();
  for (PsddNode *cur_node : serialized_nodes) {
    cur_node->ResetDataCount();
  }
  uintmax_t total_data = 0;
  uintmax_t valid_data = 0;
  for (const auto &data_pair : training_data) {
    const auto &cur_data = data_pair.first;
    uintmax_t cur_data_freq = data_pair.second;
    total_data += cur_data_freq;
    const auto &serialized_succedents = serialized_nodes;
    psdd_node_util::SetActivationFlag(cur_data, serialized_succedents);
    if (!root_node->activation_flag()) {
      for (PsddNode *cur_node : serialized_succedents) {
        cur_node->ResetActivationFlag();
      }
      continue;
    } else {
      // increment data count
      valid_data += cur_data_freq;
      std::stack<PsddNode *> explore_list;
      explore_list.push(root_node);
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
  for (auto it = serialized_nodes.rbegin(); it != serialized_nodes.rend(); ++it) {
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
      PsddNode *new_top_node = manager->GetPsddTopNode(cur_top_node->variable_index(),
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
      PsddNode *new_decn_node = manager->GetConformedPsddDecisionNode(next_primes,
                                                                      next_subs,
                                                                      next_parameters,
                                                                      cur_node->flag_index());
      cur_decision_node->SetUserData((uintmax_t) new_decn_node);
    }
  }
  auto new_root_node = (PsddNode *) root_node->user_data();
  for (PsddNode *old_node : serialized_nodes) {
    old_node->SetUserData(0);
  }
  return new_root_node;
}

#endif //STRUCTURED_BN_PSDD_LEARNER_H
