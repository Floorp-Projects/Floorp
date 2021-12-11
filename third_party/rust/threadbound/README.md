ThreadBound\<T\>
================

[<img alt="github" src="https://img.shields.io/badge/github-dtolnay/threadbound-8da0cb?style=for-the-badge&labelColor=555555&logo=github" height="20">](https://github.com/dtolnay/threadbound)
[<img alt="crates.io" src="https://img.shields.io/crates/v/threadbound.svg?style=for-the-badge&color=fc8d62&logo=rust" height="20">](https://crates.io/crates/threadbound)
[<img alt="docs.rs" src="https://img.shields.io/badge/docs.rs-threadbound-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K" height="20">](https://docs.rs/threadbound)
[<img alt="build status" src="https://img.shields.io/github/workflow/status/dtolnay/threadbound/CI/master?style=for-the-badge" height="20">](https://github.com/dtolnay/threadbound/actions?query=branch%3Amaster)

ThreadBound is a wrapper that binds a value to its original thread. The wrapper
gets to be [`Sync`] and [`Send`] but only the original thread on which the
ThreadBound was constructed can retrieve the underlying value.

[`Sync`]: https://doc.rust-lang.org/std/marker/trait.Sync.html
[`Send`]: https://doc.rust-lang.org/std/marker/trait.Send.html

```toml
[dependencies]
threadbound = "0.1"
```

*Version requirement: rustc 1.31+*

<br>

### Example

```rust
extern crate threadbound;

use std::marker::PhantomData;
use std::rc::Rc;
use std::sync::Arc;
use threadbound::ThreadBound;

// Neither Send nor Sync. Maybe the index points into a
// thread-local interner.
#[derive(Copy, Clone)]
struct Span {
    index: u32,
    marker: PhantomData<Rc<()>>,
}

// Error types are always supposed to be Send and Sync.
// We can use ThreadBound to make it so.
struct Error {
    span: ThreadBound<Span>,
    message: String,
}

fn main() {
    let err = Error {
        span: ThreadBound::new(Span {
            index: 99,
            marker: PhantomData,
        }),
        message: "fearless concurrency".to_owned(),
    };

    // Original thread can see the contents.
    assert_eq!(err.span.get_ref().unwrap().index, 99);

    let err = Arc::new(err);
    let err2 = err.clone();
    std::thread::spawn(move || {
        // Other threads cannot get access. Maybe they use
        // a default value or a different codepath.
        assert!(err2.span.get_ref().is_none());
    });

    // Original thread can still see the contents.
    assert_eq!(err.span.get_ref().unwrap().index, 99);
}
```

<br>

#### License

<sup>
Licensed under either of <a href="LICENSE-APACHE">Apache License, Version
2.0</a> or <a href="LICENSE-MIT">MIT license</a> at your option.
</sup>

<br>

<sub>
Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this crate by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.
</sub>
