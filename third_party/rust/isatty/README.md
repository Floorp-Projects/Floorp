# Rust isatty

[![Build Status](https://travis-ci.org/dtolnay/isatty.svg?branch=master)](https://travis-ci.org/dtolnay/isatty)
[![Latest Version](https://img.shields.io/crates/v/isatty.svg)](https://crates.io/crates/isatty)

This crate provides the following two functions:

```rust
fn stdout_isatty() -> bool
fn stderr_isatty() -> bool
```

On Linux and Mac they are implemented with
[`libc::isatty`](http://man7.org/linux/man-pages/man3/isatty.3.html).
On Windows they are implemented with
[`kernel32::GetConsoleMode`](https://msdn.microsoft.com/en-us/library/windows/desktop/ms683167.aspx).

## Usage

`Cargo.toml`

> ```toml
> [dependencies]
> isatty = "0.1"
> ```

`src/main.rs`

> ```rust
> extern crate isatty;
> use isatty::{stdout_isatty, stderr_isatty};
> 
> fn main() {
>     println!("stdout: {}", stdout_isatty());
>     println!("stderr: {}", stderr_isatty());
> }
> ```

## License

Licensed under either of

 * Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in isatty by you, as defined in the Apache-2.0 license, shall be
dual licensed as above, without any additional terms or conditions.
