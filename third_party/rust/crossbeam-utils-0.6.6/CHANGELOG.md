# Version 0.6.6

- Add `UnwindSafe` and `RefUnwindSafe` impls for `AtomicCell`.
- Add `AtomicCell::as_ptr()`.
- Add `AtomicCell::take()`.
- Fix a bug in `AtomicCell::compare_exchange()` and `AtomicCell::compare_and_swap()`.
- Various documentation improvements.

# Version 0.6.5

- Rename `Backoff::is_complete()` to `Backoff::is_completed()`.

# Version 0.6.4

- Add `WaitGroup`, `ShardedLock`, and `Backoff`.
- Add `fetch_*` methods for `AtomicCell<i128>` and `AtomicCell<u128>`.
- Expand documentation.

# Version 0.6.3

- Add `AtomicCell`.
- Improve documentation.

# Version 0.6.2

- Add `Parker`.
- Improve documentation.

# Version 0.6.1

- Fix a soundness bug in `Scope::spawn()`.
- Remove the `T: 'scope` bound on `ScopedJoinHandle`.
 
# Version 0.6.0

- Move `AtomicConsume` to `atomic` module.
- `scope()` returns a `Result` of thread joins.
- Remove `spawn_unchecked`.
- Fix a soundness bug due to incorrect lifetimes.
- Improve documentation.
- Support nested scoped spawns.
- Implement `Copy`, `Hash`, `PartialEq`, and `Eq` for `CachePadded`.
- Add `CachePadded::into_inner()`.

# Version 0.5.0

- Reorganize sub-modules and rename functions.

# Version 0.4.1

- Fix a documentation link.

# Version 0.4.0

- `CachePadded` supports types bigger than 64 bytes.
- Fix a bug in scoped threads where unitialized memory was being dropped.
- Minimum required Rust version is now 1.25.

# Version 0.3.2

- Mark `load_consume` with `#[inline]`.

# Version 0.3.1

- `load_consume` on ARM and AArch64.

# Version 0.3.0

- Add `join` for scoped thread API.
- Add `load_consume` for atomic load-consume memory ordering.
- Remove `AtomicOption`.

# Version 0.2.2

- Support Rust 1.12.1.
- Call `T::clone` when cloning a `CachePadded<T>`.

# Version 0.2.1

- Add `use_std` feature.

# Version 0.2.0

- Add `nightly` feature.
- Use `repr(align(64))` on `CachePadded` with the `nightly` feature.
- Implement `Drop` for `CachePadded<T>`.
- Implement `Clone` for `CachePadded<T>`.
- Implement `From<T>` for `CachePadded<T>`.
- Implement better `Debug` for `CachePadded<T>`.
- Write more tests.
- Add this changelog.
- Change cache line length to 64 bytes.
- Remove `ZerosValid`.

# Version 0.1.0

- Old implementation of `CachePadded` from `crossbeam` version 0.3.0
