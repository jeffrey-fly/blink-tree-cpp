#include "../include/blink_tree.hpp"
#include <iostream>

int main()
{
    BLinkTree_Init(2);  // 故意设小一点，比如 max_keys=2，容易触发split

    // 依次插入触发：1) 叶子split  2) 父节点也split（传播到根）  3) 再插入验证根split之后树还能正常search
    for (Key k : {1, 2, 3, 4, 5, 6, 7, 9})
    {
        BLinkTree_Insert(k, k * 100);
    }

    // 然后逐个search验证
    for (Key k : {1, 2, 3, 4, 5, 6, 7, 8})
    {
        auto r = BLinkTree_Search(k);
        std::cout << k << ": " << (r.entries.empty() ? "MISSING" : std::to_string(r.entries[r.entries.size() - 1].second)) << std::endl;
    }

    return 0;
}