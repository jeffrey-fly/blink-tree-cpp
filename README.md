# blink-tree-cpp

A B-link tree implementation in C++17, based on the Lehman & Yao (1981) paper  
*"Efficient Locking for Concurrent Operations on B-Trees"*.

Built as a learning project to bridge the gap between paper-level theory  
and working systems code.

## What is a B-link tree?

A B-link tree extends the classic B+ tree with two additions per node:
- `right_link`: a pointer to the right sibling at the same level
- `high_key`: the upper bound of keys this node covers

These two fields enable safe concurrent access with minimal locking —  
the core contribution of the Lehman-Yao paper.

## Current Status

This learning project is complete for its current scope: point search and
concurrent insertion. The test suite covers sequential and concurrent use and
can be run with ThreadSanitizer (TSAN) or AddressSanitizer (ASAN).

Range scans and deletion are outside the current scope. No further features
are planned at present. `BLinkTree_Print()` is a diagnostic helper and should
only be called when no other thread is accessing the tree.

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j4
./blink_tree_test
```

## Testing

Concurrency correctness is checked under both sanitizers (mutually exclusive):

```bash
# ThreadSanitizer
cmake .. -DCMAKE_BUILD_TYPE=Debug -DTSAN=ON
make -j4 && ./blink_tree_test

# AddressSanitizer
cmake .. -DCMAKE_BUILD_TYPE=Debug -DASAN=ON
make -j4 && ./blink_tree_test
```

The test binary also supports:

```bash
./blink_tree_test --concurrent-only   # skip single-threaded tests
./blink_tree_test --no-concurrent     # skip concurrent tests (faster iteration)
```

## Paper

Lehman, P. L., & Yao, S. B. (1981).  
Efficient locking for concurrent operations on B-trees.  
*ACM Transactions on Database Systems*, 6(4), 650–670.

## Key Design Decisions

| Decision | Reason |
|----------|--------|
| `NodeId` instead of raw pointer | Simulates page-id based buffer pool; easier to extend to disk storage |
| `unique_ptr` in NodeStore | Clear ownership; nodes never move in memory (mutex non-movable) |
| `high_key` as inclusive maximum key | `move_right` follows `right_link` when the target key is greater than this maximum |