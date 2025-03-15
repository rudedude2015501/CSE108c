#pragma once

#include <algorithm>
#include <deque>
#include <vector>

#define BTREE_DEBUG 1

#if defined(BTREE_DEBUG)
#include <sstream>
#endif

template <typename TSortKey> class BTree {
public:
  struct Block {
    uint16_t order;
    size_t key_count;
    TSortKey* keys;
    Block** children;

    Block(uint16_t order) : order(order), key_count(0), keys(nullptr), children(nullptr) {
      keys = new TSortKey[order - 1]();
      children = new Block*[order]();
      for (size_t i = 0; i < order; i++) {
        children[i] = nullptr;
      }
    }

    ~Block() {
      delete[] keys;
      delete[] children;
    }

    size_t child_count() const {
      size_t count = 0;
      for (size_t i = 0; i < order; i++) {
        if (children[i] != nullptr) {
          count++;
        } else {
          break;
        }
      }
      return count;
    }

    bool is_leaf() const { return child_count() == 0; }

#if defined(BTREE_DEBUG)
    std::string to_string() const {
      std::ostringstream oss;
      oss << "[";
      for (size_t i = 0; i < order - 1; i++) {
        if (i < key_count) {
          oss << keys[i];
        } else {
          oss << "null";
        }
        if (i < order - 2) {
          oss << ", ";
        }
      }
      oss << "]";
      return oss.str();
    }
#endif
  };

protected:
  size_t _order;
  Block* _root;

  size_t node_max_keys() const { return _order - 1; }
  size_t node_min_keys() const { return _order / 2 - 1; }
  size_t node_max_children() const { return _order; }
  size_t node_min_children() const { return _order / 2; }
  bool node_is_full(Block* node) const { return node->key_count == node_max_keys(); }

  Block* empty_node() { return new Block(_order); }

  Block* insert_non_full(Block* node, TSortKey key) {
    // find the index of the key to insert
    size_t i = 0;
    while (i < node->key_count && key > node->keys[i]) {
      i++;
    }

    // if the node is a leaf, insert the key directly
    if (node->is_leaf()) {
      // shift the keys to the right
      for (size_t j = node->key_count; j > i; j--) {
        node->keys[j] = node->keys[j - 1];
      }
      // insert the key
      node->keys[i] = key;
      node->key_count++;
      return node;
    } else {
      // get the child node
      auto child = node->children[i];
      // check if the child has space
      if (!node_is_full(child)) {
        // insert the key in child node
        child = insert_non_full(child, key);
        return node;
      } else {
        // split the child node
        node = split_child(node, i, child);
        // determine which child to insert the key
        if (key > node->keys[i]) {
          i++;
          child = node->children[i];
        }
        // insert the key in the child node
        child = insert_non_full(child, key);
        return node;
      }
    }
  }

  Block* split_child(Block* parent, size_t i, Block* child) {
    // create a new node to hold the split child
    auto new_child = empty_node();
    // let t be the min child count, so t-1 is the min key count
    size_t t = node_min_children();

    size_t child_original_key_count = child->key_count;
    size_t child_original_child_count = child->child_count();

    // move the last t-1 keys of the child to the new child ("right half")
    for (size_t j = 0; j < t - 1; j++) {
      size_t key_ix = child_original_key_count - (t - 1) + j;
      size_t sibling_key_ix = j;
      new_child->keys[sibling_key_ix] = child->keys[key_ix];
      new_child->key_count++;
      // child->keys[key_ix] = TSortKey();
      child->key_count--;
    }

    if (!child->is_leaf()) {
      // move the last t children of the child to the new child
      for (size_t j = 0; j < t; j++) {
        size_t child_ix = child_original_child_count - t + j;
        size_t sibling_child_ix = j;
        new_child->children[sibling_child_ix] = child->children[child_ix];
        child->children[child_ix] = nullptr;
      }
    }

    // shift the keys of the parent to the right
    size_t child_midpoint_key_ix = t - 1;
    for (size_t j = parent->key_count; j > i; j--) {
      parent->keys[j] = parent->keys[j - 1];
    }

    // shift the children of the parent to the right
    for (size_t j = parent->child_count(); j > i + 1; j--) {
      parent->children[j] = parent->children[j - 1];
    }

    // promote middle key of the child to the parent
    parent->keys[i] = child->keys[child_midpoint_key_ix];
    parent->key_count++;
    // child->keys[child_midpoint_key_ix] = TSortKey();
    child->key_count--;

    // bring the new child into the parent
    parent->children[i + 1] = new_child;

    return parent;
  }

  Block* remove_helper(Block* node, TSortKey key) {
    // find the index of the key to delete
    size_t i = 0;
    while (i < node->key_count && key > node->keys[i]) {
      i++;
    }

    // if the key is in the node
    if (i < node->key_count && key == node->keys[i]) {
      // if the node is a leaf
      if (node->is_leaf()) {
        return delete_key_from_leaf(node, i);
      } else {
        return delete_key_from_inner(node, i);
      }
    } else {
      // if the node is a leaf
      if (node->is_leaf()) {
        // key not found
        throw std::runtime_error("key not found in the tree");
      }

      // node is not a leaf, key was not present within this node
      // it must be in a subtree then

      // we also have to ensure that the child node has at least t-1 keys (the minimum required)
      // if that isn't the case, we'll have to move keys around

      bool key_is_in_last_child = i == node->key_count;

      // get the child containing the key
      auto child = node->children[i];
      size_t child_ix = i;

      // if the child has less than t keys, we'll have to fill it
      bool shrink_happened = false;
      if (child->key_count < node_min_keys() + 1) {
        auto new_parent = fill_child(node, i, &shrink_happened);
        if (shrink_happened) {
          node = new_parent;
        }
      }

      if (!shrink_happened) {
        // if the last child was merged with the previous child, we need to search in the previous child
        // otherwise, we can search in the current child (which now has at least t keys, so after deletion it will have
        // at least the minimum required keys)
        if (key_is_in_last_child && i > node->key_count) {
          child_ix = i - 1;
        }

        child = node->children[child_ix];
        child = remove_helper(child, key);
        return node;
      } else {
        // shrink happened, we need to search again in the new parent
        node = remove_helper(node, key);
        return node;
      }
    }

    return node;
  }

  Block* delete_key_from_leaf(Block* node, size_t key_ix) {
    // shift all keys after the key to the left
    for (size_t i = key_ix + 1; i < node->key_count; i++) {
      node->keys[i - 1] = node->keys[i];
    }
    node->keys[node->key_count - 1] = TSortKey();
    node->key_count--;
    return node;
  }

  Block* delete_key_from_inner(Block* node, size_t key_ix) {
    auto delete_k = node->keys[key_ix];
    auto child = node->children[key_ix];
    auto sibling_r = node->children[key_ix + 1];

    // if the child to the left of the key has at least t keys
    if (child->key_count >= node_min_keys() + 1) {
      // this means we can safely borrow from the predecessor
      // and relocate the predecessor to this key's position
      // find the predecessor of the key
      auto pred = get_predecessor(node, key_ix);
      // replace the key with the predecessor
      node->keys[key_ix] = pred;
      // delete the predecessor from the child
      child = remove_helper(child, pred);
    } else if (sibling_r->key_count >= node_min_keys() + 1) {
      // this means we can safely borrow from the successor
      // and relocate the successor to this key's position
      // find the successor of the key
      auto succ = get_successor(node, key_ix);
      // replace the key with the successor
      node->keys[key_ix] = succ;
      // delete the successor from the child
      sibling_r = remove_helper(sibling_r, succ);
    } else {
      // merge k and the right child into the left child
      node = merge_children(node, key_ix);
      // now, the merged child has 2t-1 keys, so we can safely delete k from it
      node = remove_helper(node, delete_k);
    }

    return node;
  }

  TSortKey get_predecessor(Block* node, size_t key_ix) {
    auto child = node->children[key_ix];
    while (!child->is_leaf()) {
      child = child->children[child->key_count];
    }
    return child->keys[child->key_count - 1];
  }

  TSortKey get_successor(Block* node, size_t key_ix) {
    auto child = node->children[key_ix + 1];
    while (!child->is_leaf()) {
      child = child->children[0];
    }
    return child->keys[0];
  }

  Block* fill_child(Block* node_info, size_t key_ix, bool* shrink_happened) {
    auto curr_child = node_info->children[key_ix];
    Block* prev_child = nullptr;
    if (key_ix > 0) {
      prev_child = node_info->children[key_ix - 1];
    }
    Block* next_child = nullptr;
    if (key_ix < node_info->key_count) {
      next_child = node_info->children[key_ix + 1];
    }

    if (prev_child != nullptr && prev_child->key_count > node_min_keys()) {
      node_info = borrow_from_previous_child(node_info, key_ix);
    } else if (next_child != nullptr && next_child->key_count > node_min_keys()) {
      node_info = borrow_from_next_child(node_info, key_ix);
    } else {
      if (key_ix == node_info->key_count) {
        node_info = merge_children(node_info, key_ix - 1);
      } else {
        node_info = merge_children(node_info, key_ix);
      }
    }

    *shrink_happened = true;
    return node_info;
  }

  Block* borrow_from_previous_child(Block* node, size_t key_ix) {
    auto child = node->children[key_ix];
    auto sibling = node->children[key_ix - 1];

    // the last key of the sibling is moved to the parent
    // the key at key_ix-1 in the parent is moved to the child as the first key
    // the sibling loses its last key, and the child gains a key
    // sibling donates a key to the parent, and the parent donates a key to the child
    // sibling moves its last key to the first slot in the parent

    // make room in the child, shift all keys to the right
    for (size_t i = child->key_count; i > 0; i--) {
      child->keys[i] = child->keys[i - 1];
    }

    // if the child is not a leaf, we also need to shift all children to the right
    if (!child->is_leaf()) {
      for (size_t i = child->child_count(); i > 0; i--) {
        child->children[i] = child->children[i - 1];
      }
    }

    // move the key from the parent to the child
    auto key_from_parent = node->keys[key_ix - 1];
    child->keys[0] = key_from_parent;
    child->key_count++;

    // move the key from the sibling to the parent
    auto sibling_last_key_ix = sibling->key_count - 1;
    auto key_from_sibling = sibling->keys[sibling_last_key_ix];
    node->keys[key_ix - 1] = key_from_sibling;
    // sibling->keys[sibling_last_key_ix] = TSortKey();
    sibling->key_count--;

    // if the sibling is not a leaf, we need to also move the corresponding (last) child
    if (!sibling->is_leaf()) {
      auto sibling_last_child_ix = sibling->child_count() - 1;
      auto sibling_last_child = sibling->children[sibling_last_child_ix];
      auto child_first_child_ix = 0;
      child->children[child_first_child_ix] = sibling_last_child;
      sibling->children[sibling_last_child_ix] = nullptr;
    }

    return node;
  }

  Block* borrow_from_next_child(Block* node, size_t key_ix) {
    auto child = node->children[key_ix];
    auto sibling = node->children[key_ix + 1];

    // the first key of the sibling is moved to the parent
    // the key at key_ix in the parent is moved to the child as the last key
    // the sibling donates its first key to the parent, and the parent donates a key to the child

    // set the last key of the child to the key at key_ix in the parent
    auto key_from_parent = node->keys[key_ix];
    child->keys[child->key_count] = key_from_parent;
    child->key_count++;

    // move the key from the sibling to the parent
    auto key_from_sibling = sibling->keys[0];
    node->keys[key_ix] = key_from_sibling;

    // move the corresponding child from the sibling
    // if the child is not a leaf, we also need to move the corresponding (first) child
    if (!child->is_leaf()) {
      auto sibling_first_child = sibling->children[0];
      auto child_last_child_ix = child->child_count();
      child->children[child_last_child_ix] = sibling_first_child;
    }

    // shift all keys in the sibling to the left
    for (size_t i = 0; i < sibling->key_count - 1; i++) {
      sibling->keys[i] = sibling->keys[i + 1];
    }
    // sibling->keys[sibling->key_count - 1] = TSortKey();
    sibling->key_count--;

    // shift all children in the sibling to the left
    if (!child->is_leaf()) {
      for (size_t i = 0; i < sibling->child_count() - 1; i++) {
        sibling->children[i] = sibling->children[i + 1];
      }
      sibling->children[sibling->child_count() - 1] = nullptr;
    }

    return node;
  }

  Block* merge_children(Block* node, size_t key_ix) {
    // merge this child and the next child back into the current child
    auto parent_old_child_count = node->child_count();

    auto child = node->children[key_ix];
    auto sibling = node->children[key_ix + 1];

    // move the key from the parent to the t-1 position in the child
    child->keys[node_min_keys()] = node->keys[key_ix];
    child->key_count++;

    // move all keys from the sibling to the child (from position t onwards)
    // t is node_min_keys+1
    auto sibling_move_key_count = sibling->key_count;
    for (size_t i = 0; i < sibling_move_key_count; i++) {
      child->keys[node_min_keys() + 1 + i] = sibling->keys[i];
      child->key_count++;
      // sibling->keys[i] = TSortKey();
      sibling->key_count--;
    }

    // if the child is not a leaf, move all children from the sibling to the child
    if (!child->is_leaf()) {
      auto sibling_move_child_count = sibling->child_count();
      for (size_t i = 0; i < sibling_move_child_count; i++) {
        child->children[node_min_children() + i] = sibling->children[i];
        sibling->children[i] = nullptr;
      }
    }

    // move all keys in the parent to the left
    for (size_t i = key_ix + 1; i < node->key_count; i++) {
      node->keys[i - 1] = node->keys[i];
    }
    // node->keys[node->key_count - 1] = TSortKey();
    node->key_count--;

    // move all children in the parent to the left
    for (size_t i = key_ix + 2; i < node->child_count(); i++) {
      node->children[i - 1] = node->children[i];
    }
    node->children[node->child_count() - 1] = nullptr;

    // if the parent is the root node, and it has no keys left, then the child becomes the new root
    if (node->key_count == 0) {
      node = child;
    }

    delete sibling;

    return node;
  }

  Block* find_helper(Block* node, TSortKey key) {
    if (node == nullptr) {
      return nullptr;
    }

    size_t i = 0;
    while (i < node->key_count && key > node->keys[i]) {
      i++;
    }

    if (i < node->key_count && key == node->keys[i]) {
      return node;
    }

    if (node->is_leaf()) {
      return nullptr;
    }

    auto child = node->children[i];
    return find_helper(child, key);
  }

public:
  BTree(size_t order) : _order(order), _root(nullptr) {}

  size_t order() const { return _order; }
  Block* root() const { return _root; }

  /** insert a key into the tree */
  void insert(TSortKey key) {
    // if the root is null, create a new node
    if (_root == nullptr) {
      _root = empty_node();
      _root->key_count = 1;
      _root->keys[0] = key;
      return;
    }

    // root exists, check if root has space or is full
    if (!node_is_full(_root)) {
      // insert key into the root
      _root = insert_non_full(_root, key);
      return;
    } else {
      // root is full, split the root
      // new root is allocated
      auto new_root = empty_node();
      // old root is child of new root
      new_root->children[0] = _root;
      // split the old root with respect to its parent (new root)
      new_root = split_child(new_root, 0, _root);
      // insert the key into the new root
      _root = insert_non_full(new_root, key);
    }
  }

  /** remove a key from the tree */
  void remove(TSortKey key) {
    // if the root is null, return
    if (_root == nullptr) {
      return;
    }

    // remove the key from the root
    _root = remove_helper(_root, key);
    // if the root is empty, set the root to null
    if (_root->key_count == 0) {
      _root = nullptr;
    }
  }

  /** whether the tree contains a key */
  bool contains(TSortKey key) { return find(key) != nullptr; }

  /** find the block containing a key */
  Block* find(TSortKey key) { return find_helper(_root, key); }

#if defined(BTREE_DEBUG)
  std::string pretty_print() {
    std::ostringstream oss;
    oss << "--- BTree ---\n";
    if (_root == nullptr) {
      oss << "<empty>";
      return oss.str();
    }
    pretty_print_tree_rec(_root, 0, oss);
    return oss.str();
  }

  void pretty_print_tree_rec(Block* node, size_t depth, std::ostringstream& oss) {
    if (node == nullptr) {
      return;
    }

    std::string spacer = "  ";

    auto spacers = [spacer](size_t n) {
      std::ostringstream oss;
      for (size_t i = 0; i < n; i++) {
        oss << spacer;
      }
      return oss.str();
    };

    oss << spacers(depth);
    oss << " ﹇\n";
    for (size_t i = 0; i < node->key_count; i++) {
      auto child = node->children[i];
      pretty_print_tree_rec(child, depth + 1, oss);
      oss << spacers(depth);
      oss << node->keys[i].to_string();
      oss << "\n";
    }
    auto child = node->children[node->key_count];
    pretty_print_tree_rec(child, depth + 1, oss);
    oss << spacers(depth);
    oss << " ﹈\n";
  }
#endif
};

