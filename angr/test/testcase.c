// gcc -o testcase testcase.c

#include <stdlib.h>
#include <stdio.h>

// workaround to use bool in C
#define bool int
#define true 1
#define false 0

// debug functionality, only to test this program's correctness
#ifdef PRINT
#define debug(x) graph_print(x)
#else
#define debug(x)
#endif

/*  Simple graph implementation.
    Each node has a unique identifier (uid), can record MAX_SUCCS successors and MAX_PREDS predecessors. Both are stored in fixed-length arrays.
    A graph consists of set of nodes stored in a fixed-length array of size MAX_NODES. A node's uid is used to index the graph's fixed-length node array.
 */
#define MAX_SUCCS 5
#define MAX_PREDS 5
#define MAX_NODES 5

struct Node;
typedef struct Node Node;

struct Graph;
typedef struct Graph Graph;

typedef int T_UID;
#define INVALID_UID -1

static int g_mark_id = 0;

struct Node {
  T_UID m_uid;
  int mark_id;

  unsigned int next_succ_index;
  T_UID m_succ_uids[MAX_SUCCS];

  unsigned int next_pred_index;
  T_UID m_pred_uids[MAX_PREDS];
};

struct Graph {
  Node *m_nodes[MAX_NODES];
};

/* Create (zero-initialise) a node with 'uid' on the heap. */
Node *node_create(T_UID uid) {
  Node *n = (Node *)malloc(sizeof(Node));

  n->m_uid = uid;
  n->mark_id = -1;

  n->next_succ_index = 0;
  for (int i = 0; i < MAX_SUCCS; i++)
    n->m_succ_uids[i] = INVALID_UID;

  n->next_pred_index = 0;
  for (int i = 0; i < MAX_PREDS; i++)
    n->m_pred_uids[i] = INVALID_UID;

  return n;
}

/* Create a copy of a node on the heap. */
Node *node_copy(Node *original) {
  Node *n = (Node *)malloc(sizeof(Node));

  n->m_uid = original->m_uid;
  n->mark_id = -1;

  n->next_succ_index = original->next_succ_index;
  for (int i = 0; i < MAX_SUCCS; i++)
    n->m_succ_uids[i] = original->m_succ_uids[i];
  
  n->next_pred_index = original->next_pred_index;
  for (int i = 0; i < MAX_PREDS; i++)
    n->m_pred_uids[i] = original->m_pred_uids[i];

  return n;
}

/* Create (zero-initialise) a graph on the heap. */
Graph *graph_create() {
  Graph *g = (Graph *)malloc(sizeof(Graph));

  for (int i = 0; i < MAX_NODES; i++)
    g->m_nodes[i] = NULL;

  return g;
}

/* Create a deep copy of a graph on the heap. */
Graph *graph_copy(Graph *original) {
  Graph *g = (Graph *)malloc(sizeof(Graph));

  for (int i = 0; i < MAX_NODES; i++)
    g->m_nodes[i] = node_copy(original->m_nodes[i]);

  return g;
}

/* Add a node with 'uid' to graph 'g', but only if no such node exists yet. */
Node *graph_add_node(Graph *g, T_UID uid) {
  // bounds check
  if (uid < 0 || uid >= MAX_NODES)
    return NULL;

  if (g->m_nodes[uid] == NULL)
    g->m_nodes[uid] = node_create(uid);

  return g->m_nodes[uid];
}

/*  Connect node 'from_uid' and node 'to_uid' with an edge going from the former to the latter
    by modifying de nodes' successor and predecessor arrays.
    Don't do anything if such edge exists already (no duplicate edges are created).
 */
void graph_add_edge(Graph *g, T_UID from_uid, T_UID to_uid) {
  // bounds check
  if (from_uid < 0 || from_uid >= MAX_NODES)
    return;
  if (to_uid < 0 || to_uid >= MAX_NODES)
    return;

  // node existence check
  if (g->m_nodes[from_uid] == NULL)
    return;
  if (g->m_nodes[to_uid] == NULL)
    return;

  // don't introduce duplicate edges
  Node *from = g->m_nodes[from_uid];
  for (int i = 0; i < from->next_succ_index; i++) {
    if (from->m_succ_uids[i] == to_uid)
      return;
  }
  
  // add edge
  Node *to = g->m_nodes[to_uid];

  from->m_succ_uids[from->next_succ_index] = to_uid;
  from->next_succ_index++;

  to->m_pred_uids[to->next_pred_index] = from_uid;
  to->next_pred_index++;
}

