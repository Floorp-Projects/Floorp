# Serde JSON &emsp; [![Build Status](https://api.travis-ci.org/serde-rs/json.svg?branch=master)](https://travis-ci.org/serde-rs/json) [![Latest Version](https://img.shields.io/crates/v/serde_json.svg)](https://crates.io/crates/serde\_json)

**Serde is a framework for *ser*ializing and *de*serializing Rust data structures efficiently and generically.**

---

```toml
[dependencies]
serde_json = "0.9"
```

You may be looking for:

- [JSON API documentation](https://docs.serde.rs/serde_json/)
- [Serde API documentation](https://docs.serde.rs/serde/)
- [Detailed documentation about Serde](https://serde.rs/)
- [Setting up `#[derive(Serialize, Deserialize)]`](https://serde.rs/codegen.html)
- [Release notes](https://github.com/serde-rs/json/releases)

JSON is a ubiquitous open-standard format that uses human-readable text to
transmit data objects consisting of key-value pairs.

```json,ignore
{
  "name": "John Doe",
  "age": 43,
  "address": {
    "street": "10 Downing Street",
    "city": "London"
  },
  "phones": [
    "+44 1234567",
    "+44 2345678"
  ]
}
```

There are three common ways that you might find yourself needing to work
with JSON data in Rust.

 - **As text data.** An unprocessed string of JSON data that you receive on
   an HTTP endpoint, read from a file, or prepare to send to a remote
   server.
 - **As an untyped or loosely typed representation.** Maybe you want to
   check that some JSON data is valid before passing it on, but without
   knowing the structure of what it contains. Or you want to do very basic
   manipulations like add a level of nesting.
 - **As a strongly typed Rust data structure.** When you expect all or most
   of your data to conform to a particular structure and want to get real
   work done without JSON's loosey-goosey nature tripping you up.

Serde JSON provides efficient, flexible, safe ways of converting data
between each of these representations.

## JSON to the Value enum

Any valid JSON data can be manipulated in the following recursive enum
representation. This data structure is [`serde_json::Value`][value].

```rust,ignore
enum Value {
    Null,
    Bool(bool),
    Number(Number),
    String(String),
    Array(Vec<Value>),
    Object(Map<String, Value>),
}
```

A string of JSON data can be parsed into a `serde_json::Value` by the
[`serde_json::from_str`][from_str] function. There is also
[`from_slice`][from_slice] for parsing from a byte slice &[u8],
[`from_iter`][from_iter] for parsing from an iterator of bytes, and
[`from_reader`][from_reader] for parsing from any `io::Read` like a File or
a TCP stream.

```rust
use serde_json::Value;

let data = r#" { "name": "John Doe", "age": 43, ... } "#;
let v: Value = serde_json::from_str(data)?;
println!("Please call {} at the number {}", v["name"], v["phones"][0]);
```

The `Value` representation is sufficient for very basic tasks but is brittle
and tedious to work with. Error handling is verbose to implement correctly,
for example imagine trying to detect the presence of unrecognized fields in
the input data. The compiler is powerless to help you when you make a
mistake, for example imagine typoing `v["name"]` as `v["nmae"]` in one of
the dozens of places it is used in your code.

## JSON to strongly typed data structures

Serde provides a powerful way of mapping JSON data into Rust data structures
largely automatically.

```rust
#[derive(Serialize, Deserialize)]
struct Person {
    name: String,
    age: u8,
    address: Address,
    phones: Vec<String>,
}

#[derive(Serialize, Deserialize)]
struct Address {
    street: String,
    city: String,
}

let data = r#" { "name": "John Doe", "age": 43, ... } "#;
let p: Person = serde_json::from_str(data)?;
println!("Please call {} at the number {}", p.name, p.phones[0]);
```

This is the same `serde_json::from_str` function as before, but this time we
assign the return value to a variable of type `Person` so Serde JSON will
automatically interpret the input data as a `Person` and produce informative
error messages if the layout does not conform to what a `Person` is expected
to look like.

Any type that implements Serde's `Deserialize` trait can be deserialized
this way. This includes built-in Rust standard library types like `Vec<T>`
and `HashMap<K, V>`, as well as any structs or enums annotated with
`#[derive(Deserialize)]`.

Once we have `p` of type `Person`, our IDE and the Rust compiler can help us
use it correctly like they do for any other Rust code. The IDE can
autocomplete field names to prevent typos, which was impossible in the
`serde_json::Value` representation. And the Rust compiler can check that
when we write `p.phones[0]`, then `p.phones` is guaranteed to be a
`Vec<String>` so indexing into it makes sense and produces a `String`.

## Constructing JSON

Serde JSON provides a [`json!` macro][macro] to build `serde_json::Value`
objects with very natural JSON syntax. In order to use this macro,
`serde_json` needs to be imported with the `#[macro_use]` attribute.

```rust
#[macro_use]
extern crate serde_json;

fn main() {
    // The type of `john` is `serde_json::Value`
    let john = json!({
      "name": "John Doe",
      "age": 43,
      "phones": [
        "+44 1234567",
        "+44 2345678"
      ]
    });

    println!("first phone number: {}", john["phones"][0]);

    // Convert to a string of JSON and print it out
    println!("{}", john.to_string());
}
```

The `Value::to_string()` function converts a `serde_json::Value` into a
`String` of JSON text.

One neat thing about the `json!` macro is that variables and expressions can
be interpolated directly into the JSON value as you are building it. Serde
will check at compile time that the value you are interpolating is able to
be represented as JSON.

```rust
let full_name = "John Doe";
let age_last_year = 42;

// The type of `john` is `serde_json::Value`
let john = json!({
  "name": full_name,
  "age": age_last_year + 1,
  "phones": [
    format!("+44 {}", random_phone())
  ]
});
```

This is amazingly convenient but we have the problem we had before with
`Value` which is that the IDE and Rust compiler cannot help us if we get it
wrong. Serde JSON provides a better way of serializing strongly-typed data
structures into JSON text.

## Serializing data structures

A data structure can be converted to a JSON string by
[`serde_json::to_string`][to_string]. There is also
[`serde_json::to_vec`][to_vec] which serializes to a `Vec<u8>` and
[`serde_json::to_writer`][to_writer] which serializes to any `io::Write`
such as a File or a TCP stream.

```rust
#[derive(Serialize, Deserialize)]
struct Address {
    street: String,
    city: String,
}

let address = Address {
    street: "10 Downing Street".to_owned(),
    city: "London".to_owned(),
};

let j = serde_json::to_string(&address)?;
```

Any type that implements Serde's `Serialize` trait can be serialized this
way. This includes built-in Rust standard library types like `Vec<T>` and
`HashMap<K, V>`, as well as any structs or enums annotated with
`#[derive(Serialize)]`.

## Getting help

Serde developers live in the #serde channel on
[`irc.mozilla.org`](https://wiki.mozilla.org/IRC). The #rust channel is also a
good resource with generally faster response time but less specific knowledge
about Serde. If IRC is not your thing, we are happy to respond to [GitHub
issues](https://github.com/serde-rs/json/issues/new) as well.

## License

Serde JSON is licensed under either of

 * Apache License, Version 2.0, ([LICENSE-APACHE](LICENSE-APACHE) or
   http://www.apache.org/licenses/LICENSE-2.0)
 * MIT license ([LICENSE-MIT](LICENSE-MIT) or
   http://opensource.org/licenses/MIT)

at your option.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in Serde JSON by you, as defined in the Apache-2.0 license, shall
be dual licensed as above, without any additional terms or conditions.

[value]: https://docs.serde.rs/serde_json/value/enum.Value.html
[from_str]: https://docs.serde.rs/serde_json/de/fn.from_str.html
[from_slice]: https://docs.serde.rs/serde_json/de/fn.from_slice.html
[from_iter]: https://docs.serde.rs/serde_json/de/fn.from_iter.html
[from_reader]: https://docs.serde.rs/serde_json/de/fn.from_reader.html
[to_string]: https://docs.serde.rs/serde_json/ser/fn.to_string.html
[to_vec]: https://docs.serde.rs/serde_json/ser/fn.to_vec.html
[to_writer]: https://docs.serde.rs/serde_json/ser/fn.to_writer.html
[macro]: https://docs.serde.rs/serde_json/macro.json.html
