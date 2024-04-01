#ifndef SJTU_TRIE_HPP
#define SJTU_TRIE_HPP

#include <vector>
#include <exception>

#include <algorithm>
#include <cstddef>
#include <map>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include <string_view>
#include <iostream>
#include <shared_mutex>
#include <string_view>
#include <mutex>  
#include <future> 

namespace sjtu {


// A TrieNode is a node in a Trie.
class TrieNode {
 public:
  using ChildrenMap = std::map<char, std::shared_ptr<const TrieNode>>;
  using ShardNodePtr = std::shared_ptr<const TrieNode>;
//  protected:
  // A map of children, where the key is the next character in the key, and the value is the next TrieNode.
  ChildrenMap children_;

  // Indicates if the node is the terminal node.
  bool is_value_node_{false};

 public:
  // Create a TrieNode with no children.
  TrieNode() = default;

  // Create a TrieNode with some children.
  explicit TrieNode(ChildrenMap children) : children_(std::move(children)) {}

  virtual ~TrieNode() = default;

  // Clone returns a copy of this TrieNode. If the TrieNode has a value, the value is copied. The return
  // type of this function is a unique_ptr to a TrieNode.
  //
  // You cannot use the copy constructor to clone the node because it doesn't know whether a `TrieNode`
  // contains a value or not.
  //
  // Note: if you want to convert `unique_ptr` into `shared_ptr`, you can use `std::shared_ptr<T>(std::move(ptr))`.
  virtual auto Clone() const -> std::unique_ptr<TrieNode> { return std::make_unique<TrieNode>(children_); }
  // VIRTUAL is important here.

  // You can add additional fields and methods here. But in general, you don't need to add extra fields to
  // complete this project.

  // test whether child c exists
  bool child_exist(char c) const {
    return children_.find(c) != children_.end();
  }

};

// A TrieNodeWithValue is a TrieNode that also has a value of type T associated with it.
template <class T>
class TrieNodeWithValue : public TrieNode {
 public:
  using SharedValuePtr = std::shared_ptr<T>;
//  protected:
  // The value associated with this trie node.
  SharedValuePtr value_;

 public:
  // Create a trie node with no children and a value.
  explicit TrieNodeWithValue(SharedValuePtr value) : value_(std::move(value)) { this->is_value_node_ = true; }

  // Create a trie node with children and a value.
  TrieNodeWithValue(ChildrenMap children, SharedValuePtr value)
      : TrieNode(std::move(children)), value_(std::move(value)) {
    this->is_value_node_ = true;
  }

  // Override the Clone method to also clone the value.
  //
  // Note: if you want to convert `unique_ptr` into `shared_ptr`, you can use `std::shared_ptr<T>(std::move(ptr))`.
  auto Clone() const -> std::unique_ptr<TrieNode> override {
    return std::make_unique<TrieNodeWithValue<T>>(children_, value_);
  }
};

// A Trie is a data structure that maps strings to values of type T. All operations on a Trie should not
// modify the trie itself. It should reuse the existing nodes as much as possible, and create new nodes to
// represent the new trie.
class Trie {
  using SharedNodePtr = typename TrieNode::ShardNodePtr;
 private:
  // The root of the trie.
  SharedNodePtr root_{nullptr};

  // Create a new trie with the given root.
  explicit Trie(SharedNodePtr root) : root_(std::move(root)) {}

 private:
  SharedNodePtr get_basic(std::string_view key) const {
    SharedNodePtr p = root_;
    if (!root_)
      return nullptr;
    for (char c : key) {
      if (p && p->child_exist(c))
        p = p->children_.at(c);
      else 
        return nullptr;
    }
    return p;
  }

 public:
  // Create an empty trie.
  Trie() = default;

  // Get the value associated with the given key.
  // 1. If the key is not in the trie, return nullptr.
  // 2. If the key is in the trie but the type is mismatched, return nullptr.
  // 3. Otherwise, return the value.
  template <class T>
  auto Get(std::string_view key) const -> const T * {
    SharedNodePtr p = get_basic(key);
    if (const TrieNodeWithValue<T> *pv 
      = dynamic_cast<const TrieNodeWithValue<T>*>(p.get())) {
      return pv->value_.get();
    } else
      return nullptr;
  }