/* Remove the edge from node 'from_uid' to node 'to_uid'. */
void graph_del_edge(Graph *g, T_UID from_uid, T_UID to_uid) {
  // bounds check
  if (from_uid < 0 || from_uid >= MAX_NODES)
    return;
  if (to_uid < 0 || to_uid >= MAX_NODES)
    return;

  // node existence check
  if (g->m_nodes[from_uid] == NULL)
    return;
  if (g->m_nodes[to_uid] == NULL)
    return;

  // delete edge
  Node *from = g->m_nodes[from_uid];
  for (int i = 0; i < from->next_succ_index; i++) {
    if (from->m_succ_uids[i] == to_uid) {
      from->m_succ_uids[i] = INVALID_UID;
      break;
    }
  }

  Node *to = g->m_nodes[to_uid];
  for (int i = 0; i < to->next_pred_index; i++) {
    if (to->m_pred_uids[i] == from_uid) {
      to->m_pred_uids[i] = INVALID_UID;
      break;
    }
  }
}

/*  Get the node with 'uid' from a graph.
    Returns NULL when no such node exists.
 */
Node *graph_get_node(Graph *g, T_UID uid) {
  if (uid < 0 || uid >= MAX_NODES)
    return NULL;
  
  return g->m_nodes[uid];
}

/*  Check whether two nodes 'a' and 'b' are adjacent in some graph 'g'.
    Returns TRUE when both are connected, FALSE otherwise.
 */
bool graph_adjacent(Graph *g, Node *a, Node *b) {
  T_UID b_uid = b->m_uid;

  // successor?
  for (int i = 0; i < a->next_succ_index; i++) {
    if (a->m_succ_uids[i] == INVALID_UID)
      continue;
    if (a->m_succ_uids[i] == b_uid)
      return true;
  }

  // predecessor?
  for (int i = 0; i < a->next_pred_index; i++) {
    if (a->m_pred_uids[i] == INVALID_UID)
      continue;
    if (a->m_pred_uids[i] == b_uid)
      return true;
  }

  // neither successor nor predecessor
  return false;
}

/*  Compute the set of nodes in graph 'g' that are reachable from some 'root'.
    The resulting set is returned in the variable declared below as 'reachable_from_result',
    with the number of valid entries in 'nr_reachable'.
 */
Node *reachable_from_result[MAX_NODES];
Node **graph_reachable_from(Graph *g, Node *root, int *nr_reachable) {
  int reachable_from_result_index = 0;

  g_mark_id++;

  Node *queue[MAX_NODES];
  int queue_index = -1;

  // queue the root node
  queue_index++;
  queue[queue_index] = root;

  while (queue_index >= 0) {
    // pop from queue
    Node *node = queue[queue_index];
    queue_index--;

    // mark this node and add it to the result list
    node->mark_id = g_mark_id;
    reachable_from_result[reachable_from_result_index] = node;
    reachable_from_result_index++;

    // add any non-marked successors to the queue
    for (int i = 0; i < node->next_succ_index; i++) {
      int succ_uid = node->m_succ_uids[i];

      // skip invalid entries
      if (succ_uid == INVALID_UID)
        continue;

      // get the node and queue unmarked successors
      Node *succ = g->m_nodes[succ_uid];
      if (succ->mark_id != g_mark_id) {
        queue_index++;
        queue[queue_index] = succ;
      }
    }
  }

  *nr_reachable = reachable_from_result_index;
  return reachable_from_result;
}

/*  Compute the set of neighboring nodes in graph 'g' for some 'node'.
    The resulting set is returned in the variable declared below as 'neighbors_result',
    with the number of valid entries in 'nr_reachale'.
 */
Node *neighbors_result[MAX_NODES];
Node **graph_get_neighbors(Graph *g, Node *node, int *nr_neighbors) {
  int neighbors_result_index = 0;

  g_mark_id++;

  // iterate predecessors
  for (int i = 0; i < node->next_pred_index; i++) {
    int pred_uid = node->m_pred_uids[i];

    // skip invalid entries
    if (pred_uid == INVALID_UID)
      continue;
    
    // add unmarked predecessors
    Node *pred = g->m_nodes[pred_uid];
    if (pred->mark_id != g_mark_id) {
      neighbors_result[neighbors_result_index] = pred;
      neighbors_result_index++;

      pred->mark_id = g_mark_id;
    }
  }

  // iterate successors
  for (int i = 0; i < node->next_succ_index; i++) {
    int succ_uid = node->m_succ_uids[i];

    if (succ_uid == INVALID_UID)
      continue;
    
    Node *succ = g->m_nodes[succ_uid];
    if (succ->mark_id != g_mark_id) {
      neighbors_result[neighbors_result_index] = succ;
      neighbors_result_index++;

      succ->mark_id = g_mark_id;
    }
  }

  *nr_neighbors = neighbors_result_index;
  return neighbors_result;
}

