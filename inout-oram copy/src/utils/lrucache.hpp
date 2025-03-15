#include <cassert>
#include <cstddef>
#include <list>
#include <memory>
#include <optional>
#include <type_traits>
#include <unordered_map>
#include <vector>

template <typename Key, typename Value> class LRUCache {
  static_assert(std::is_default_constructible_v<Key>, "Key must be default constructible");
  static_assert(std::is_default_constructible_v<Value>, "Value must be default constructible");
  static_assert(std::is_standard_layout_v<Key> && std::is_trivial_v<Key>, "Key must be a POD type");

private:
  struct Node {
    Key key;
    Value value;
    Node* prev;
    Node* next;
  };

  std::size_t _capacity;
  std::size_t _size;
  std::vector<Node> _nodes;
  std::unordered_map<Key, Node*> _map;
  Node* _head;
  Node* _tail;
  Node* _free_list_head;

  void move_to_front(Node* node) {
    if (node == _head) {
      return;
    }

    // remove from its current position
    if (node->prev) {
      node->prev->next = node->next;
    }
    if (node->next) {
      node->next->prev = node->prev;
    }
    if (node == _tail) {
      _tail = node->prev;
    }

    // insert at front
    node->prev = nullptr;
    node->next = _head;
    if (_head) {
      _head->prev = node;
    }
    _head = node;

    // update tail if needed
    if (!_tail) {
      _tail = node;
    }
  }

  // allocate node data
  Node* allocate_node() {
    if (_free_list_head) {
      // free list stores the next free node
      Node* node = _free_list_head;
      _free_list_head = _free_list_head->next;
      return node;
    } else {
      // free list is not initialized
      // use the previously unused part of the nodes buffer
      return &_nodes[_size];
    }
  }

  // release node data
  void release_node(Node* node) {
    // move this node to the front of the free list
    node->next = _free_list_head;
    _free_list_head = node;
  }

public:
  explicit LRUCache(std::size_t max_size) :
      _capacity(max_size), _size(0), _nodes(max_size), _head(nullptr), _tail(nullptr), _free_list_head(nullptr) {
    _map.reserve(max_size);
  }

  void put(const Key& key, const Value& value) {
    auto it = _map.find(key);
    if (it != _map.end()) {
      // key exists, update value and move to front
      it->second->value = value;
      move_to_front(it->second);
    } else {
      ENSURE(_size < _capacity, "LRUCache::put: cache is full");

      Node* node = allocate_node();
      node->key = key;
      node->value = value;
      node->prev = nullptr;
      node->next = nullptr;

      _size++;

      _map[key] = node;
      // printf("LRUCache::put: key=%d, size=%d, map_count=%d\n", key, _size, _map.size());

      if (!_head) {
        // cache has nothing in it
        // this becomes the first and last node
        _head = _tail = node;
      } else {
        // insert at front
        node->next = _head;
        _head->prev = node;
        _head = node;
      }
    }
  }

  bool get(const Key& key, Value** value) {
    auto it = _map.find(key);
    // printf("LRUCache::get: key=%d, size=%d, map_count=%d\n", key, _size, _map.size());
    if (it == _map.end()) {
      return false; // key not found
    }
    // update value and move to front
    *value = &it->second->value;
    move_to_front(it->second);
    return true;
  }

  bool evict_lru(Key* evicted_key, Value* evicted_value) {
    if (_size == 0) {
      return false; // nothing to evict
    }

    // remove the tail node (least recently used)
    Node* lru_node = _tail;

    // erase from map
    _map.erase(lru_node->key);

    // save the key and value to return
    *evicted_key = lru_node->key;
    *evicted_value = lru_node->value;

    // update tail
    _tail = _tail->prev;
    if (_tail) {
      _tail->next = nullptr;
    } else {
      // cache is now empty
      _head = nullptr;
    }

    // decrease size
    --_size;

    // reset and release the evicted node
    lru_node->prev = nullptr;
    lru_node->next = nullptr;
    release_node(lru_node);

    // printf("LRUCache::evict_lru: key=%d, size=%d, map_count=%d\n", *evicted_key, _size, _map.size());

    return true;
  }

  bool contains(const Key& key) const { return _map.find(key) != _map.end(); }
  std::size_t size() const { return _size; }
  std::size_t capacity() const { return _capacity; }
  bool empty() const { return _size == 0; }
  bool is_full() const { return _size == _capacity; }
};
