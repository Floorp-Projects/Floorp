# Serde JSON &emsp; [![Build Status]][travis] [![Latest Version]][crates.io] [![Rustc Version 1.15+]][rustc]

[Build Status]: https://api.travis-ci.org/serde-rs/json.svg?branch=master
[travis]: https://travis-ci.org/serde-rs/json
[Latest Version]: https://img.shields.io/crates/v/serde_json.svg
[crates.io]: https://crates.io/crates/serde\_json
[Rustc Version 1.15+]: https://img.shields.io/badge/rustc-1.15+-lightgray.svg
[rustc]: https://blog.rust-lang.org/2017/02/02/Rust-1.15.html

**Serde is a framework for *ser*ializing and *de*serializing Rust data structures efficiently and generically.**

---

```toml
[dependencies]
serde_json = "1.0"
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
   manipulations like insert a key in a particular spot.
 - **As a strongly typed Rust data structure.** When you expect all or most
   of your data to conform to a particular structure and want to get real
   work done without JSON's loosey-goosey nature tripping you up.

Serde JSON provides efficient, flexible, safe ways of converting data
between each of these representations.

## Operating on untyped JSON values

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
[`from_slice`][from_slice] for parsing from a byte slice &[u8] and
[`from_reader`][from_reader] for parsing from any `io::Read` like a File or
a TCP stream.

<a href="http://play.integer32.com/?gist=a266662bc71712e080efbf25ce30f306" target="_blank">
<img align="right" width="50" src="https://raw.githubusercontent.com/serde-rs/serde-rs.github.io/master/img/run.png">
</a>

```rust
extern crate serde_json;

use serde_json::{Value, Error};

fn untyped_example() -> Result<(), Error> {
    // Some JSON input data as a &str. Maybe this comes from the user.
    let data = r#"{
                    "name": "John Doe",
                    "age": 43,
                    "phones": [
                      "+44 1234567",
                      "+44 2345678"
                    ]
                  }"#;

    // Parse the string of data into serde_json::Value.
    let v: Value = serde_json::from_str(data)?;

    // Access parts of the data by indexing with square brackets.
    println!("Please call {} at the number {}", v["name"], v["phones"][0]);

    Ok(())
}
```

The result of square bracket indexing like `v["name"]` is a borrow of the data
at that index, so the type is `&Value`. A JSON map can be indexed with string
keys, while a JSON array can be indexed with integer keys. If the type of the
data is not right for the type with which it is being indexed, or if a map does
not contain the key being indexed, or if the index into a vector is out of
bounds, the returned element is `Value::Null`.

When a `Value` is printed, it is printed as a JSON string. So in the code above,
the output looks like `Please call "John Doe" at the number "+44 1234567"`. The
quotation marks appear because `v["name"]` is a `&Value` containing a JSON
string and its JSON representation is `"John Doe"`. Printing as a plain string
without quotation marks involves converting from a JSON string to a Rust string
with [`as_str()`] or avoiding the use of `Value` as described in the following
section.

[`as_str()`]: https://docs.serde.rs/serde_json/enum.Value.html#method.as_str

The `Value` representation is sufficient for very basic tasks but can be tedious
to work with for anything more significant. Error handling is verbose to
implement correctly, for example imagine trying to detect the presence of
unrecognized fields in the input data. The compiler is powerless to help you
when you make a mistake, for example imagine typoing `v["name"]` as `v["nmae"]`
in one of the dozens of places it is used in your code.

## Parsing JSON as strongly typed data structures

Serde provides a powerful way of mapping JSON data into Rust data structures
largely automatically.

<a href="http://play.integer32.com/?gist=cff572b80d3f078c942a2151e6020adc" target="_blank">
<img align="right" width="50" src="https://raw.githubusercontent.com/serde-rs/serde-rs.github.io/master/img/run.png">
</a>

```rust
extern crate serde;
extern crate serde_json;

#[macro_use]
extern crate serde_derive;

use serde_json::Error;

#[derive(Serialize, Deserialize)]
struct Person {
    name: String,
    age: u8,
    phones: Vec<String>,
}

