uuid
====

[![Build Status](https://travis-ci.org/rust-lang-nursery/uuid.svg?branch=master)](https://travis-ci.org/rust-lang-nursery/uuid)
[![](http://meritbadge.herokuapp.com/uuid)](https://crates.io/crates/uuid)

A Rust library to generate and parse UUIDs.

Provides support for Universally Unique Identifiers (UUIDs). A UUID is a unique
128-bit number, stored as 16 octets. UUIDs are used to assign unique identifiers
to entities without requiring a central allocating authority.

They are particularly useful in distributed systems, though can be used in
disparate areas, such as databases and network protocols. Typically a UUID is
displayed in a readable string form as a sequence of hexadecimal digits,
separated into groups by hyphens.

The uniqueness property is not strictly guaranteed, however for all practical
purposes, it can be assumed that an unintentional collision would be extremely
unlikely.

[Documentation](https://doc.rust-lang.org/uuid)

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]

uuid = "0.1"
```

and this to your crate root:

```rust
extern crate uuid;
```

## Examples

To create a new random (V4) UUID and print it out in hexadecimal form:

```rust
use uuid::Uuid;

fn main() {
    let my_uuid = Uuid::new_v4();
    println!("{}", my_uuid);
}
```

The library supports 5 versions of UUID:

Name     | Version
---------|----------
Mac      | Version 1: MAC address
Dce      | Version 2: DCE Security
Md5      | Version 3: MD5 hash
Random   | Version 4: Random
Sha1     | Version 5: SHA-1 hash

To parse a simple UUID, then print the version and urn string format:

```rust
use uuid::Uuid;

fn main() {
    let my_uuid = Uuid::parse_str("936DA01F9ABD4d9d80C702AF85C822A8").unwrap();
    println!("Parsed a version {} UUID.", my_uuid.get_version_num());
    println!("{}", my_uuid.to_urn_string());
}
```

## References

[Wikipedia: Universally Unique Identifier](https://en.wikipedia.org/wiki/Universally_unique_identifier)