/* Debug functionality. Needs explicit compilation with '-DPRINT' to enable. */
void graph_print(Graph *g) {
  printf("=== GRAPH\n");

  int nr_nodes = 0;
  for (int i = 0; i < MAX_NODES; i++) {
    Node *node = g->m_nodes[i];
    if (node == NULL)
      continue;

    nr_nodes++;
    
    // node uid
    printf("node %d: ", i);

    // predecessors
    printf("predecessors(");
    for (int j = 0; j < node->next_pred_index; j++) {
      if (node->m_pred_uids[j] == INVALID_UID)
        continue;

      printf("%d, ", node->m_pred_uids[j]);
    }
    printf(") ");

    // successors
    printf("successors(");
    for (int j = 0; j < node->next_succ_index; j++) {
      if (node->m_succ_uids[j] == INVALID_UID)
        continue;

      printf("%d, ", node->m_succ_uids[j]);
    }
    printf(") ");

    printf("\n");
  }

  printf("=== %d nodes\n", nr_nodes);
}

/* ACTUAL PROGRAM */
Graph *G, *X;
Node *K, *L;

/* First possible outcome. */
void D() {
  printf("D()\n");
}

/* Second possible outcome. */
void Bogus() {
  printf("Bogus()\n");
}

/* Work function */
void C(Graph *G, Graph *X, Node *root, bool param) {
  int x = 0;

  int nr_reachable = 0;
  Node **reachable = graph_reachable_from(G, root, &nr_reachable);
  for (int i = 0; i < nr_reachable; i++) {
    Node *n = reachable[i];

    int nr_neighbors = 0;
    Node **neighbors = graph_get_neighbors(G, n, &nr_neighbors);
    for (int j = 0; j < nr_neighbors; j++) {
      Node *m = neighbors[j];

      if (/*...*/param) {
        printf("THEN\n");
        graph_del_edge(G, n->m_uid, m->m_uid);
        //...
        graph_add_edge(X, K->m_uid, K->m_uid);
        x++;
      }
      else {
        printf("ELSE\n");
        L = graph_get_node(X, m->m_uid);
        //...
        graph_add_edge(G, m->m_uid, n->m_uid);
      }
    }
  }

  //int temp2 = G.do_something()
  //G.do_something_else(temp2)
}

/* Work function. */
void A(bool param) {
  // SYMBOLIC PARAMETER
  int temp1; scanf("%d", &temp1);

  K = graph_add_node(X, temp1);

  // sanity check to prevent symbolic execution from doing crazy things with NULL pointers ('temp1' out of allowed range).
  if (K == NULL)
    return;

  L = graph_get_node(G, 0);
  debug(G);

  C(G, X, graph_get_node(G, 0), param);
  debug(G);

  /* This is an opaque predicate for cases where G and X point to different graphs.
   When pointing to the same graph instances, both 'then' and 'else' cases can occur, depending on the input parameters.
  */
  if (!graph_adjacent(G, K, L)) {
    D();
  }
  else {
    Bogus();
  }
}

int main(int argc, char** argv) {
  // SYMBOLIC PARAMETER 'param'
  int temp2; scanf("%d", &temp2);
  bool param = (temp2 != 0) ? true : false;

  /* input-independent program initialisation
   Creates the following graph:
         0
        / \
       1   2
       |
       3
  */
  G = graph_create();
  debug(G);

  // add nodes
  graph_add_node(G, 0);
  graph_add_node(G, 1);
  graph_add_node(G, 2);
  graph_add_node(G, 3);
  debug(G);

  // add edges
  graph_add_edge(G, 0, 1);
  graph_add_edge(G, 0, 2);
  graph_add_edge(G, 1, 3);
  debug(G);

  X = graph_copy(G);

  // FUNCTION CALL
  A(param);

  return 0;
}
