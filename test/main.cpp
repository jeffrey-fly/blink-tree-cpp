#include "blink_tree.hpp"
#include <iostream>
#include <assert.h>

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
        bool found = !r.entries.empty();
        Value expected = k * 100;  // 假设你的测试数据规律是 key*100

        if (k == 8)
        {
            // 故意没插入的 key，期望是 MISSING
            assert(!found && "key 8 should not exist");
        }
        else
        {
            assert(found && "expected key to exist");
            assert(r.entries.back().second == expected && "wrong value returned");
        }
    }
    std::cout << "All assertions passed." << std::endl;

    return 0;
}