# atty

[![Build Status](https://travis-ci.org/softprops/atty.svg?branch=master)](https://travis-ci.org/softprops/atty) [![Build status](https://ci.appveyor.com/api/projects/status/geggrsnsjsuse8cv?svg=true)](https://ci.appveyor.com/project/softprops/atty) [![Coverage Status](https://coveralls.io/repos/softprops/atty/badge.svg?branch=master&service=github)](https://coveralls.io/github/softprops/atty?branch=master) [![crates.io](http://meritbadge.herokuapp.com/atty)](https://crates.io/crates/atty)

> are you or are you not a tty?


[Api documentation](http://softprops.github.io/atty)

## usage

```rust
extern crate atty;

use atty::Stream;

fn main() {
  if atty::is(Stream::Stdout) {
    println!("I'm a terminal");
  } else {
    println!("I'm not");
  }
}
```

## install

Add the following to your `Cargo.toml`

```toml
[dependencies]
atty = "0.2"
```

## testing

This library has been unit tested on both unix and windows platforms (via appveyor).


A simple example program is provided in this repo to test various tty's. By default.

It prints

```bash
$ cargo run --example atty
stdout? true
stderr? true
stdin? true
```

To test std in, pipe some text to the program

```bash
$ echo "test" | cargo run --example atty
stdout? true
stderr? true
stdin? false
```

To test std out, pipe the program to something

```bash
$ cargo run --example atty | grep std
stdout? false
stderr? true
stdin? true
```

To test std err, pipe the program to something redirecting std err

```bash
$ cargo run --example atty 2>&1 | grep std
stdout? false
stderr? false
stdin? true
```

Doug Tangren (softprops) 2015
