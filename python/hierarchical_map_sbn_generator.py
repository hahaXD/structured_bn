import json
import tempfile
from graphillion import GraphSet
import gmplot
import sys

class Edge(object):
    def __init__(self,x,y,name):
        if cmp(x,y) > 0: x,y = y,x
        self.x = x
        self.y = y
        self.name = name
        self.nodes = set([x,y])
    def OtherEnd(self, input_end):
        if self.x == input_end:
            return self.y
        else:
            return self.x
    def global_id(self):
        return "e%d" % self.name

    def as_tuple(self):
        return (self.x,self.y)

    def __repr__(self):
        return "%s (%s,%s)" % (self.global_id(),str(self.x),str(self.y))

    def __cmp__(self,other):
        return cmp(self.name,other.name)

    def __eq__ (self, other):
        return (self.x, self.y, self.name) == (other.x, other.y, other.name)

    def __hash__(self):
        return hash((self.x, self.y, self.name))

class Graph(object):
    def __init__(self, edge_list):
        self.edge_list = edge_list
        self.node_to_edges = {}
        for idx,cur_node_pair in enumerate(edge_list):
            cur_edge = Edge(cur_node_pair[0], cur_node_pair[1], idx+1)
            self.node_to_edges.setdefault(cur_node_pair[0], []).append(cur_edge)
            self.node_to_edges.setdefault(cur_node_pair[1], []).append(cur_edge)

class HmCluster (object):
    @staticmethod
    def GetLeaveCluster(name, node_indexes, graph, cluster_node_indexes):
        a = HmCluster(name);
        if name in cluster_node_indexes:
            a.cluster_variable = cluster_node_indexes[name]
        a.nodes = set(node_indexes)
        a.internal_edges = set()
        a.external_edges = {}
        for node_index in node_indexes:
            neighboring_edges = graph.node_to_edges[node_index]
            for cur_neighboring_edge in neighboring_edges:
                cur_neighboring_node = cur_neighboring_edge.OtherEnd(node_index)
                if cur_neighboring_node in a.nodes:
                    a.internal_edges.add(cur_neighboring_edge)
                else:
                    a.external_edges[cur_neighboring_edge] = node_index
        return a
    @staticmethod
    def GetInternalCluster(name, child_clusters, graph, cluster_node_indexes):
        a = HmCluster(name);
        if name in cluster_node_indexes:
            a.cluster_variable = cluster_node_indexes[name]
        a.nodes = set.union(*[child.nodes for child in child_clusters])
        a.children = {}
        a.internal_edges = set()
        a.external_edges = {} # key is the Edge and value is the connection child region
        a.sub_region_edges = {}
        a.children_variables = {}
        node_to_child = {}
        for child_cluster in child_clusters:
            a.children[child_cluster.name] = child_cluster
            a.children_variables[child_cluster.name] = cluster_node_indexes[child_cluster.name]
            for cur_node in child_cluster.nodes:
                node_to_child[cur_node] = child_cluster
        for child_cluster in child_clusters:
            for cur_node_index in child_cluster.nodes:
                neighboring_edges = graph.node_to_edges[cur_node_index]
                for cur_neighboring_edge in neighboring_edges:
                    cur_neighboring_node = cur_neighboring_edge.OtherEnd(cur_node_index)
                    if cur_neighboring_node not in child_cluster.nodes and cur_neighboring_node in a.nodes:
                        #internal edges
                        a.internal_edges.add(cur_neighboring_edge)
                        a_region_name = child_cluster.name
                        b_region_name = node_to_child[cur_neighboring_node].name
                        edge_tuple = (min(a_region_name, b_region_name), max(a_region_name, b_region_name))
                        node_to_child.setdefault(edge_tuple, []).append(cur_neighboring_edge)
                    elif cur_neighboring_node not in a.nodes:
                        #external edges
                        a.external_edges[cur_neighboring_edge] = child_cluster.name
                    else:
                        pass
        return a
    @staticmethod
    def GetCluster(cluster_name, hm_clusters, graph, cluster_node_indexes, cache):
        if cluster_name in cache:
            return cache[cluster_name]
        if "nodes" in hm_clusters[cluster_name]:
            result  = HmCluster.GetLeaveCluster(cluster_name, hm_clusters[cluster_name]["nodes"], graph, cluster_node_indexes)
            cache[cluster_name] = result
            return result
        else:
            child_clusters = [HmCluster.GetCluster(child_cluster_name, hm_clusters, graph, cluster_node_indexes, cache) for child_cluster_name in hm_clusters[cluster_name]["sub_clusters"]]
            result = HmCluster.GetInternalCluster(cluster_name, child_clusters, graph, cluster_node_indexes)
            cache[cluster_name] = result
            return result
    def GetLocalConstraints(self):
        if len(self.external_edges) == 0:
            # root variable
            return self.GetLocalConstraintsForRoot()
        elif self.children:
            # internal variable
            return self.GetLocalConstraintsForInternalClusters()
        else:
            # leaf variable
            return self.GetLocalConstraintsForLeaveClusters()
    def GetLocalConstraintsForRoot(self):
        universe = []
        pass
    def GetLocalConstraintsForLeaveClusters(self):
        pass
    def GetLocalConstraintsForInternalClusters(self):
        pass
    def __init__(self, name):
        self.name = name
        self.nodes = None
        self.children = None
        self.internal_edges = None
        self.external_edges = None
        self.sub_region_edges = None
        self.children_variables = None
        self.cluster_variable = None

