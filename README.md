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

- [x] Single-threaded insert with leaf and internal node splitting
- [x] Search
- [ ] Concurrent insert (in progress)

## Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j4
./blink_tree_test
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
| `high_key` as strict upper bound | Matches paper's definition; simplifies `move_right` condition |