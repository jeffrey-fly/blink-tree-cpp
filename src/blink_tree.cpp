#include "blink_tree.hpp"
#include <algorithm>
#include <map>
#include <stack>
#include <iostream>

static size_t g_max_keys_per_node = 1;
static std::atomic<uint64_t> g_next_node_id{0};
static std::map<NodeId, BLinkNodePtr> g_node_store; 
static NodeId g_root_id = NULL_NODE;

static NodeId GetNextNodeId()
{
    return g_next_node_id++;
}

/*
* search key in the BLink tree, return node id. maybe: node itself; right link; or child node id. 
*/
static NodeId ScanNode(Key key, BLinkNode* node)
{
    if(node->right_link != NULL_NODE && node->high_key.has_value() && key >= node->high_key.value())
    {
        return node->right_link; // Traverse to the right sibling if the key is greater than the high key
    }

    if (node->is_leaf)
    {
        return node->self_id; 
    }

    auto it = std::lower_bound(node->keys.begin(), node->keys.end(), key);
    size_t index = std::distance(node->keys.begin(), it);
    if (it != node->keys.end() && *it == key)
        return node->children[index + 1];
    else
        return node->children[index];
}

static size_t FindInNode(Key key, BLinkNode* node)
{
    auto it = std::lower_bound(node->keys.begin(), node->keys.end(), key);
    if (it != node->keys.end() && *it == key)
    {
        return std::distance(node->keys.begin(), it);
    }
    return static_cast<size_t>(-1); // Key not found
}

static BLinkNode* GetNodeById(NodeId node_id)
{
    auto it = g_node_store.find(node_id);
    if (it != g_node_store.end())
    {
        return it->second.get();
    }
    return nullptr;
}

static bool IsNodeSafeToInsert(BLinkNode* node)
{
    return node->keys.size() < g_max_keys_per_node;
}

static bool InsertIntoLeaf(BLinkNode* node, Key key, std::optional<Value> value)
{
    if (!node->is_leaf)
    {
        return false;
    }

    auto it = std::lower_bound(node->keys.begin(), node->keys.end(), key);
    size_t index = std::distance(node->keys.begin(), it);

    if (it != node->keys.end() && *it == key)
    {
        return false; // Key already exists
    }

    node->keys.insert(it, key);
    if (node->is_leaf && value.has_value())
    {
        node->values.insert(node->values.begin() + index, value.value());
    }
    return true;
}

static bool InsertIntoInternal(BLinkNode* node, Key key, NodeId child_id)
{
    auto it = std::lower_bound(node->keys.begin(), node->keys.end(), key);
    size_t index = std::distance(node->keys.begin(), it);

    if (it != node->keys.end() && *it == key)
    {
        return false; // Key already exists
    }

    node->keys.insert(it, key);
    node->children.insert(node->children.begin() + index + 1, child_id);
    return true;
}

static void PrintBLinkTree(BLinkNode* node, int level = 0)
{
    if (!node) return;

    std::cout << std::string(level * 4, ' ') << "Node ID: " << node->self_id << ", Keys: ";
    for (const auto& k : node->keys)
    {
        std::cout << k << " ";
    }
    std::cout << ", Is Leaf: " << node->is_leaf;
    if (node->high_key.has_value())
    {
        std::cout << ", High Key: " << node->high_key.value();
    }
    std::cout << ", Right Link: " << node->right_link;

    if (!node->is_leaf)
    {
        std::cout << ", children's keys: " ;
        for (const auto& child_id : node->children)
        {
            std::cout << child_id << " ";
        }

        for (const auto& child_id : node->children)
        {
            BLinkNode* child_node = GetNodeById(child_id);
            std::cout << std::endl;
            PrintBLinkTree(child_node, level + 1);
        }
    }
}

void BlinkTree_Reset()
{
    g_next_node_id = 1;
    g_node_store.clear();
    g_root_id = NULL_NODE;
}

bool BLinkTree_Init(int max_keys_per_node)
{
    g_max_keys_per_node = (max_keys_per_node <= 0) ? 4 : max_keys_per_node;
    g_next_node_id = 1;
    g_node_store.clear();
    g_root_id = NULL_NODE;
    return true;
}

SearchResult BLinkTree_Search(Key _key)  
{
    SearchResult result;
    if (g_root_id == NULL_NODE)
    {
        return result; 
    }
    BLinkNode* current_node = GetNodeById(g_root_id);

    while (!current_node->is_leaf) // Traverse right ordown to the leaf node
    {
        NodeId _id = ScanNode(_key, current_node);
        current_node = GetNodeById(_id);
    }

    while(ScanNode(_key, current_node) == current_node->right_link)
    {
        current_node = GetNodeById(current_node->right_link);
    }

    auto index = FindInNode(_key, current_node);

    if (index != static_cast<size_t>(-1))
    {
        result.entries.emplace_back(_key, current_node->values[index]); 
    }

    return result;
}

