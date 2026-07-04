#include <cassert>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>
#include <random>

#include "blink_tree.hpp"

#define CHECK(cond, msg)                                                     \
    do {                                                                     \
        if (!(cond)) {                                                       \
            std::cerr << "CHECK FAILED: " << (msg)                           \
                       << "  [" << #cond << "]"                              \
                       << "  at " << __FILE__ << ":" << __LINE__ << std::endl;\
            std::abort();                                                    \
        }                                                                    \
    } while (0)

// ---------------------------------------------------------------------------
// Test infrastructure
// ---------------------------------------------------------------------------

// Insert a batch of keys, value = key * 100 by convention (easy to eyeball).
static void InsertAll(const std::vector<Key>& keys)
{
    for (Key k : keys)
    {
        BLinkTree_Insert(k, static_cast<Value>(k) * 100);
    }
}

// Checks that every key in `should_exist` is found with the expected value,
// and every key in `should_not_exist` is reported missing.
static void VerifyAll(const std::vector<Key>& should_exist,
                       const std::vector<Key>& should_not_exist)
{
    for (Key k : should_exist)
    {
        auto r = BLinkTree_Search(k);
        CHECK(!r.entries.empty(), "expected key " + std::to_string(k) + " to exist");
        CHECK(r.entries.back().second == k * 100,
              "wrong value for key " + std::to_string(k));
    }
    for (Key k : should_not_exist)
    {
        auto r = BLinkTree_Search(k);
        CHECK(r.entries.empty(), "expected key " + std::to_string(k) + " to be missing");
    }
}

// ---------------------------------------------------------------------------
// Test cases
// ---------------------------------------------------------------------------

static void TestAscending()
{
    BlinkTree_Reset();
    InsertAll({1, 2, 3, 4, 5, 6, 7});
    VerifyAll(/*should_exist=*/{1, 2, 3, 4, 5, 6, 7},
              /*should_not_exist=*/{8});
    std::cout << "  [OK] TestAscending" << std::endl;
}

static void TestDescending()
{
    BlinkTree_Reset();
    InsertAll({7, 6, 5, 4, 3, 2, 1});
    VerifyAll({1, 2, 3, 4, 5, 6, 7}, {8});
    std::cout << "  [OK] TestDescending" << std::endl;
}

static void TestRandomOrder()
{
    BlinkTree_Reset();
    InsertAll({4, 1, 7, 2, 6, 3, 5});
    VerifyAll({1, 2, 3, 4, 5, 6, 7}, {8});
    std::cout << "  [OK] TestRandomOrder" << std::endl;
}

// Keys that land exactly on split boundaries are where off-by-one bugs like
// the mid_index one you just fixed tend to hide. Larger, denser key sets
// exercise multiple levels of splitting (leaf splits AND internal splits,
// possibly multiple root splits), not just a single split.
static void TestDenseSequential(size_t n)
{
    BlinkTree_Reset();
    std::vector<Key> keys;
    for (Key k = 1; k <= static_cast<Key>(n); ++k) keys.push_back(k);
    InsertAll(keys);
    VerifyAll(keys, {static_cast<Key>(n) + 1});
    std::cout << "  [OK] TestDenseSequential(" << n << ")" << std::endl;
}

// Randomized test with a fixed seed. Fixed seed matters: if a test fails,
// you want to be able to re-run it and get the exact same failing sequence,
// not a new random shuffle that might not reproduce the bug.
static void TestShuffled(size_t n, unsigned seed)
{
    BlinkTree_Reset();
    std::vector<Key> keys;
    for (Key k = 1; k <= static_cast<Key>(n); ++k) keys.push_back(k);

    std::mt19937 rng(seed);
    std::shuffle(keys.begin(), keys.end(), rng);

    InsertAll(keys);

    std::vector<Key> sorted_keys = keys;
    std::sort(sorted_keys.begin(), sorted_keys.end());
    VerifyAll(sorted_keys, {static_cast<Key>(n) + 1});
    std::cout << "  [OK] TestShuffled(n=" << n << ", seed=" << seed << ")" << std::endl;
}

// Duplicate insert should not corrupt the tree or silently create a second
// entry, depending on your intended semantics (reject vs overwrite). Adjust
// the CHECK below to match whatever behavior you've decided is correct.
static void TestDuplicateInsert()
{
    BlinkTree_Reset();
    InsertAll({1, 2, 3});
    // TODO: replace with your real insert call, capture return value
    bool inserted_again = BLinkTree_Insert(2, 999);
    CHECK(!inserted_again, "duplicate insert should return false");
    VerifyAll({1, 2, 3}, {4});
    std::cout << "  [OK] TestDuplicateInsert" << std::endl;
}

// ---------------------------------------------------------------------------
// Run every test case for a given max_keys_per_node value.
// ---------------------------------------------------------------------------
static void RunAllTestsForMaxKeys(size_t max_keys)
{
    std::cout << "max_keys_per_node = " << max_keys << ":" << std::endl;
    BLinkTree_Init(max_keys);

    TestAscending();
    TestDescending();
    TestRandomOrder();
    TestDenseSequential(20);
    TestShuffled(20, /*seed=*/12345);
    TestShuffled(50, /*seed=*/67890);
    TestDuplicateInsert();

    std::cout << "  all passed for max_keys_per_node=" << max_keys << std::endl << std::endl;
}

int main()
{
    // A spread of fanout values: smallest possible (forces splitting almost
    // immediately), a couple of odd/even middle values (mid_index rounding
    // tends to differ for odd vs even), and a larger one (fewer splits,
    // different code path coverage).
    for (size_t max_keys : {1, 2, 3, 4, 5, 8, 16})
    {
        RunAllTestsForMaxKeys(max_keys);
    }

    std::cout << "ALL TESTS PASSED" << std::endl;
    return 0;
}