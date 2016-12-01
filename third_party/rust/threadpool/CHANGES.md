# Changes

## 1.3.2

* [Enable `#[deprecated]` doc, requires Rust 1.9](https://github.com/frewsxcv/rust-threadpool/pull/38)

## 1.3.1

* [Implement std::fmt::Debug for ThreadPool](https://github.com/frewsxcv/rust-threadpool/pull/50)

## 1.3.0

* [Add barrier sync example](https://github.com/frewsxcv/rust-threadpool/pull/35)
* [Rename `threads` method/params to `num_threads`, deprecate old usage](https://github.com/frewsxcv/rust-threadpool/pull/34)
* [Stop using deprecated `sleep_ms` function in tests](https://github.com/frewsxcv/rust-threadpool/pull/33)

## 1.2.0

* [New method to determine number of panicked threads](https://github.com/frewsxcv/rust-threadpool/pull/31)

## 1.1.1

* [Silence warning related to unused result](https://github.com/frewsxcv/rust-threadpool/pull/30)
* [Minor doc improvements](https://github.com/frewsxcv/rust-threadpool/pull/30)

## 1.1.0

* [New constructor for specifying thread names for a thread pool](https://github.com/frewsxcv/rust-threadpool/pull/28)

## 1.0.2

* [Use atomic counters](https://github.com/frewsxcv/rust-threadpool/pull/25)

## 1.0.1

* [Switch active_count from Mutex to RwLock for more performance](https://github.com/frewsxcv/rust-threadpool/pull/23)
