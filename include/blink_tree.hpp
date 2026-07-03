#pragma once

#include <cstdint>
#include <vector>
#include <optional>
#include <shared_mutex>
#include <atomic>
#include <memory>

using Key = int64_t;          
using Value = int64_t;      
using NodeId = uint64_t;      // node identifier type, used to uniquely identify nodes in the BLink tree

constexpr NodeId NULL_NODE = 0;

/*
 * Structure to hold the result of a search operation in the BLink tree.
 */
struct SearchResult {
    std::vector<std::pair<Key, Value>> entries; // store the key-value pairs found in the search range
};

/*
 * Structure representing a node in the BLink tree.
 * represents both internal and leaf nodes, with fields for keys, children, values, and synchronization mechanisms.
 * remarks:
 *  - children[i] corresponds to keys[i] for i in [0, keys.size() - 1]
 *  - children has one more element than keys, i.e., children.size() == keys.size() + 1
 */
struct BLinkNode {
    bool is_leaf;                   // Indicates whether the node is a leaf node or an internal node
    NodeId self_id;                 // The unique identifier for this node

    std::vector<Key> keys;          // The keys stored in this node
    std::vector<NodeId> children;   // The child node IDs corresponding to the keys
    std::vector<Value> values;      // The values associated with the keys (only used in leaf nodes) 
    NodeId right_link = NULL_NODE;  // The ID of the right sibling node (used for linking leaf nodes)
    std::optional<Key> high_key;    // The highest key in this node (used for determining the range of keys covered by this node)

    std::shared_mutex latch;        // A shared mutex for synchronizing access to this node        
    std::atomic<uint64_t> version{0}; // A version number for optimistic concurrency control
};

using BLinkNodePtr = std::unique_ptr<BLinkNode>;

/* 
* Initialize the BLink tree with the maximum number of keys per node.
* @param max_keys_per_node: The maximum number of keys allowed in each node.
* @return: true if initialization is successful, false otherwise.
*/
bool BLinkTree_Init(int max_keys_per_node);

/*
* Search for entries in the BLink tree within the specified key range.
* @param key: The key to search for in the BLink tree.
* @return: A SearchResult containing the entries found within the range.  
*/
SearchResult BLinkTree_Search(Key key);  

/*
* Insert a key-value pair into the BLink tree.
* @param key: The key to be inserted.
* @param value: The value to be inserted.
* @return: true if the insertion is successful, false if the key already exists.
*/
bool BLinkTree_Insert(Key key,Value value);

void BlinkTree_Reset();