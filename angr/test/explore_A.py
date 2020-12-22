#!/usr/bin/env python
# python ./explore_A.py ./testcase

import angr
import sys
import claripy

import time

binary = sys.argv[1]
project = angr.Project(binary)

# start the S.E. at function A taking one symbolic argument
initial_state = project.factory.call_state(project.loader.find_symbol('A').rebased_addr, claripy.BVS('param', 32))
initial_state.memory.store(project.loader.find_symbol('G').rebased_addr, claripy.BVS('G_ptr', 64))
initial_state.memory.store(project.loader.find_symbol('X').rebased_addr, claripy.BVS('X_ptr', 64))

initial_state.mem.types['struct Node'] = angr.types.parse_type('struct Node { int m_uid; int mark_id; unsigned int next_succ_index; int m_succ_uids[5]; unsigned int next_pred_index; int m_pred_uids[5]; }')
initial_state.mem.types['struct Graph'] = angr.types.parse_type('struct Graph { struct Node *m_nodes[5]; }')

def print_states(collection):
  idx=0
  for x in collection:
    print("[%d] 0x%x %s (path length %d)\n  stdout=%s\n  stdin=%s" % (idx, x.addr, project.loader.describe_addr(x.addr), x.history.block_count, x.posix.dumps(1), x.posix.dumps(0)))
    idx += 1
  #endfor
#enddef

def then_d(s):
  return (b'THEN' in s.posix.dumps(sys.stdout.fileno())) and (b'D()' in s.posix.dumps(sys.stdout.fileno()))

def else_d(s):
  return (b'ELSE' in s.posix.dumps(sys.stdout.fileno())) and (b'D()' in s.posix.dumps(sys.stdout.fileno()))

def then_bogus(s):
  return (b'THEN' in s.posix.dumps(sys.stdout.fileno())) and (b'Bogus()' in s.posix.dumps(sys.stdout.fileno()))

def else_bogus(s):
  return (b'ELSE' in s.posix.dumps(sys.stdout.fileno())) and (b'Bogus()' in s.posix.dumps(sys.stdout.fileno()))

def print_graph(state, graph_addr):
  graph_ds = state.mem[graph_addr].struct.Graph
  for i in range(5):
    node_ds = graph_ds.m_nodes[i]
    print(".m_nodes[%d] = %s" % (i, node_ds))
    node_data = node_ds.deref
    print("  .m_uid = %s" % node_data.m_uid)
    print("  .mark_id = %s" % node_data.mark_id)
    print("  .next_succ_index = %s" % node_data.next_succ_index)
    for j in range(5):
      print("    .m_succ_uids[%d] = %s" % (j, node_data.m_succ_uids[j]))
    print("  .next_pred_index = %s" % node_data.next_pred_index)
    for j in range(5):
      print("    .m_pred_uids[%d] = %s" % (j, node_data.m_pred_uids[j]))
  #enddef
#enddef

simulation = project.factory.simulation_manager(initial_state)

simulation.use_technique(angr.exploration_techniques.DFS())

A_addr = project.loader.find_symbol('A').rebased_addr
C_addr = project.loader.find_symbol('C').rebased_addr

# THEN-block address
then_addr = C_addr + 0xc9
# ELSE-block address
else_addr = C_addr + 0x115
# D-block address
d_addr = A_addr + 0xd5
# Bogus-block address
bogus_addr = A_addr + 0xe1

start_time = time.time()

# search!
# print("then-d")
# simulation.explore(find=then_d, avoid=[else_addr, bogus_addr])
# print_states(simulation.found)

# print("else-d")
# simulation.explore(find=else_d, avoid=[then_addr])
# print_states(simulation.found)

# print("then-bogus")
# simulation.explore(find=then_bogus, avoid=[else_addr])
# print_states(simulation.found)

print("else-bogus")
simulation.explore(find=else_bogus, avoid=[then_addr])
print_states(simulation.found)

end_time = time.time()
print("=== TOTAL RUN TIME %s seconds" % (end_time - start_time))
