dtoa
====

[<img alt="github" src="https://img.shields.io/badge/github-dtolnay/dtoa-8da0cb?style=for-the-badge&labelColor=555555&logo=github" height="20">](https://github.com/dtolnay/dtoa)
[<img alt="crates.io" src="https://img.shields.io/crates/v/dtoa.svg?style=for-the-badge&color=fc8d62&logo=rust" height="20">](https://crates.io/crates/dtoa)
[<img alt="docs.rs" src="https://img.shields.io/badge/docs.rs-dtoa-66c2a5?style=for-the-badge&labelColor=555555&logoColor=white&logo=data:image/svg+xml;base64,PHN2ZyByb2xlPSJpbWciIHhtbG5zPSJodHRwOi8vd3d3LnczLm9yZy8yMDAwL3N2ZyIgdmlld0JveD0iMCAwIDUxMiA1MTIiPjxwYXRoIGZpbGw9IiNmNWY1ZjUiIGQ9Ik00ODguNiAyNTAuMkwzOTIgMjE0VjEwNS41YzAtMTUtOS4zLTI4LjQtMjMuNC0zMy43bC0xMDAtMzcuNWMtOC4xLTMuMS0xNy4xLTMuMS0yNS4zIDBsLTEwMCAzNy41Yy0xNC4xIDUuMy0yMy40IDE4LjctMjMuNCAzMy43VjIxNGwtOTYuNiAzNi4yQzkuMyAyNTUuNSAwIDI2OC45IDAgMjgzLjlWMzk0YzAgMTMuNiA3LjcgMjYuMSAxOS45IDMyLjJsMTAwIDUwYzEwLjEgNS4xIDIyLjEgNS4xIDMyLjIgMGwxMDMuOS01MiAxMDMuOSA1MmMxMC4xIDUuMSAyMi4xIDUuMSAzMi4yIDBsMTAwLTUwYzEyLjItNi4xIDE5LjktMTguNiAxOS45LTMyLjJWMjgzLjljMC0xNS05LjMtMjguNC0yMy40LTMzLjd6TTM1OCAyMTQuOGwtODUgMzEuOXYtNjguMmw4NS0zN3Y3My4zek0xNTQgMTA0LjFsMTAyLTM4LjIgMTAyIDM4LjJ2LjZsLTEwMiA0MS40LTEwMi00MS40di0uNnptODQgMjkxLjFsLTg1IDQyLjV2LTc5LjFsODUtMzguOHY3NS40em0wLTExMmwtMTAyIDQxLjQtMTAyLTQxLjR2LS42bDEwMi0zOC4yIDEwMiAzOC4ydi42em0yNDAgMTEybC04NSA0Mi41di03OS4xbDg1LTM4Ljh2NzUuNHptMC0xMTJsLTEwMiA0MS40LTEwMi00MS40di0uNmwxMDItMzguMiAxMDIgMzguMnYuNnoiPjwvcGF0aD48L3N2Zz4K" height="20">](https://docs.rs/dtoa)
[<img alt="build status" src="https://img.shields.io/github/workflow/status/dtolnay/dtoa/CI/master?style=for-the-badge" height="20">](https://github.com/dtolnay/dtoa/actions?query=branch%3Amaster)

This crate provides fast functions for printing floating-point primitives to an
[`io::Write`]. The implementation is a straightforward Rust port of [Milo Yip]'s
C++ implementation [dtoa.h]. The original C++ code of each function is included
in comments.

See also [`itoa`] for printing integer primitives.

*Version requirement: rustc 1.0+*

[`io::Write`]: https://doc.rust-lang.org/std/io/trait.Write.html
[Milo Yip]: https://github.com/miloyip
[dtoa.h]: https://github.com/miloyip/rapidjson/blob/master/include/rapidjson/internal/dtoa.h
[`itoa`]: https://github.com/dtolnay/itoa

```toml
[dependencies]
dtoa = "0.4"
```

<br>

## Performance (lower is better)

![performance](https://raw.githubusercontent.com/dtolnay/dtoa/master/performance.png)

<br>

## Examples

```rust
use std::io;

fn main() -> io::Result<()> {
    // Write to a vector or other io::Write.
    let mut buf = Vec::new();
    dtoa::write(&mut buf, 2.71828f64)?;
    println!("{:?}", buf);

    // Write to a stack buffer.
    let mut bytes = [b'\0'; 20];
    let n = dtoa::write(&mut bytes[..], 2.71828f64)?;
    println!("{:?}", &bytes[..n]);

    Ok(())
}
```

The function signature is:

```rust
fn write<W: io::Write, V: dtoa::Floating>(writer: W, value: V) -> io::Result<()>;
```

where `dtoa::Floating` is implemented for f32 and f64. The return value gives
the number of bytes written.

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
