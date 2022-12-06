# Serde CBOR
[![Build Status](https://travis-ci.org/pyfisch/cbor.svg?branch=master)](https://travis-ci.org/pyfisch/cbor)
[![Crates.io](https://img.shields.io/crates/v/serde_cbor.svg)](https://crates.io/crates/serde_cbor)
[![Documentation](https://docs.rs/serde_cbor/badge.svg)](https://docs.rs/serde_cbor)

## PROJECT IS ARCHIVED

After almost 6 years it is time to retire this crate.
This implementation of CBOR for serde is used in hundreds of projects with widely differing needs.
Besides the standard features it contains code for no-std environments, a packed encoding and CBOR tags.
However while these features are useful to many people they sometimes interact poorly with each others and with optional features of serde itself.
Because I don't use the crate myself and because of the potential for new errors I have been reluctant to accept any changes or additional features for the crate.
Since this situation is unlikely to change anytime soon and no one else stepped up to maintain this crate I am archiving the repository today.
If the crate works for you there is no need to switch to another implementation.
However if you encounter problems or for new projects I recommend you take a look at these crates:

* [ciborium](https://crates.io/crates/ciborium)
* [minicbor](https://crates.io/crates/minicbor)

~~ Pyfisch, August 2021



This crate implements the Concise Binary Object Representation from [RFC 7049].
It builds on [Serde], the generic serialization framework for Rust.
CBOR provides a binary encoding for a superset
of the JSON data model that is small and very fast to parse.

[RFC 7049]: https://tools.ietf.org/html/rfc7049
[Serde]: https://github.com/serde-rs/serde

## Usage

Serde CBOR supports Rust 1.40 and up. Add this to your `Cargo.toml`:
```toml
[dependencies]
serde_cbor = "0.11.2"
```

Storing and loading Rust types is easy and requires only
minimal modifications to the program code.

```rust
use serde_derive::{Deserialize, Serialize};
use std::error::Error;
use std::fs::File;

// Types annotated with `Serialize` can be stored as CBOR.
// To be able to load them again add `Deserialize`.
#[derive(Debug, Serialize, Deserialize)]
struct Mascot {
    name: String,
    species: String,
    year_of_birth: u32,
}

fn main() -> Result<(), Box<dyn Error>> {
    let ferris = Mascot {
        name: "Ferris".to_owned(),
        species: "crab".to_owned(),
        year_of_birth: 2015,
    };

    let ferris_file = File::create("examples/ferris.cbor")?;
    // Write Ferris to the given file.
    // Instead of a file you can use any type that implements `io::Write`
    // like a HTTP body, database connection etc.
    serde_cbor::to_writer(ferris_file, &ferris)?;

    let tux_file = File::open("examples/tux.cbor")?;
    // Load Tux from a file.
    // Serde CBOR performs roundtrip serialization meaning that
    // the data will not change in any way.
    let tux: Mascot = serde_cbor::from_reader(tux_file)?;

    println!("{:?}", tux);
    // prints: Mascot { name: "Tux", species: "penguin", year_of_birth: 1996 }

    Ok(())
}
```

There are a lot of options available to customize the format.
To operate on untyped CBOR values have a look at the `Value` type.

## License
Licensed under either of

 * Apache License, Version 2.0 ([LICENSE-APACHE](LICENSE-APACHE) or http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or http://opensource.org/licenses/MIT)

at your option.

### Contribution
Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in the work by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any
additional terms or conditions.