template <typename TKey, typename TValue> struct KeyValuePairSortKey {
  TKey kvp_key;
  TValue kvp_value;

  bool operator==(const KeyValuePairSortKey& other) const {
    return kvp_key == other.kvp_key && kvp_value == other.kvp_value;
  }

  bool operator!=(const KeyValuePairSortKey& other) const { return !(*this == other); }

  bool operator<(const KeyValuePairSortKey& other) const {
    if (kvp_key < other.kvp_key) {
      return true;
    } else if (kvp_key > other.kvp_key) {
      return false;
    } else {
      return kvp_value < other.kvp_value;
    }
  }

  bool operator>(const KeyValuePairSortKey& other) const { return other < *this; }

  bool operator<=(const KeyValuePairSortKey& other) const { return !(*this > other); }

  bool operator>=(const KeyValuePairSortKey& other) const { return !(*this < other); }

#if defined(BTREE_DEBUG)
  std::string to_string() const {
    std::ostringstream oss;
    oss << "(" << kvp_key << "|" << kvp_value << ")";
    return oss.str();
  }
#endif
};

template <typename TKey, typename TValue> class BTreeMultimap : public BTree<KeyValuePairSortKey<TKey, TValue>> {
  using TSortKey = KeyValuePairSortKey<TKey, TValue>;
  using Block = typename BTree<TSortKey>::Block;

private:
  std::vector<Block*> _smallest_kvp_path;
  std::vector<Block*> _largest_kvp_path;

  struct range_query_bounds {
    Block* smallest_kvp_node;
    Block* largest_kvp_node;
    Block* dca_node;
  };

  range_query_bounds get_range_query_bounds(TKey key) {
    _smallest_kvp_path.clear();
    auto smallest_kvp_node = find_smallest_k_kvp(BTree<TSortKey>::root(), key, _smallest_kvp_path);
    if (smallest_kvp_node == nullptr) {
      throw std::runtime_error("smallest_kvp not found");
    }

    _largest_kvp_path.clear();
    auto largest_kvp_node = find_largest_k_kvp(BTree<TSortKey>::root(), key, _largest_kvp_path);
    if (largest_kvp_node == nullptr) {
      throw std::runtime_error("largest_kvp not found");
    }

    auto dca = find_deepest_common_ancestor(BTree<TSortKey>::root(), _smallest_kvp_path, _largest_kvp_path);

    return range_query_bounds{smallest_kvp_node, largest_kvp_node, dca};
  }

  Block* find_smallest_k_kvp(Block* node, TKey key, std::vector<Block*>& path) {
    if (path.empty()) {
      path.push_back(node);
    }

    if (node->is_leaf()) {
      for (size_t i = 0; i < node->key_count; i++) {
        auto scan_key = node->keys[i];
        if (scan_key.kvp_key == key) {
          return node;
        }
      }
      return nullptr;
    }

    // find the leftmost key: if the smallest key's k-key is still larger, we must go left
    if (key < node->keys[0].kvp_key) {
      path.push_back(node->children[0]);
      auto child = node->children[0];
      return find_smallest_k_kvp(child, key, path);
    }

    // otherwise, we scan through the keys to find the smallest key with the same k-key, and go left if possible
    size_t key_ix = 0;
    while (key_ix < node->key_count && key > node->keys[key_ix].kvp_key) {
      key_ix++;
    }

    auto curr_path_length = path.size();

    // descend into the child node
    auto child = node->children[key_ix];
    path.push_back(child);
    auto child_smallest_k_kvp = find_smallest_k_kvp(child, key, path);
    if (child_smallest_k_kvp != nullptr) {
      return child_smallest_k_kvp;
    } else {
      // nothing better in the subtree, rollback
      path.resize(curr_path_length);
      return node;
    }
  }

  Block* find_largest_k_kvp(Block* node, TKey key, std::vector<Block*>& path) {
    if (path.empty()) {
      path.push_back(node);
    }

    if (node->is_leaf()) {
      for (size_t i = 0; i < node->key_count; i++) {
        auto scan_key = node->keys[i];
        if (scan_key.kvp_key == key) {
          return node;
        }
      }
      return nullptr;
    }

    // find the rightmost key: if the largest key's k-key is still smaller, we must go right
    if (key > node->keys[node->key_count - 1].kvp_key) {
      path.push_back(node->children[node->key_count]);
      auto child = node->children[node->key_count];
      return find_largest_k_kvp(child, key, path);
    }

    // otherwise, we scan through the keys to find the largest key with the same k-key, and go right if possible
    size_t key_ix = 0;
    while (key_ix < node->key_count && key >= node->keys[key_ix].kvp_key) {
      key_ix++;
    }

    auto curr_path_length = path.size();

    // descend into the child node
    auto child = node->children[key_ix];
    path.push_back(child);
    auto child_largest_k_kvp = find_largest_k_kvp(child, key, path);
    if (child_largest_k_kvp != nullptr) {
      return child_largest_k_kvp;
    } else {
      // nothing better in the subtree, rollback
      path.resize(curr_path_length);
      return node;
    }
  }

  Block* find_deepest_common_ancestor(Block* root, std::vector<Block*>& path1, std::vector<Block*>& path2) {
    Block* dca = root;
    for (size_t i = 0; i < std::min(path1.size(), path2.size()); i++) {
      if (path1[i] != path2[i]) {
        break;
      }
      dca = path2[i];
    }
    return dca;
  }

  void collect_same_key_values_inorder(Block* node, TKey key, std::vector<TValue>& values) {
    // inorder traversal of the subtree rooted at the node
    if (node->is_leaf()) {
      for (size_t i = 0; i < node->key_count; i++) {
        if (node->keys[i].kvp_key == key) {
          values.push_back(node->keys[i].kvp_value);
        }
      }
      return;
    }

    // collect left child, key, then right child
    for (size_t i = 0; i < node->key_count; i++) {
      // collect left child
      if (!node->is_leaf()) {
        auto child = node->children[i];
        collect_same_key_values_inorder(child, key, values);
      }
      // collect key
      if (node->keys[i].kvp_key == key) {
        values.push_back(node->keys[i].kvp_value);
      }
    }

    // collect last child
    if (!node->is_leaf()) {
      auto child = node->children[node->key_count];
      collect_same_key_values_inorder(child, key, values);
    }
  }

  void collect_same_key_values_bfs(
      Block* root, Block* smallest_kvp_node, Block* largest_kvp_node, TKey key, std::vector<TValue>& values
  ) {
    // BFS traversal of the subtree rooted at the node

    // queue of nodes to visit
    std::deque<Block*> queue;
    queue.push_back(root);
    while (!queue.empty()) {
      // pop the current node
      auto current = queue.front();
      queue.pop_front();

      // find lower and higher key indices in the current node
      // lower key is the key after a lower kgroup
      // higher key is the key before a higher kgroup
      size_t lower_key_ix = 0;
      size_t higher_key_ix = current->key_count;
      while (lower_key_ix < current->key_count && current->keys[lower_key_ix].kvp_key < key) {
        lower_key_ix++;
      }
      while (higher_key_ix > 0 && current->keys[higher_key_ix - 1].kvp_key > key) {
        higher_key_ix--;
      }

      auto current_towards_lower = lower_key_ix == current->key_count;
      auto current_towards_upper = higher_key_ix == 0;

      // ensure it's not outside the intersection of the two extent nodes
      if (current_towards_lower && current_towards_upper) {
        throw std::runtime_error("error: outside the intersection of the two");
      }

      // queue children that are between the lower and higher keys
      if (!current->is_leaf()) {
        auto lower_child_ix = lower_key_ix;
        auto higher_child_ix = higher_key_ix;
        for (size_t child_ix = lower_child_ix; child_ix <= higher_child_ix; child_ix++) {
          auto child = current->children[child_ix];
          queue.push_back(child);
        }
      }

      // add to values if it's in the range
      for (size_t key_ix = lower_key_ix; key_ix < higher_key_ix; key_ix++) {
        if (current->keys[key_ix].kvp_key == key) {
          values.push_back(current->keys[key_ix].kvp_value);
        }
      }
    }
  }

public:
  BTreeMultimap(size_t order) : BTree<TSortKey>(order) {}

  /** insert a key-value pair into the tree */
  void insert(TKey kvp_key, TValue kvp_value) {
    auto key = TSortKey{kvp_key, kvp_value};
    BTree<TSortKey>::insert(key);
  }

  /** remove a key-value pair from the tree */
  void remove(TKey kvp_key, TValue kvp_value) {
    auto key = TSortKey{kvp_key, kvp_value};
    BTree<TSortKey>::remove(key);
  }

  /** get the first value for a key */
  TValue get_first(TKey key) {
    _smallest_kvp_path.clear();
    auto smallest_kvp_node = find_smallest_k_kvp(BTree<TSortKey>::root(), key, _smallest_kvp_path);
    if (smallest_kvp_node == nullptr) {
      throw std::runtime_error("smallest_kvp not found");
    }

    // find the value in the smallest kvp node
    for (size_t i = 0; i < smallest_kvp_node->key_count; i++) {
      if (smallest_kvp_node->keys[i].kvp_key == key) {
        return smallest_kvp_node->keys[i].kvp_value;
      }
    }

    throw std::runtime_error("key not found in node");
  }

  /** get all values for a key, ordered */
  std::vector<TValue> get_all_ordered(TKey key) {
    auto range_query_bounds = get_range_query_bounds(key);

    std::vector<TValue> values;

    collect_same_key_values_inorder(range_query_bounds.dca_node, key, values);

    return values;
  }

  /** get all values for a key, unordered */
  std::vector<TValue> get_all_unordered(TKey key) {
    auto range_query_bounds = get_range_query_bounds(key);

    std::vector<TValue> values;

    collect_same_key_values_bfs(
        range_query_bounds.dca_node, range_query_bounds.smallest_kvp_node, range_query_bounds.largest_kvp_node, key,
        values
    );

    return values;
  }
};
