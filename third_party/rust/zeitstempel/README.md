# zeitstempel

[![Crates.io version](https://img.shields.io/crates/v/zeitstempel.svg?style=flat-square)](https://crates.io/crates/zeitstempel)
[![docs.rs docs](https://img.shields.io/badge/docs-latest-blue.svg?style=flat-square)](https://docs.rs/zeitstempel)
[![License: MPL 2.0](https://img.shields.io/github/license/badboy/zeitstempel?style=flat-square)](LICENSE)
[![Build Status](https://img.shields.io/github/workflow/status/badboy/zeitstempel/CI?style=flat-square)](https://github.com/badboy/zeitstempel/actions?query=workflow%3ACI)

zeitstempel is German for "timestamp".

---

Time's hard. Correct time is near impossible.

This crate has one purpose: give me a timestamp as an integer, coming from a monotonic clock
source, include time across suspend/hibernation of the host machine and let me compare it to
other timestamps.

It becomes the developer's responsibility to only compare timestamps obtained from this clock source.
Timestamps are not comparable across operating system reboots.

# Why not `std::time::Instant`?

[`std::time::Instant`] fulfills some of our requirements:

[`std::time::Instant`]: https://doc.rust-lang.org/1.47.0/std/time/struct.Instant.html

* It's monotonic, guaranteed ([sort of][rustsource]).
* It can be compared to other timespans.

However:

* It can't be serialized.
* It's not guaranteed that the clock source it uses contains suspend/hibernation time across all operating systems.

[rustsource]: https://doc.rust-lang.org/1.47.0/src/std/time.rs.html#213-237

# Example

```rust
use std::{thread, time::Duration};

let start = zeitstempel::now();
thread::sleep(Duration::from_millis(2));

let diff = Duration::from_nanos(zeitstempel::now() - start);
assert!(diff >= Duration::from_millis(2));
```

# Supported operating systems

We support the following operating systems:

* Windows\*
* macOS
* Linux
* Android
* iOS

For other operating systems there's a fallback to `std::time::Instant`,
compared against a process-global fixed reference point.
We don't guarantee that measured time includes time the system spends in sleep or hibernation.

\* To use native Windows 10 functionality enable the `win10plus` feature. Otherwise it will use the fallback.

# License

MPL 2.0. See [LICENSE](LICENSE).
