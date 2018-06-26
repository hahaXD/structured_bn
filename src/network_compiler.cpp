//
// Created by Yujia Shen on 4/6/18.
//

#include <cassert>
#include <unordered_map>
#include <vector>
#include <queue>
#include <iostream>
#include <structured_bn/network_compiler.h>
namespace {
using structured_bn::Cluster;
PsddNode *RecoverPsdd(PsddNode *before_recovery,
                      const std::vector<uint32_t> &condition_variables,
                      const std::vector<PsddNode *> &succedents,
                      PsddManager *target_manager) {
  std::vector<SddLiteral> succedent_variables = vtree_util::VariablesUnderVtree(succedents[0]->vtree_node());
  Vtree *x_node_in_target_manager = nullptr;
  std::vector<Vtree *> serialized_vtrees = vtree_util::SerializeVtree(target_manager->vtree());
  std::unordered_set<SddLiteral> succedent_variables_set(succedent_variables.begin(), succedent_variables.end());
  for (auto v_it = serialized_vtrees.rbegin(); v_it != serialized_vtrees.rend(); ++v_it) {
    Vtree *cur_vnode = *v_it;
    if (sdd_vtree_is_leaf(cur_vnode)) {
      if (succedent_variables_set.find(sdd_vtree_var(cur_vnode)) != succedent_variables_set.end()) {
        sdd_vtree_set_data((void *) 1, cur_vnode);
        x_node_in_target_manager = cur_vnode;
      } else {
        sdd_vtree_set_data(nullptr, cur_vnode);
      }
    } else {
      Vtree *left_child = sdd_vtree_left(cur_vnode);
      Vtree *right_child = sdd_vtree_right(cur_vnode);
      if (sdd_vtree_data(left_child) && sdd_vtree_data(right_child)) {
        sdd_vtree_set_data((void *) 1, cur_vnode);
        x_node_in_target_manager = cur_vnode;
      } else {
        sdd_vtree_set_data(nullptr, cur_vnode);
      }
    }
  }
  for (Vtree *cur_vnode : serialized_vtrees) {
    sdd_vtree_set_data(nullptr, cur_vnode);
  }
  serialized_vtrees = vtree_util::SerializeVtree(before_recovery->vtree_node());
  std::unordered_map<uint32_t, uint32_t> condition_aux_variables;
  auto condition_size = condition_variables.size();
  for (auto i = 0; i < condition_size; ++i) {
    condition_aux_variables[condition_variables[i]] = (uint32_t) i;
  }
  Vtree *x_node = nullptr;
  for (auto v_it = serialized_vtrees.rbegin(); v_it != serialized_vtrees.rend(); ++v_it) {
    Vtree *cur_vnode = *v_it;
    if (sdd_vtree_is_leaf(cur_vnode)) {
      if (condition_aux_variables.find((uint32_t) sdd_vtree_var(cur_vnode)) != condition_aux_variables.end()) {
        sdd_vtree_set_data((void *) 1, cur_vnode);
        x_node = cur_vnode;
      } else {
        sdd_vtree_set_data(nullptr, cur_vnode);
      }
    } else {
      Vtree *left_child = sdd_vtree_left(cur_vnode);
      Vtree *right_child = sdd_vtree_right(cur_vnode);
      if (sdd_vtree_data(left_child) != nullptr && sdd_vtree_data(right_child) != nullptr) {
        sdd_vtree_set_data((void *) 1, cur_vnode);
        x_node = cur_vnode;
      } else {
        sdd_vtree_set_data(nullptr, cur_vnode);
      }
    }
  }
  assert(x_node != nullptr);
  assert(x_node_in_target_manager != nullptr);
  std::vector<PsddNode *> serialized_input = psdd_node_util::SerializePsddNodes(before_recovery);
  for (auto it = serialized_input.rbegin(); it != serialized_input.rend(); ++it) {
    PsddNode *cur_node = *it;
    Vtree *vtree_at_cur_node = cur_node->vtree_node();
    if (sdd_vtree_data(vtree_at_cur_node) != nullptr) {
      if (cur_node->node_type() == LITERAL_NODE_TYPE) {
        PsddLiteralNode *cur_literal_node = cur_node->psdd_literal_node();
        assert(cur_literal_node->sign());
        cur_node->SetUserData(cur_literal_node->variable_index());
      } else if (cur_node->node_type() == TOP_NODE_TYPE) {
        cur_node->SetUserData(0);
      } else {
        //cannot be top node
        assert(cur_node->node_type() == DECISION_NODE_TYPE);
        PsddDecisionNode *cur_decn_node = cur_node->psdd_decision_node();
        const auto &cur_primes = cur_decn_node->primes();
        const auto &cur_subs = cur_decn_node->subs();
        assert(cur_primes.size() == 1);
        PsddNode *cur_prime_node = cur_primes[0];
        PsddNode *cur_sub_node = cur_subs[0];
        uintmax_t cur_node_data = cur_prime_node->user_data() | cur_sub_node->user_data();
        cur_node->SetUserData(cur_node_data);
      }
    } else {
      if (cur_node->node_type() == LITERAL_NODE_TYPE) {
        PsddLiteralNode *cur_literal_node = cur_node->psdd_literal_node();
        PsddNode *new_node = target_manager->GetPsddLiteralNode(cur_literal_node->literal(), 0);
        cur_node->SetUserData((uintmax_t) new_node);
      } else if (cur_node->node_type() == TOP_NODE_TYPE) {
        PsddTopNode *cur_top_node = cur_node->psdd_top_node();
        PsddNode *new_node = target_manager->GetPsddTopNode(cur_top_node->variable_index(),
                                                            0,
                                                            PsddParameter::CreateFromDecimal(1),
                                                            PsddParameter::CreateFromDecimal(1));
        cur_node->SetUserData((uintmax_t) new_node);
      } else {
        PsddDecisionNode *cur_decn_node = cur_node->psdd_decision_node();
        const auto &cur_primes = cur_decn_node->primes();
        const auto &cur_subs = cur_decn_node->subs();
        std::vector<PsddParameter> params(cur_primes.size(), PsddParameter::CreateFromDecimal(1));
        std::vector<PsddNode *> new_primes;
        std::vector<PsddNode *> new_subs;
        auto element_size = cur_primes.size();
        for (auto i = 0; i < element_size; ++i) {
          new_primes.push_back((PsddNode *) cur_primes[i]->user_data());
          new_subs.push_back((PsddNode *) cur_subs[i]->user_data());
        }
        PsddNode *new_node = target_manager->GetConformedPsddDecisionNode(new_primes, new_subs, params, 0);
        cur_node->SetUserData((uintmax_t) new_node);
      }
    }
    if (vtree_at_cur_node == x_node) {
      auto condition_aux_variable_index = cur_node->user_data();
      assert(condition_aux_variable_index != 0);
      auto condition_aux_it = condition_aux_variables.find((uint32_t) condition_aux_variable_index);
      assert(condition_aux_it != condition_aux_variables.end());
      PsddNode *cur_succedent = succedents[condition_aux_it->second];
      PsddNode *new_node = target_manager->LoadPsddNode(x_node_in_target_manager, cur_succedent, 0);
      cur_node->SetUserData((uintmax_t) new_node);
    }
  }
  auto result_node = (PsddNode *) before_recovery->user_data();
  PsddNode *new_node = target_manager->NormalizePsddNode(target_manager->vtree(), result_node, 0);
  for (PsddNode *cur_node : serialized_input) {
    cur_node->SetUserData(0);
  }
  for (Vtree *vnode : serialized_vtrees) {
    sdd_vtree_set_data(nullptr, vnode);
  }
  return new_node;
}

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
    Vtree *right_child = ConstructVtree(remaining_clusters, vtree_per_cluster);
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
Vtree *VtreeSwapVariables(Vtree *orig_vtree, std::unordered_map<SddLiteral, SddLiteral> &variable_map) {
  std::vector<Vtree *> serialized_vtrees = vtree_util::SerializeVtree(orig_vtree);
  for (auto vit = serialized_vtrees.rbegin(); vit != serialized_vtrees.rend(); ++vit) {
    Vtree *cur_node = *vit;
    Vtree *new_node = nullptr;
    if (sdd_vtree_is_leaf(cur_node)) {
      SddLiteral cur_leaf_variable = sdd_vtree_var(cur_node);
      assert(variable_map.find(cur_leaf_variable) != variable_map.end());
      new_node = new_leaf_vtree(variable_map[cur_leaf_variable]);
    } else {
      Vtree *left_child = sdd_vtree_left(cur_node);
      Vtree *right_child = sdd_vtree_right(cur_node);
      auto new_left_child = (Vtree *) sdd_vtree_data(left_child);
      auto new_right_child = (Vtree *) sdd_vtree_data(right_child);
      sdd_vtree_set_data(nullptr, left_child);
      sdd_vtree_set_data(nullptr, right_child);
      new_node = new_internal_vtree(new_left_child, new_right_child);
    }
    sdd_vtree_set_data((void *) new_node, cur_node);
  }
  auto result = (Vtree *) sdd_vtree_data(orig_vtree);
  sdd_vtree_set_data(nullptr, orig_vtree);
  set_vtree_properties(result);
  return result;
}

// expand_variable should be the right most variable in the carrier. Just santiy check
Vtree *ExpandRightMostNode(Vtree *carrier, Vtree *follower, SddLiteral expand_variable) {
  if (sdd_vtree_is_leaf(carrier)) {
    assert(sdd_vtree_var(carrier) == expand_variable);
    Vtree *result = vtree_util::CopyVtree(follower);
    return result;
  }
  Vtree *copy_follower = vtree_util::CopyVtree(follower);
  Vtree *vit = carrier;
  while (!sdd_vtree_is_leaf(sdd_vtree_right(vit))) {
    vit = sdd_vtree_right(vit);
  }
  assert(sdd_vtree_var(sdd_vtree_right(vit)) == expand_variable);
  Vtree *vit_copy = new_internal_vtree(vtree_util::CopyVtree(sdd_vtree_left(vit)), copy_follower);
  while (sdd_vtree_parent(vit) != nullptr) {
    vit = sdd_vtree_parent(vit);
    vit_copy = new_internal_vtree(vtree_util::CopyVtree(sdd_vtree_left(vit)), vit_copy);
  }
  set_vtree_properties(vit_copy);
  return vit_copy;
}

}
namespace structured_bn {
NetworkCompiler::NetworkCompiler(structured_bn::Network *network,
                                 PsddManager *joint_psdd_manager)
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

PsddNode *NetworkCompiler::ConformLocalPsdd(Cluster *target_cluster,
                                            PsddManager *target_manager) {
  if (target_cluster->parent_clusters().empty()) {
    // root cluster.
    assert(target_cluster->succedents().size() == 1);
    PsddNode *cluster_psdd_node = target_cluster->succedents()[0];
    cluster_psdd_node = target_manager->LoadPsddNode(target_manager->vtree(), cluster_psdd_node, 0);
    return cluster_psdd_node;
  }
  std::vector<SddLiteral> antecedent_variables = vtree_util::VariablesUnderVtree(target_cluster->antecedent_vtree());
  std::vector<SddLiteral> succedent_variables = vtree_util::VariablesUnderVtree(target_cluster->succedent_vtree());
  std::unordered_map<SddLiteral, SddLiteral> antecedent_variables_to_sdd_index;
  uint32_t sdd_variable_size = 0;
  for (SddLiteral cur_antecedent_variable : antecedent_variables) {
    antecedent_variables_to_sdd_index[cur_antecedent_variable] = ++sdd_variable_size;
  }
  // Construct the sdd vtree
  Vtree *sdd_vtree = nullptr;
  std::unordered_map<SddLiteral, SddLiteral> condition_temp_index_to_sdd_index;
  {
    // pick a succedent_variable be the place holder
    auto picked_succedent_variable = succedent_variables[0];
    auto picked_succedent_variable_tempory_sdd_index = sdd_variable_size + 1;
    // temporary add picked succedent variables in antecedent variables for projection purpose. It will be poped later.
    antecedent_variables.push_back(picked_succedent_variable);
    Vtree *projected_vtree_on_antecedent_variables =
        vtree_util::ProjectVtree(target_manager->vtree(), antecedent_variables);
    antecedent_variables.pop_back();
    antecedent_variables_to_sdd_index[picked_succedent_variable] = picked_succedent_variable_tempory_sdd_index;
    Vtree
        *carrier_vtree = VtreeSwapVariables(projected_vtree_on_antecedent_variables, antecedent_variables_to_sdd_index);
    sdd_vtree_free(projected_vtree_on_antecedent_variables);
    auto condition_size = target_cluster->succedents().size();
    for (auto i = 0; i < condition_size; ++i) {
      condition_temp_index_to_sdd_index[i + 1] = ++sdd_variable_size;
    }
    Vtree *balanced_vtree = sdd_vtree_new(condition_size, "balanced");
    Vtree *follower_vtree = VtreeSwapVariables(balanced_vtree, condition_temp_index_to_sdd_index);
    sdd_vtree_free(balanced_vtree);
    sdd_vtree = ExpandRightMostNode(carrier_vtree, follower_vtree, picked_succedent_variable_tempory_sdd_index);
    sdd_vtree_free(follower_vtree);
    sdd_vtree_free(carrier_vtree);
  }
  // Construct the SDD logic
  SddNode *complete_logic = nullptr;
  SddManager *sdd_manager = nullptr;
  {
    sdd_manager = sdd_manager_new(sdd_vtree);
    sdd_vtree_free(sdd_vtree);
    sdd_vtree = sdd_manager_vtree(sdd_manager);
    sdd_manager_auto_gc_and_minimize_off(sdd_manager);
    complete_logic = sdd_manager_false(sdd_manager);
    sdd_ref(complete_logic, sdd_manager);
    auto condition_size = target_cluster->succedents().size();
    const auto &cur_cluster_antecedents = target_cluster->antecedents();
    for (auto k = 0; k < condition_size; ++k) {
      PsddNode *cur_antecedent = cur_cluster_antecedents[k];
      std::vector<PsddNode *> serialized_psdd_nodes = psdd_node_util::SerializePsddNodes(cur_antecedent);
      SddNode *cur_antecedent_sdd =
          psdd_node_util::ConvertPsddNodeToSddNode(serialized_psdd_nodes, antecedent_variables_to_sdd_index, sdd_manager);
      SddNode *cur_constraint = sdd_conjoin(cur_antecedent_sdd,
                                            sdd_manager_literal(condition_temp_index_to_sdd_index[k + 1], sdd_manager),
                                            sdd_manager);
      sdd_deref(complete_logic, sdd_manager);
      complete_logic = sdd_disjoin(complete_logic, cur_constraint, sdd_manager);
      sdd_ref(complete_logic, sdd_manager);
      sdd_manager_garbage_collect(sdd_manager);
    }
  }
  // Construct the PSDD
  PsddNode *result_psdd = nullptr;
  {
    std::unordered_map<uint32_t, uint32_t> psdd_manager_variable_mapping;
    uint32_t psdd_variable_index_offset = 0;
    for (auto cache_pair : antecedent_variables_to_sdd_index) {
      psdd_manager_variable_mapping[(uint32_t) cache_pair.second] = (uint32_t) cache_pair.first;
      psdd_variable_index_offset = psdd_variable_index_offset > (uint32_t) cache_pair.first ? psdd_variable_index_offset
                                                                                            : (uint32_t) cache_pair.first;
    }
    std::vector<uint32_t> condition_variables;
    auto condition_size = target_cluster->succedents().size();
    for (auto i = 0; i < condition_size; ++i) {
      uint32_t cur_condition_psdd_variable_index = ++psdd_variable_index_offset;
      condition_variables.push_back(cur_condition_psdd_variable_index);
    }
    for (auto cache_pair : condition_temp_index_to_sdd_index) {
      psdd_manager_variable_mapping[(uint32_t) cache_pair.second] = condition_variables[cache_pair.first - 1];
    }
    PsddManager *psdd_manager_with_condition_variables =
        PsddManager::GetPsddManagerFromSddVtree(sdd_vtree, psdd_manager_variable_mapping);
    PsddNode *conformed_psdd_with_condition_variables = psdd_manager_with_condition_variables->ConvertSddToPsdd(
        complete_logic,
        sdd_vtree,
        0,
        psdd_manager_variable_mapping);
    result_psdd = RecoverPsdd(conformed_psdd_with_condition_variables,
                              condition_variables,
                              target_cluster->succedents(),
                              target_manager);
    delete psdd_manager_with_condition_variables;
  }
  sdd_manager_free(sdd_manager);
  std::vector<uint32_t> succedent_aux_variables;
  return result_psdd;
}

std::pair<PsddNode *, Probability> NetworkCompiler::Compile() {
  const auto &clusters = network_->clusters();
  PsddNode *accumulator = joint_psdd_manager_->GetTrueNode(joint_psdd_manager_->vtree(), 0);
  Probability partition = Probability::CreateFromDecimal(1);
  Probability zero_prob = Probability::CreateFromDecimal(1);
  std::bitset<MAX_VAR> mask;
  mask.set();
  std::bitset<MAX_VAR> zero_instantiation;
  zero_instantiation.reset();
  std::vector<Cluster*> top_clusters = network_->ArbitraryTopologicalOrder();
  for (Cluster *cur_cluster : top_clusters) {
    PsddNode *psdd_node = ConformLocalPsdd(cur_cluster, joint_psdd_manager_);
    Probability local_zero_prob = psdd_node_util::Evaluate(mask, zero_instantiation, psdd_node);
    zero_prob = local_zero_prob * zero_prob;
    auto multiply_result = joint_psdd_manager_->Multiply(accumulator, psdd_node, 0);
    partition = partition * multiply_result.second;
    std::cout << "Cluster name " << network_->cluster_names()[cur_cluster->cluster_index()]<<std::endl;
    std::cout << "Partition function " << partition.parameter()/log(2) << std::endl;
    std::cout << "variables inside this cluster " << sdd_vtree_var_count(cur_cluster->succedent_vtree()) << std::endl;
    accumulator = multiply_result.first;
    joint_psdd_manager_->DeleteUnusedPsddNodes({accumulator});
  }
  return {accumulator, partition};
}
NetworkCompiler::~NetworkCompiler() {
  delete (joint_psdd_manager_);
}
Vtree *NetworkCompiler::GetVtree() const {
  return joint_psdd_manager_->vtree();
}

}
