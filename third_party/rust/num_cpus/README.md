# num_cpus

[![crates.io](http://meritbadge.herokuapp.com/num_cpus)](https://crates.io/crates/num_cpus)
[![Travis CI Status](https://travis-ci.org/seanmonstar/num_cpus.svg?branch=master)](https://travis-ci.org/seanmonstar/num_cpus)
[![AppVeyor status](https://ci.appveyor.com/api/projects/status/qn8t6grhko5jwno6?svg=true)](https://ci.appveyor.com/project/seanmonstar/num-cpus)

[Documentation](https://docs.rs/num_cpus)

Count the number of CPUs on the current machine.

## Usage

Add to Cargo.toml:

```toml
[dependencies]
num_cpus = "1.0"
```

In your `main.rs` or `lib.rs`:

```rust
extern crate num_cpus;

// elsewhere
let num = num_cpus::get();
```

## License

Licensed under either of

 * Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any
additional terms or conditions.