class HmNetwork(object):
    def __init__(self):
        self.clusters = {}
        pass
    @staticmethod
    def ReadHmSpec(hm_spec):
        graph = Graph(hm_spec["edges"])
        cluster_spec = hm_spec["clusters"]
        cluster_node_indexes = {}
        variable_index = len(graph.edge_list) + 1
        for cluster_name in cluster_spec:
            cur_cluster_spec = cluster_spec[cluster_name]
            if "sub_clusters" in cur_cluster_spec:
                for sub_cluster_name in cur_cluster_spec["sub_clusters"]:
                    cluster_node_indexes[sub_cluster_name] = variable_index
                    variable_index += 1
        result = HmNetwork()
        for cluster_name in cluster_spec:
            if cluster_name not in result.clusters:
                HmCluster.GetCluster(cluster_name, cluster_spec, graph, cluster_node_indexes, result.clusters)
        return result

    def TopologicalSort(self):
        leave_to_root_order = []
        node_queue = [self.clusters[cluster_name] for cluster_name in self.clusters]
        while len(node_queue) > 0:
            leave_nodes = [x for x in node_queue if x.children == None or all( x.children[p] in leave_to_root_order for p in x.children)]
            node_queue = [x for x in node_queue if x not in leave_nodes]
            leave_to_root_order.extend(leave_nodes)
        return leave_to_root_order

    def write_hierarchy_to_dot(self, dot_filename):
        dot_file_content = "digraph g {\n"
        for cluster_name in self.clusters:
            cur_cluster = self.clusters[cluster_name]
            if cur_cluster.children == None:
                continue
            for child_cluster_name in cur_cluster.children:
                child_cluster = cur_cluster.children[child_cluster_name]
                dot_file_content += "%s -> %s\n" % (cluster_name, child_cluster.name)
        dot_file_content += "}"
        with open(dot_filename , "w") as fp:
            fp.write(dot_file_content)


with open (sys.argv[1], "r") as fp:
    hm_spec = json.load(fp)

network = HmNetwork.ReadHmSpec(hm_spec)
cluster_leave_to_root = network.TopologicalSort()
for cluster in cluster_leave_to_root:
    print cluster.name
#network.write_hierarchy_to_dot("test.dot")
for cluster_name in network.clusters:
    cluster = network.clusters[cluster_name]
    print ("Cluster name : %s \n Internal Edges : %s \n External Edges : %s Child Cluster Index : %s Current Cluster Index : %s \n \n \n \n" % (cluster.name, cluster.internal_edges, cluster.external_edges, cluster.cluster_variable, cluster.children_variables))
