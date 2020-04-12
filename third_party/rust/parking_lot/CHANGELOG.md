## parking_lot 0.9.0, parking_lot_core 0.6.0, lock_api 0.3.1 (2019-07-14)

- The minimum supported rust version (MSRV) is now 1.32. This was primarily
  increased for testing with the latest _rand_ crate. Rust 1.31 may continue to
  work for normal use of these releases.
- Re-export lock_api (0.3.1) from parking_lot (#150)
- Removed (non-dev) dependency on rand crate for fairness mechanism, by
  including a simple xorshift PRNG in core (#144)
- Android now uses the futex-based ThreadParker. (#140)
- Fixed CloudABI ThreadParker. (#140)
- Fix race condition in lock_api::ReentrantMutex (da16c2c7)

## lock_api 0.3.0 (2019-07-03, _yanked_)

- Use NonZeroUsize in GetThreadId::nonzero_thread_id (#148)
- Debug assert lock_count in ReentrantMutex (#148)
- Tag as `unsafe` and document some internal methods (#148)
- This release was _yanked_ due to a regression in ReentrantMutex (da16c2c7)

## parking_lot 0.8.1 (2019-07-03, _yanked_)

- Re-export lock_api (0.3.0) from parking_lot (#150)
- This release was _yanked_ from crates.io due to unexpected breakage (#156)

## parking_lot 0.8.0, parking_lot_core 0.5.0, lock_api 0.2.0 (2019-05-04)

- Fix race conditions in deadlock detection.
- Support for more platforms by adding ThreadParker implementations for
  Wasm, Redox, SGX and CloudABI.
- Drop support for older Rust. parking_lot now requires 1.31 and is a
  Rust 2018 edition crate (#122).
- Disable the owning_ref feature by default.
- Fix was_last_thread value in the timeout callback of park() (#129).
- Support single byte Mutex/Once on stable Rust when compiler is at least
  version 1.34.
- Make Condvar::new and Once::new const fns on stable Rust and remove
  ONCE_INIT (#134).
- Add optional Serde support (#135).

## parking_lot 0.7.1 (2019-01-01)

- Fixed potential deadlock when upgrading a RwLock.
- Fixed overflow panic on very long timeouts (#111).

## parking_lot 0.7.0, parking_lot_core 0.4.0 (2018-11-26)

- Return if or how many threads were notified from `Condvar::notify_*`

## parking_lot 0.6.3 (2018-07-18)

- Export `RawMutex`, `RawRwLock` and `RawThreadId`.

## parking_lot 0.6.2 (2018-06-18)

- Enable `lock_api/nightly` feature from `parking_lot/nightly` (#79)

## parking_lot 0.6.1 (2018-06-08)

Added missing typedefs for mapped lock guards:

- `MappedMutexGuard`
- `MappedReentrantMutexGuard`
- `MappedRwLockReadGuard`
- `MappedRwLockWriteGuard`

## parking_lot 0.6.0 (2018-06-08)

This release moves most of the code for type-safe `Mutex` and `RwLock` types
into a separate crate called `lock_api`. This new crate is compatible with
`no_std` and provides `Mutex` and `RwLock` type-safe wrapper types from a raw
mutex type which implements the `RawMutex` or `RawRwLock` trait. The API
provided by the wrapper types can be extended by implementing more traits on
the raw mutex type which provide more functionality (e.g. `RawMutexTimed`). See
the crate documentation for more details.

There are also several major changes:

- The minimum required Rust version is bumped to 1.26.
- All methods on `MutexGuard` (and other guard types) are no longer inherent
  methods and must be called as `MutexGuard::method(self)`. This avoids
  conflicts with methods from the inner type.
- `MutexGuard` (and other guard types) add the `unlocked` method which
  temporarily unlocks a mutex, runs the given closure, and then re-locks the
   mutex.
- `MutexGuard` (and other guard types) add the `bump` method which gives a
  chance for other threads to acquire the mutex by temporarily unlocking it and
  re-locking it. However this is optimized for the common case where there are
  no threads waiting on the lock, in which case no unlocking is performed.
- `MutexGuard` (and other guard types) add the `map` method which returns a
  `MappedMutexGuard` which holds only a subset of the original locked type. The
  `MappedMutexGuard` type is identical to `MutexGuard` except that it does not
  support the `unlocked` and `bump` methods, and can't be used with `CondVar`.
