same-file
=========
A safe and simple **cross platform** crate to determine whether two files or
directories are the same.

[![Linux build status](https://api.travis-ci.org/BurntSushi/same-file.png)](https://travis-ci.org/BurntSushi/same-file)
[![Windows build status](https://ci.appveyor.com/api/projects/status/github/BurntSushi/same-file?svg=true)](https://ci.appveyor.com/project/BurntSushi/same-file)
[![](http://meritbadge.herokuapp.com/same-file)](https://crates.io/crates/same-file)

Dual-licensed under MIT or the [UNLICENSE](http://unlicense.org).

### Documentation

https://docs.rs/same-file

### Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
same-file = "1"
```

and this to your crate root:

```rust
extern crate same_file;
```

### Example

The simplest use of this crate is to use the `is_same_file` function, which
takes two file paths and returns true if and only if they refer to the same
file:

```rust
extern crate same_file;

use same_file::is_same_file;

fn main() {
    assert!(is_same_file("/bin/sh", "/usr/bin/sh").unwrap());
}
```
