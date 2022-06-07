<!--
	This readme is created with https://github.com/livioribeiro/cargo-readme

	Edit `src/lib.rs` and use `cargo readme > README.md` to update it.
-->

# fs-err

[![Crates.io](https://img.shields.io/crates/v/fs-err.svg)](https://crates.io/crates/fs-err)
[![GitHub Actions](https://github.com/andrewhickman/fs-err/workflows/CI/badge.svg)](https://github.com/andrewhickman/fs-err/actions?query=workflow%3ACI)

fs-err is a drop-in replacement for [`std::fs`][std::fs] that provides more
helpful messages on errors. Extra information includes which operations was
attempted and any involved paths.

## Error Messages

Using [`std::fs`][std::fs], if this code fails:

```rust
let file = File::open("does not exist.txt")?;
```

The error message that Rust gives you isn't very useful:

```txt
The system cannot find the file specified. (os error 2)
```

...but if we use fs-err instead, our error contains more actionable information:

```txt
failed to open file `does not exist.txt`
    caused by: The system cannot find the file specified. (os error 2)
```

## Usage

fs-err's API is the same as [`std::fs`][std::fs], so migrating code to use it is easy.

```rust
// use std::fs;
use fs_err as fs;

let contents = fs::read_to_string("foo.txt")?;

println!("Read foo.txt: {}", contents);

```

fs-err uses [`std::io::Error`][std::io::Error] for all errors. This helps fs-err
compose well with traits from the standard library like
[`std::io::Read`][std::io::Read] and crates that use them like
[`serde_json`][serde_json]:

```rust
use fs_err::File;

let file = File::open("my-config.json")?;

// If an I/O error occurs inside serde_json, the error will include a file path
// as well as what operation was being performed.
let decoded: Vec<String> = serde_json::from_reader(file)?;

println!("Program config: {:?}", decoded);

```

[std::fs]: https://doc.rust-lang.org/stable/std/fs/
[std::io::Error]: https://doc.rust-lang.org/stable/std/io/struct.Error.html
[std::io::Read]: https://doc.rust-lang.org/stable/std/io/trait.Read.html
[serde_json]: https://crates.io/crates/serde_json

## License

Licensed under either of

* Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or https://www.apache.org/licenses/LICENSE-2.0)
* MIT license ([LICENSE-MIT](LICENSE-MIT) or https://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally
submitted for inclusion in the work by you, as defined in the Apache-2.0
license, shall be dual licensed as above, without any additional terms or
conditions.