fn typed_example() -> Result<(), Error> {
    // Some JSON input data as a &str. Maybe this comes from the user.
    let data = r#"{
                    "name": "John Doe",
                    "age": 43,
                    "phones": [
                      "+44 1234567",
                      "+44 2345678"
                    ]
                  }"#;

    // Parse the string of data into a Person object. This is exactly the
    // same function as the one that produced serde_json::Value above, but
    // now we are asking it for a Person as output.
    let p: Person = serde_json::from_str(data)?;

    // Do things just like with any other Rust data structure.
    println!("Please call {} at the number {}", p.name, p.phones[0]);

    Ok(())
}
```

This is the same `serde_json::from_str` function as before, but this time we
assign the return value to a variable of type `Person` so Serde will
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

## Constructing JSON values

Serde JSON provides a [`json!` macro][macro] to build `serde_json::Value`
objects with very natural JSON syntax. In order to use this macro,
`serde_json` needs to be imported with the `#[macro_use]` attribute.

<a href="http://play.integer32.com/?gist=c216d6beabd9429a6ac13b8f88938dfe" target="_blank">
<img align="right" width="50" src="https://raw.githubusercontent.com/serde-rs/serde-rs.github.io/master/img/run.png">
</a>

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

<a href="http://play.integer32.com/?gist=aae3af4d274bd249d1c8a947076355f2" target="_blank">
<img align="right" width="50" src="https://raw.githubusercontent.com/serde-rs/serde-rs.github.io/master/img/run.png">
</a>

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

## Creating JSON by serializing data structures

A data structure can be converted to a JSON string by
[`serde_json::to_string`][to_string]. There is also
[`serde_json::to_vec`][to_vec] which serializes to a `Vec<u8>` and
[`serde_json::to_writer`][to_writer] which serializes to any `io::Write`
such as a File or a TCP stream.

<a href="http://play.integer32.com/?gist=40967ece79921c77fd78ebc8f177c063" target="_blank">
<img align="right" width="50" src="https://raw.githubusercontent.com/serde-rs/serde-rs.github.io/master/img/run.png">
</a>

```rust
extern crate serde;
extern crate serde_json;

#[macro_use]
extern crate serde_derive;

use serde_json::Error;

#[derive(Serialize, Deserialize)]
struct Address {
    street: String,
    city: String,
}

fn print_an_address() -> Result<(), Error> {
    // Some data structure.
    let address = Address {
        street: "10 Downing Street".to_owned(),
        city: "London".to_owned(),
    };

    // Serialize it to a JSON string.
    let j = serde_json::to_string(&address)?;

    // Print, write to a file, or send to an HTTP server.
    println!("{}", j);

    Ok(())
}
```

Any type that implements Serde's `Serialize` trait can be serialized this
way. This includes built-in Rust standard library types like `Vec<T>` and
`HashMap<K, V>`, as well as any structs or enums annotated with
`#[derive(Serialize)]`.

## Performance

It is fast. You should expect in the ballpark of 500 to 1000 megabytes per
second deserialization and 600 to 900 megabytes per second serialization,
depending on the characteristics of your data. This is competitive with the
fastest C and C++ JSON libraries or even 30% faster for many use cases.
Benchmarks live in the [serde-rs/json-benchmark] repo.

[serde-rs/json-benchmark]: https://github.com/serde-rs/json-benchmark

## Getting help

Serde developers live in the #serde channel on
[`irc.mozilla.org`](https://wiki.mozilla.org/IRC). The #rust channel is also a
good resource with generally faster response time but less specific knowledge
about Serde. If IRC is not your thing, we are happy to respond to [GitHub
issues](https://github.com/serde-rs/json/issues/new) as well.

## No-std support

This crate currently requires the Rust standard library. For JSON support in
Serde without a standard library, please see the [`serde-json-core`] crate.

[`serde-json-core`]: https://japaric.github.io/serde-json-core/serde_json_core/

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
[from_reader]: https://docs.serde.rs/serde_json/de/fn.from_reader.html
[to_string]: https://docs.serde.rs/serde_json/ser/fn.to_string.html
[to_vec]: https://docs.serde.rs/serde_json/ser/fn.to_vec.html
[to_writer]: https://docs.serde.rs/serde_json/ser/fn.to_writer.html
[macro]: https://docs.serde.rs/serde_json/macro.json.html
