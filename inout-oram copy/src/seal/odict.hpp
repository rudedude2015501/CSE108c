#ifndef ODICT_HPP
#define ODICT_HPP

#include <string>
#include <vector>
#include <memory>
#include <map>
#include <random>
#include <cstring>
#include <utility>

namespace odict {

// Simple node structure for AVL tree implementation
template<typename K, typename V>
struct TreeNode {
    K key;
    V value;
    int height;
    std::shared_ptr<TreeNode> left;
    std::shared_ptr<TreeNode> right;
    
    TreeNode(K k, V v) : key(k), value(v), height(1), left(nullptr), right(nullptr) {}
};

// Oblivious Dictionary implementation using oblivious AVL tree
template<typename K, typename V>
class ObliviousDictionary {
public:
    struct State {
        std::vector<uint8_t> key;
        // In a real implementation, we would have proper encryption state
    };
    
    struct Tree {
        std::shared_ptr<TreeNode<K, V>> root;
        // In a real implementation, this would be encrypted
    };
    
    // Setup the oblivious dictionary
    static std::pair<std::shared_ptr<State>, std::shared_ptr<Tree>> Setup(
        size_t security_param, 
        size_t N) {

        auto state = std::make_shared<State>();
        auto tree = std::make_shared<Tree>();
        
        // Generate key using the renamed function from ADJORAM if you're using the same class
        // Or implement key generation directly:
        state->key.resize(32);
        std::random_device rd;
        std::mt19937 gen(rd());
        std::uniform_int_distribution<uint8_t> dist(0, 255);
        
        for (size_t i = 0; i < state->key.size(); i++) {
            state->key[i] = dist(gen);
        }
        
        return {state, tree};
    }
    
    // Insert a key-value pair into the dictionary
    static std::pair<std::shared_ptr<State>, std::shared_ptr<Tree>> Insert(
        const std::shared_ptr<State>& state,
        const std::shared_ptr<Tree>& tree,
        const K& key,
        const V& value) {
        
        auto new_state = state;
        auto new_tree = std::make_shared<Tree>();
        
        // Insert into the AVL tree (not oblivious in this example)
        new_tree->root = insert(tree->root, key, value);
        
        return {new_state, new_tree};
    }
    
    // Search for a key in the dictionary
    static std::pair<V, std::shared_ptr<State>> Search(
        const std::shared_ptr<State>& state,
        const std::shared_ptr<Tree>& tree,
        const K& key) {
        
        auto result = search(tree->root, key);
        
        return {result, state};
    }
    
private:
    // Helper function to get the height of a node
    static int height(const std::shared_ptr<TreeNode<K, V>>& node) {
        if (!node) return 0;
        return node->height;
    }
    
    // Helper function to get the balance factor of a node
    static int getBalance(const std::shared_ptr<TreeNode<K, V>>& node) {
        if (!node) return 0;
        return height(node->left) - height(node->right);
    }
    
    // Right rotation
    static std::shared_ptr<TreeNode<K, V>> rightRotate(std::shared_ptr<TreeNode<K, V>> y) {
        auto x = y->left;
        auto T2 = x->right;
        
        x->right = y;
        y->left = T2;
        
        y->height = std::max(height(y->left), height(y->right)) + 1;
        x->height = std::max(height(x->left), height(x->right)) + 1;
        
        return x;
    }
    
    // Left rotation
    static std::shared_ptr<TreeNode<K, V>> leftRotate(std::shared_ptr<TreeNode<K, V>> x) {
        auto y = x->right;
        auto T2 = y->left;
        
        y->left = x;
        x->right = T2;
        
        x->height = std::max(height(x->left), height(x->right)) + 1;
        y->height = std::max(height(y->left), height(y->right)) + 1;
        
        return y;
    }
    
    // Insert a node into the AVL tree
    static std::shared_ptr<TreeNode<K, V>> insert(
        std::shared_ptr<TreeNode<K, V>> node,
        const K& key,
        const V& value) {
        
        // Standard BST insert
        if (!node) return std::make_shared<TreeNode<K, V>>(key, value);
        
        if (key < node->key)
            node->left = insert(node->left, key, value);
        else if (key > node->key)
            node->right = insert(node->right, key, value);
        else {
            // Update value if key already exists
            node->value = value;
            return node;
        }
        
        // Update height of this ancestor node
        node->height = 1 + std::max(height(node->left), height(node->right));
        
        // Get the balance factor
        int balance = getBalance(node);
        
        // Left Left Case
        if (balance > 1 && key < node->left->key)
            return rightRotate(node);
        
        // Right Right Case
        if (balance < -1 && key > node->right->key)
            return leftRotate(node);
        
        // Left Right Case
        if (balance > 1 && key > node->left->key) {
            node->left = leftRotate(node->left);
            return rightRotate(node);
        }
        
        // Right Left Case
        if (balance < -1 && key < node->right->key) {
            node->right = rightRotate(node->right);
            return leftRotate(node);
        }
        
        return node;
    }
    
    // Search for a key in the AVL tree
    static V search(const std::shared_ptr<TreeNode<K, V>>& node, const K& key) {
        if (!node) return V();
        
        if (key == node->key)
            return node->value;
        
        if (key < node->key)
            return search(node->left, key);
        else
            return search(node->right, key);
    }
};

} // namespace odict

#endif // ODICT_HPP