  // Put a new key-value pair into the trie. If the key already exists, overwrite the value.
  // Returns the new trie.
  template <class T>
  auto Put(std::string_view key, T value) const -> Trie {
    if (!key.size()) {
      return Trie(std::make_shared<TrieNodeWithValue<T>>(TrieNodeWithValue<T>(root_->children_, std::make_shared<T>(std::move(value)))));
    }
    std::unique_ptr<TrieNode> new_root = root_ ? root_->Clone() : std::make_unique<TrieNode>(TrieNode());
    TrieNode *p = new_root.get(), *last = nullptr;
    SharedNodePtr q = root_;
    for (char c : key) {
      last = p;
      if (q && q->child_exist(c)) {
        p->children_[c] = q->children_.at(c)->Clone();
        q = q->children_.at(c);
      } else 
        p->children_[c] = std::make_shared<const TrieNode>(TrieNode());
      p = const_cast<TrieNode*>(p->children_[c].get());
    }
    last->children_[key.back()] = std::make_shared<TrieNodeWithValue<T>>(TrieNodeWithValue<T>(p->children_, std::make_shared<T>(std::move(value))));
    return Trie(std::shared_ptr<const TrieNode>(std::move(new_root)));
  }
  // Remove the key from the trie. If the key does not exist, return the original trie.
  // Otherwise, returns the new trie.
  auto Remove(std::string_view key) const -> Trie {
    SharedNodePtr dst = get_basic(key);
    if (!dst || !dst->is_value_node_)
      return *this;
    
    if (!key.size()) {
      return Trie(std::make_shared<TrieNode>(root_->children_));
    }
    
    std::unique_ptr<TrieNode> new_root = root_->Clone(); // root 一定存在
    TrieNode* p = new_root.get();
    SharedNodePtr  q = root_;
    std::vector<TrieNode*> vec;
    for (char c : key) {
      vec.push_back(p);
      p->children_[c] = q->children_.at(c)->Clone();
      p = const_cast<TrieNode*>(p->children_[c].get());
      q = q->children_.at(c);
    }
    TrieNode* last = vec.back();
    last->children_[key.back()] = std::make_shared<TrieNode>(p->children_);
    int back = key.size() - 1;
    while ((last = (vec.size() ? vec.back() : nullptr)) 
      && last->children_[key[back]]->children_.empty() && !(last->children_[key[back]]->is_value_node_)) {
      last->children_.erase(key[back]);
      vec.pop_back();
      back--;
    }
    if (!vec.size() && !new_root->is_value_node_)
      new_root = nullptr;
    return Trie(std::shared_ptr<TrieNode>(std::move(new_root)));
  }

  const TrieNode* native_handle() {
    return root_.get();
  }
};


// This class is used to guard the value returned by the trie. It holds a reference to the root so
// that the reference to the value will not be invalidated.
template <class T>
class ValueGuard {
 public:
  ValueGuard(Trie root, const T &value) : root_(std::move(root)), value_(value) {}
  auto operator*() const -> const T & { return value_; }

 private:
  Trie root_;
  const T &value_;
};

// This class is a thread-safe wrapper around the Trie class. It provides a simple interface for
// accessing the trie. It should allow concurrent reads and a single write operation at the same
// time.
class TrieStore {
 public:
  // This function returns a ValueGuard object that holds a reference to the value in the trie of the given version (default: newest version). If
  // the key does not exist in the trie, it will return std::nullopt.
  template <class T>
  auto Get(std::string_view key, size_t version = size_t(-1)) const -> std::optional<ValueGuard<T>> {
  // auto Get(std::string_view key, size_t version = size_t(-1)) -> std::optional<ValueGuard<T>> {
    if (version == size_t(-1))
      version = get_version();
    if (version > get_version())
      throw std::runtime_error("invalid version");
    Trie trie = snapshots_[version];
    T* result = const_cast<T*>(trie.Get<T>(key));
    if (result)
      return ValueGuard<T>(trie, *result);
    else 
      return std::nullopt;
  }

  // This function will insert the key-value pair into the trie. If the key already exists in the
  // trie, it will overwrite the value
  // return the version number after operation
  // Hint: new version should only be visible after the operation is committed(completed)
  template <class T>
  size_t Put(std::string_view key, T value) {
    std::lock_guard write_lock(write_lock_);

    Trie trie = snapshots_.back();

    trie = trie.Put(key, std::move(value));
    snapshots_.push_back(trie);
    version_.fetch_add(1, std::memory_order_acquire); // commit point

    return get_version();
  }

  // This function will remove the key-value pair from the trie.
  // return the version number after operation
  // if the key does not exist, version number should not be increased
  size_t Remove(std::string_view key) {
    std::lock_guard write_lock(write_lock_);

    Trie trie = snapshots_.back();
    Trie new_trie = trie.Remove(key);
    if (new_trie.native_handle() == trie.native_handle())
      return get_version();
    snapshots_.push_back(new_trie);
    version_.fetch_add(1, std::memory_order_acquire); // commit point

    return get_version();
  }

  // This function return the newest version number
  size_t get_version() const {
    return version_;
  }

  // ~TrieStore() {
  //   // std::cerr << "DESTRUCT" << std::endl;
  // }

 private:

  // This mutex sequences all writes operations and allows only one write operation at a time.
  // Concurrent modifications should have the effect of applying them in some sequential order
  std::mutex write_lock_;

  // Stores all historical versions of trie
  // version number ranges from [0, snapshots_.size())
  std::vector<Trie> snapshots_{1};
  std::atomic<size_t> version_{0};
};

}  // namespace sjtu

#endif  // SJTU_TRIE_HPP