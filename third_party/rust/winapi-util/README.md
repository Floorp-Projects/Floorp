winapi-util
===========
This crate provides a smattering of safe wrappers around various parts of the
[winapi](https://crates.io/crates/winapi) crate.

[![Linux build status](https://api.travis-ci.org/BurntSushi/winapi-util.png)](https://travis-ci.org/BurntSushi/winapi-util)
[![Windows build status](https://ci.appveyor.com/api/projects/status/github/BurntSushi/winapi-util?svg=true)](https://ci.appveyor.com/project/BurntSushi/winapi-util)
[![](http://meritbadge.herokuapp.com/winapi-util)](https://crates.io/crates/winapi-util)

Dual-licensed under MIT or the [UNLICENSE](http://unlicense.org).


### Documentation

https://docs.rs/winapi-util


### Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
winapi-util = "0.1"
```

and this to your crate root:

```rust
extern crate winapi_util;
```


### Notes

This crate was born out of frustration with having to write lots of little
ffi utility bindings in a variety of crates in order to get Windows support.
Eventually, I started needing to copy & paste a lot of those utility routines.
Since they are utility routines, they often don't make sense to expose directly
in the crate in which they are defined. Instead of continuing this process,
I decided to make a crate instead.

Normally, I'm not a huge fan of "utility" crates like this that don't have a
well defined scope, but this is primarily a practical endeavor to make it
easier to isolate Windows specific ffi code.

While I don't have a long term vision for this crate, I will welcome additional
PRs that add more high level routines/types on an as-needed basis.

**WARNING:** I am not a Windows developer, so extra review to make sure I've
got things right is most appreciated.
