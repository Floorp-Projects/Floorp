scoped-threadpool-rs
==============

[![Travis-CI Status](https://travis-ci.org/Kimundi/scoped-threadpool-rs.png?branch=master)](https://travis-ci.org/Kimundi/scoped-threadpool-rs)

A library for scoped and cached threadpools.

For more details, see the [docs](http://kimundi.github.io/scoped-threadpool-rs/scoped_threadpool/index.html).

# Getting Started

[scoped-threadpool-rs is available on crates.io](https://crates.io/crates/scoped_threadpool).
Add the following dependency to your Cargo manifest to get the latest version of the 0.1 branch:
```toml
[dependencies]

scoped_threadpool = "0.1.*"
```

To always get the latest version, add this git repository to your
Cargo manifest:

```toml
[dependencies.scoped_threadpool]
git = "https://github.com/Kimundi/scoped-threadpool-rs"
```
# Example

```rust
extern crate scoped_threadpool;
use scoped_threadpool::Pool;

fn main() {
    // Create a threadpool holding 4 threads
    let mut pool = Pool::new(4);

    let mut vec = vec![0, 1, 2, 3, 4, 5, 6, 7];

    // Use the threads as scoped threads that can
    // reference anything outside this closure
    pool.scoped(|scoped| {
        // Create references to each element in the vector ...
        for e in &mut vec {
            // ... and add 1 to it in a seperate thread
            scoped.execute(move || {
                *e += 1;
            });
        }
    });

    assert_eq!(vec, vec![1, 2, 3, 4, 5, 6, 7, 8]);
}
```