BLinkNode* MoveRightIfNecessary(BLinkNode* current_node, Key key)
{
    while (ScanNode(key, current_node) == current_node->right_link)
    {
        current_node = GetNodeById(current_node->right_link);
    }
    return current_node;
}

bool BLinkTree_Insert(Key key, Value value)
{
// #ifdef DEBUG
//     PrintBLinkTree(GetNodeById(g_root_id));
//     std::cout << std::endl;
//     std::cout << " ************ " << std::endl;   
// #endif
    std::stack<BLinkNode*> node_stack;
    BLinkNode* current_node = GetNodeById(g_root_id);
    if (!current_node)
    {
        BLinkNodePtr new_root = std::make_unique<BLinkNode>();
        NodeId new_root_id = GetNextNodeId();
        new_root->self_id = new_root_id;
        new_root->is_leaf = true;
        new_root->keys.push_back(key);
        new_root->values.push_back(value);
        g_node_store[new_root->self_id] = std::move(new_root);
        g_root_id = new_root_id;  
        return true;
    }
    while (!current_node->is_leaf) // Traverse down to the leaf node
    {
        NodeId _id = ScanNode(key, current_node);
        if (_id != current_node->right_link) 
        {
            node_stack.push(current_node);
        }
        current_node = GetNodeById(_id);
    }
    
    while(ScanNode(key, current_node) == current_node->right_link){ // Traverse right if the value is greater than the high key of the current node
        current_node = GetNodeById(current_node->right_link);
    }

    auto index = FindInNode(key, current_node);

    if (index != static_cast<size_t>(-1))
    {
        return false; // Key already exists
    }

    NodeId child_id = NULL_NODE;
Doinsertion:
    bool is_safe = IsNodeSafeToInsert(current_node);
    bool is_leaf = current_node->is_leaf;

    if (is_safe)
    {
        bool ret = false;
        if (is_leaf)
        {
            ret = InsertIntoLeaf(current_node, key, std::make_optional(value));
        }
        else
        {
            ret = InsertIntoInternal(current_node, key, child_id);
        }
        return ret;
    }
    else
    {
        BLinkNodePtr new_node = std::make_unique<BLinkNode>();
        new_node->self_id = GetNextNodeId();
        new_node->is_leaf = is_leaf;

        if (is_leaf)
        {
            InsertIntoLeaf(current_node, key, std::make_optional(value));
            size_t mid_index = current_node->keys.size() / 2;
            new_node->keys.assign(current_node->keys.begin() + mid_index, current_node->keys.end());
            new_node->values.assign(current_node->values.begin() + mid_index, current_node->values.end());
            key = current_node->keys[mid_index]; // Promote the middle key of the current node

            child_id = new_node->self_id;

            current_node->keys.resize(mid_index);
            current_node->values.resize(mid_index);
        }
        else
        {
            InsertIntoInternal(current_node, key, child_id);
            size_t mid_index = current_node->keys.size() / 2;
            key = current_node->keys[mid_index]; // Promote the middle key of the current node
            child_id = new_node->self_id; // Promote the new node's ID as the child ID for the parent

            new_node->keys.assign(current_node->keys.begin() + mid_index + 1, current_node->keys.end());
            new_node->children.assign(current_node->children.begin() + mid_index + 1, current_node->children.end());
            
            current_node->keys.resize(mid_index);
            current_node->children.resize(mid_index + 1); 

            new_node->high_key = current_node->high_key; 
            current_node->high_key = key;
        }
        
        new_node->right_link = current_node->right_link;
        current_node->right_link = new_node->self_id;
        g_node_store[new_node->self_id] = std::move(new_node);

        BLinkNode* old_node = current_node;
        if (node_stack.empty()) 
        {
            BLinkNodePtr new_root = std::make_unique<BLinkNode>();
            NodeId new_root_id = GetNextNodeId();
            new_root->self_id = new_root_id;
            new_root->is_leaf = false;
            new_root->keys.push_back(key);
            new_root->children.push_back(old_node->self_id);
            new_root->children.push_back(g_node_store[old_node->right_link]->self_id);
            new_root->high_key = std::nullopt; 
            g_node_store[new_root->self_id] = std::move(new_root);
            g_root_id = new_root_id;  
            return true;
        } else
        {
            current_node = node_stack.top();
            node_stack.pop();
        }
        current_node = MoveRightIfNecessary(current_node, key);
        goto Doinsertion;
    }

    return true;
}
