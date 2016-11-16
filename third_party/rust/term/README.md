term
====

A Rust library for terminfo parsing and terminal colors.

[![Build Status](https://travis-ci.org/Stebalien/term.svg?branch=master)](https://travis-ci.org/Stebalien/term)
[![Build status](https://ci.appveyor.com/api/projects/status/2duvop23k4n3owyt?svg=true)](https://ci.appveyor.com/project/Stebalien/term)

[Documentation](https://stebalien.github.io/doc/term/term/)

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]

term = "*"
```

and this to your crate root:

```rust
extern crate term;
```

## Packaging and Distributing

For all terminals but windows consoles, this library depends on a non-hashed
(for now) terminfo database being present. For example, on Debian derivitives,
you should depend on ncurses-term; on Arch Linux, you depend on ncurses; and on
MinGW, you should depend on mingw32-terminfo.

Unfortunately, if you're using a non-windows console on Windows (e.g. MinGW,
Cygwin, Git Bash), you'll need to set the TERMINFO environment variable to
point to the directory containing the terminfo database.
