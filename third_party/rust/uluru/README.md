# uluru

A simple, fast, least-recently-used (LRU) cache implementation used for
Servo's style system.

`LRUCache` uses a fixed-capacity array for storage. It provides `O(1)`
insertion, and `O(n)` lookup.  It does not require an allocator and can be
used in `no_std` crates.

* [Documentation](https://docs.rs/uluru)
* [crates.io](https://crates.io/crates/uluru)
* [Release notes](https://github.com/servo/uluru/releases)
