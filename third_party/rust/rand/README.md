rand
====

A Rust library for random number generators and other randomness functionality.

[![Build Status](https://travis-ci.org/rust-lang-nursery/rand.svg?branch=master)](https://travis-ci.org/rust-lang-nursery/rand)
[![Build status](https://ci.appveyor.com/api/projects/status/rm5c9o33k3jhchbw?svg=true)](https://ci.appveyor.com/project/alexcrichton/rand)

[Documentation](https://doc.rust-lang.org/rand)

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
rand = "0.3"
```

and this to your crate root:

```rust
extern crate rand;
```

## Examples

There is built-in support for a random number generator (RNG) associated with each thread stored in thread-local storage. This RNG can be accessed via thread_rng, or used implicitly via random. This RNG is normally randomly seeded from an operating-system source of randomness, e.g. /dev/urandom on Unix systems, and will automatically reseed itself from this source after generating 32 KiB of random data.

```rust
let tuple = rand::random::<(f64, char)>();
println!("{:?}", tuple)
```

```rust
use rand::Rng;

let mut rng = rand::thread_rng();
if rng.gen() { // random bool
    println!("i32: {}, u32: {}", rng.gen::<i32>(), rng.gen::<u32>())
}
```

It is also possible to use other RNG types, which have a similar interface. The following uses the "ChaCha" algorithm instead of the default.

```rust
use rand::{Rng, ChaChaRng};

let mut rng = rand::ChaChaRng::new_unseeded();
println!("i32: {}, u32: {}", rng.gen::<i32>(), rng.gen::<u32>())
```

