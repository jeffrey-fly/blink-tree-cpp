#include <cassert>
#include <iostream>
#include <optional>
#include <string>
#include <vector>
#include <algorithm>
#include <random>
#include <thread>
#include <atomic>
#include <mutex>

#include "blink_tree.hpp"

#define CHECK(cond, msg)                                                     \
    do {                                                                     \
        if (!(cond)) {                                                       \
            BLinkTree_Print();                                               \
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
// Test cases (single-threaded)
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
// Test cases (concurrent)
//
// NOTE: these tests assume BLinkTree_Insert / BLinkTree_Search are safe to
// call concurrently from multiple threads on the same tree instance (i.e.
// the Lehman-Yao lock-coupling / latch protocol is implemented). Until the
// concurrent version lands, these will likely fail under TSAN or produce
// lost/duplicated entries — that's exactly the signal you want.
//
// Design choice: each thread is given a *disjoint* partition of the key
// space. This sidesteps the question of "what does it mean for two threads
// to race on the same key" (upsert vs reject semantics under concurrency is
// a separate, harder question) and isolates the property we actually want
// to test first: structural correctness of concurrent splits when threads
// touch different parts of the tree, possibly the same node.
// ---------------------------------------------------------------------------

// All threads insert into disjoint contiguous ranges. This still forces
// real contention: with a small max_keys_per_node, ranges land in
// neighboring leaves and threads will race on shared ancestors / right
// siblings during split propagation.
static void TestConcurrentDisjointRanges(size_t num_threads, size_t keys_per_thread)
{
    BlinkTree_Reset();

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (size_t t = 0; t < num_threads; ++t)
    {
        threads.emplace_back([t, keys_per_thread]() {
            Key start = static_cast<Key>(t * keys_per_thread) + 1;
            Key end = static_cast<Key>((t + 1) * keys_per_thread);
            for (Key k = start; k <= end; ++k)
            {
                BLinkTree_Insert(k, static_cast<Value>(k) * 100);
            }
        });
    }
    for (auto& th : threads) th.join();

    std::vector<Key> all_keys;
    for (Key k = 1; k <= static_cast<Key>(num_threads * keys_per_thread); ++k)
        all_keys.push_back(k);

    VerifyAll(all_keys, {static_cast<Key>(num_threads * keys_per_thread) + 1});
    std::cout << "  [OK] TestConcurrentDisjointRanges(threads=" << num_threads
               << ", keys_per_thread=" << keys_per_thread << ")" << std::endl;
}

// Same disjoint-partition idea, but each thread's own slice is shuffled
// before insertion (instead of ascending), so within-thread order is also
// randomized while still avoiding cross-thread key collisions.
static void TestConcurrentDisjointShuffled(size_t num_threads, size_t keys_per_thread, unsigned seed)
{
    BlinkTree_Reset();

    std::vector<std::thread> threads;
    threads.reserve(num_threads);

    for (size_t t = 0; t < num_threads; ++t)
    {
        threads.emplace_back([t, keys_per_thread, seed]() {
            Key start = static_cast<Key>(t * keys_per_thread) + 1;
            Key end = static_cast<Key>((t + 1) * keys_per_thread);

            std::vector<Key> keys;
            for (Key k = start; k <= end; ++k) keys.push_back(k);

            // Seed varies per-thread so threads don't all shuffle identically,
            // but is still derived deterministically from the input seed.
            std::mt19937 rng(seed + static_cast<unsigned>(t));
            std::shuffle(keys.begin(), keys.end(), rng);

            for (Key k : keys)
            {
                BLinkTree_Insert(k, static_cast<Value>(k) * 100);
            }
        });
    }
    for (auto& th : threads) th.join();

    std::vector<Key> all_keys;
    for (Key k = 1; k <= static_cast<Key>(num_threads * keys_per_thread); ++k)
        all_keys.push_back(k);

    VerifyAll(all_keys, {static_cast<Key>(num_threads * keys_per_thread) + 1});
    std::cout << "  [OK] TestConcurrentDisjointShuffled(threads=" << num_threads
               << ", keys_per_thread=" << keys_per_thread
               << ", seed=" << seed << ")" << std::endl;
}

// Interleaved variant: instead of contiguous per-thread ranges (which tend
// to map to contiguous leaves), assign keys round-robin across threads so
// that concurrently-inserted keys are much more likely to land in the SAME
// leaf at the same time. This is a harsher test of the split/lock protocol
// than contiguous ranges, since it maximizes same-node contention.
static void TestConcurrentInterleaved(size_t num_threads, size_t n)
{
    BlinkTree_Reset();

    std::vector<std::vector<Key>> per_thread_keys(num_threads);
    for (Key k = 1; k <= static_cast<Key>(n); ++k)
    {
        per_thread_keys[(k - 1) % num_threads].push_back(k);
    }

    std::vector<std::thread> threads;
    threads.reserve(num_threads);
    for (size_t t = 0; t < num_threads; ++t)
    {
        threads.emplace_back([&per_thread_keys, t]() {
            for (Key k : per_thread_keys[t])
            {
                BLinkTree_Insert(k, static_cast<Value>(k) * 100);
            }
        });
    }
    for (auto& th : threads) th.join();

    std::vector<Key> all_keys;
    for (Key k = 1; k <= static_cast<Key>(n); ++k) all_keys.push_back(k);

    VerifyAll(all_keys, {static_cast<Key>(n) + 1});
    std::cout << "  [OK] TestConcurrentInterleaved(threads=" << num_threads
               << ", n=" << n << ")" << std::endl;
}

// Concurrent reads racing with concurrent writes. Readers repeatedly search
// for keys that have already been (or are about to be) inserted by writers.
// A reader must never see a torn/partial node during a split — it should
// either find the key or correctly report it missing, never crash, hang,
// or return a wrong value for a key that IS present.
static void TestConcurrentReadWrite(size_t num_writer_threads, size_t num_reader_threads, size_t n)
{
    BlinkTree_Reset();

    std::atomic<bool> stop_readers{false};
    std::atomic<size_t> reader_iterations{0};

    std::vector<std::vector<Key>> per_thread_keys(num_writer_threads);
    for (Key k = 1; k <= static_cast<Key>(n); ++k)
    {
        per_thread_keys[(k - 1) % num_writer_threads].push_back(k);
    }

    std::vector<std::thread> writers;
    writers.reserve(num_writer_threads);
    for (size_t t = 0; t < num_writer_threads; ++t)
    {
        writers.emplace_back([&per_thread_keys, t]() {
            for (Key k : per_thread_keys[t])
            {
                BLinkTree_Insert(k, static_cast<Value>(k) * 100);
            }
        });
    }

    std::vector<std::thread> readers;
    readers.reserve(num_reader_threads);
    for (size_t r = 0; r < num_reader_threads; ++r)
    {
        readers.emplace_back([&stop_readers, &reader_iterations, n]() {
            std::mt19937 rng(std::random_device{}());
            std::uniform_int_distribution<Key> dist(1, static_cast<Key>(n));
            while (!stop_readers.load(std::memory_order_relaxed))
            {
                Key k = dist(rng);
                auto result = BLinkTree_Search(k);
                // Whatever the outcome, if we DID find it, the value must be
                // correct — a wrong value would indicate we read a torn or
                // stale node during a concurrent split.
                if (!result.entries.empty())
                {
                    CHECK(result.entries.back().second == k * 100,
                          "reader observed wrong value for key " + std::to_string(k) +
                          " during concurrent insert");
                }
                reader_iterations.fetch_add(1, std::memory_order_relaxed);
            }
        });
    }

    for (auto& th : writers) th.join();
    stop_readers.store(true, std::memory_order_relaxed);
    for (auto& th : readers) th.join();

    CHECK(reader_iterations.load() > 0, "readers should have made progress");

    std::vector<Key> all_keys;
    for (Key k = 1; k <= static_cast<Key>(n); ++k) all_keys.push_back(k);
    VerifyAll(all_keys, {static_cast<Key>(n) + 1});

    std::cout << "  [OK] TestConcurrentReadWrite(writers=" << num_writer_threads
               << ", readers=" << num_reader_threads << ", n=" << n
               << ", reader_iterations=" << reader_iterations.load() << ")" << std::endl;
}

// Runs one of the concurrent scenarios repeatedly across several seeds /
// thread counts to shake out flaky interleavings. Concurrency bugs are often
// timing-dependent, so a single pass proves much less than N passes.
static void TestConcurrentStress(size_t repetitions)
{
    const size_t hw = std::max(2u, std::thread::hardware_concurrency());
    for (size_t i = 0; i < repetitions; ++i)
    {
        TestConcurrentDisjointRanges(/*num_threads=*/hw, /*keys_per_thread=*/50);
        TestConcurrentDisjointShuffled(/*num_threads=*/hw, /*keys_per_thread=*/50, /*seed=*/1000 + static_cast<unsigned>(i));
        TestConcurrentInterleaved(/*num_threads=*/hw, /*n=*/hw * 50);
    }
    std::cout << "  [OK] TestConcurrentStress(repetitions=" << repetitions
               << ", hardware_concurrency=" << hw << ")" << std::endl;
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

// Concurrent tests get their own runner: they're meaningfully slower (real
// OS threads, TSAN overhead) and it's useful to be able to run them
// separately from the fast single-threaded suite above, e.g.
//   ./test_blink_tree --concurrent-only
static void RunAllConcurrentTestsForMaxKeys(size_t max_keys)
{
    std::cout << "[concurrent] max_keys_per_node = " << max_keys << ":" << std::endl;
    BLinkTree_Init(max_keys);

    TestConcurrentDisjointRanges(/*num_threads=*/4, /*keys_per_thread=*/25);
    TestConcurrentDisjointShuffled(/*num_threads=*/4, /*keys_per_thread=*/25, /*seed=*/42);
    TestConcurrentInterleaved(/*num_threads=*/4, /*n=*/200);
    TestConcurrentReadWrite(/*num_writer_threads=*/4, /*num_reader_threads=*/4, /*n=*/200);

    std::cout << "  all concurrent tests passed for max_keys_per_node=" << max_keys
               << std::endl << std::endl;
}

int main(int argc, char** argv)
{
    bool concurrent_only = false;
    bool skip_concurrent = false;
    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if (arg == "--concurrent-only") concurrent_only = true;
        if (arg == "--no-concurrent") skip_concurrent = true;
    }

    // A spread of fanout values: smallest possible (forces splitting almost
    // immediately), a couple of odd/even middle values (mid_index rounding
    // tends to differ for odd vs even), and a larger one (fewer splits,
    // different code path coverage).
    const std::vector<size_t> fanouts = {1, 2, 3, 4, 5, 8, 16};

    if (!concurrent_only)
    {
        for (size_t max_keys : fanouts)
        {
            RunAllTestsForMaxKeys(max_keys);
        }
    }
    
    if (!skip_concurrent)
    {
        for (size_t max_keys : fanouts)
        {
            RunAllConcurrentTestsForMaxKeys(max_keys);
        }
        TestConcurrentStress(/*repetitions=*/3);
    }

    std::cout << "ALL TESTS PASSED" << std::endl;
    return 0;
}