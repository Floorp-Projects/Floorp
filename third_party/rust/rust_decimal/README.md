# Decimal &emsp; [![Build Status]][actions] [![Latest Version]][crates.io]

[Build Status]: https://img.shields.io/endpoint.svg?url=https%3A%2F%2Factions-badge.atrox.dev%2Fpaupino%2Frust-decimal%2Fbadge&label=build&logo=none
[actions]: https://actions-badge.atrox.dev/paupino/rust-decimal/goto
[Latest Version]: https://img.shields.io/crates/v/rust-decimal.svg
[crates.io]: https://crates.io/crates/rust-decimal

A Decimal implementation written in pure Rust suitable for financial calculations that require significant integral and fractional digits with no round-off errors.

The binary representation consists of a 96 bit integer number, a scaling factor used to specify the decimal fraction and a 1 bit sign. Because of this representation, trailing zeros are preserved and may be exposed when in string form. These can be truncated using the `normalize` or `round_dp` functions.

[Documentation](https://docs.rs/rust_decimal/)

## Usage

Decimal numbers can be created in a few distinct ways. The easiest and most optimal method of creating a Decimal is to use the procedural macro within the `rust_decimal_macros` crate:

```rust
// Procedural macros need importing directly
use rust_decimal_macros::*;

let number = dec!(-1.23);
```

Alternatively you can also use one of the Decimal number convenience functions:

```rust
use rust_decimal::prelude::*;

// Using an integer followed by the decimal points
let scaled = Decimal::new(202, 2); // 2.02

// From a string representation
let from_string = Decimal::from_str("2.02").unwrap(); // 2.02

// Using the `Into` trait
let my_int : Decimal = 3i32.into();

// Using the raw decimal representation
// 3.1415926535897932384626433832
let pi = Decimal::from_parts(1102470952, 185874565, 1703060790, false, 28);
```

## Features

* [db-postgres](#db-postgres)
* [db-tokio-postgres](#db-tokio-postgres)
* [db-diesel-postgres](#db-diesel-postgres)
* [serde-float](#serde-float)
* [serde-bincode](#serde-bincode)

## `db-postgres`

This feature enables a PostgreSQL communication module. It allows for reading and writing the `Decimal`
type by transparently serializing/deserializing into the `NUMERIC` data type within PostgreSQL.

## `db-tokio-postgres`

Enables the tokio postgres module allowing for async communication with PostgreSQL.

## `db-diesel-postgres`

Enable `diesel` PostgreSQL support. 

## `serde-float`

Enable this so that JSON serialization of Decimal types are sent as a float instead of a string (default).

e.g. with this turned on, JSON serialization would output:
```
{
  "value": 1.234
}
```

## `serde-bincode`

Since `bincode` does not specify type information, we need to ensure that a type hint is provided in order to 
correctly be able to deserialize. Enabling this feature on it's own will force deserialization to use `deserialize_str` 
instead of `deserialize_any`. 

If, for some reason, you also have `serde-float` enabled then this will use `deserialize_f64` as a type hint. Because
converting to `f64` _loses_ precision, it's highly recommended that you do NOT enable this feature when working with 
`bincode`. That being said, this will only use 8 bytes so is slightly more efficient in regards to storage size.
