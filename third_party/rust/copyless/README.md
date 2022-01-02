## copyless
[![Build Status](https://travis-ci.org/kvark/copyless.svg)](https://travis-ci.org/kvark/copyless)
[![Crates.io](https://img.shields.io/crates/v/copyless.svg)](https://crates.io/crates/copyless)

Rust abstractions can be zero cost in theory, but often reveal quite a few unnecessary `memcpy` calls in practice. This library provides a number of trait extensions for standard containers that expose API that is more friendly to LLVM optimization passes and doesn't end up with as many copies.

It aims to accelerate [WebRender](https://github.com/servo/webrender) and [gfx-rs](https://github.com/gfx-rs/gfx).

## Background

The `memcpy` instructions showed in profiles of WebRender running in Gecko. @jrmuizel built a tool called [memcpy-find](https://github.com/jrmuizel/memcpy-find) that analyzes LLVM IR and spews out the call stacks that end up producing `memcpy` instructions. We figured out a way to convince the compiler to eliminate the copies. This library attempts to make these ways available to Rust ecosystem, at least until the compiler gets smart enough ;)

## Here is a small example

```rust
use copyless::BoxHelper;

enum Foo {
    Small(i8),
    Big([f32; 100]),
}

#[inline(never)]
fn foo() -> Box<Foo> {
    Box::new(Foo::Small(4)) // this has 1 memcopy
    //Box::alloc().init(Foo::Small(4)) // this has 0 memcopies
}

fn main() {
    let z = foo();
    println!("{:?}", &*z as *const _);
}
```

Playground [permalink](https://play.rust-lang.org/?version=stable&mode=release&edition=2018&gist=579ab13345b1266752b1fa4400194cc7).
