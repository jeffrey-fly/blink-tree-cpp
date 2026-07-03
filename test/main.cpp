#include "blink_tree.hpp"
#include <iostream>

int main()
{
    BLinkTree_Init(2);  // we set max_keys_per_node to 2 to trigger splits quickly

    // Insert keys 1-7 and 9 (skip 8 to test search for a missing key)
    for (Key k : {1, 2, 3, 4, 5, 6, 7, 9})
    {
        BLinkTree_Insert(k, k * 100);
    }

    // Then search each key to verify
    for (Key k : {1, 2, 3, 4, 5, 6, 7, 8})
    {
        auto r = BLinkTree_Search(k);
        std::cout << k << ": " << (r.entries.empty() ? "MISSING" : std::to_string(r.entries[r.entries.size() - 1].second)) << std::endl;
    }

    return 0;
}