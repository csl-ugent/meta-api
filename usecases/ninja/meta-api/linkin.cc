/* so we can access private members within STL data structures */
#define private public

#include <src/build.h>

/* functions that are too complex */
typedef _Rb_tree<Edge*, Edge*, std::_Identity<Edge*>, std::less<Edge*>, std::allocator<Edge*> > Tree;

__attribute__((used,noinline))
Tree *__DIABLO_META_API__constructor_tree() {
  return new Tree();
}

/* _M_insert_unique returns some value on the stack (return value opt (RVO)? copy elision?),
 * which we can't handle in the meta-API for now. So let the compiler do the work. */
__attribute__((used,noinline))
std::_Rb_tree_node_base* __DIABLO_META_API___M_insert_unique(Tree *tree, Edge *e) {
  std::pair<std::_Rb_tree_iterator<Edge*>, bool> pair = tree->_M_insert_unique(e);
  return pair.first._M_node;
}

/* same. Returns NULL when 'e' is not present in the tree */
__attribute__((used,noinline))
std::_Rb_tree_node_base* __DIABLO_META_API___M_get_insert_unique_pos(Tree *tree, Edge *e) {
  std::pair<std::_Rb_tree_node_base*, std::_Rb_tree_node_base*> pair = tree->_M_get_insert_unique_pos(e);
  return pair.first;
}

__attribute__((constructor(65500)))
void __DIABLO_META_API__init() {
  return;
}
