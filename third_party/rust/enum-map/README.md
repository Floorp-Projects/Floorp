<!--
SPDX-FileCopyrightText: 2017 - 2023 Kamila Borowska <kamila@borowska.pw>

SPDX-License-Identifier: MIT OR Apache-2.0
-->

# enum-map

A library providing enum map providing type safe enum array. It is
implemented using regular Rust arrays, so using them is as fast
as using regular Rust arrays.

This crate follows the "N minus two" MSRV policy. This means that it
supports the current Rust release, as well as the two before that.

## Examples

```rust
#[macro_use]
extern crate enum_map;

use enum_map::EnumMap;

#[derive(Debug, Enum)]
enum Example {
    A,
    B,
    C,
}

fn main() {
    let mut map = enum_map! {
        Example::A => 1,
        Example::B => 2,
        Example::C => 3,
    };
    map[Example::C] = 4;

    assert_eq!(map[Example::A], 1);

    for (key, &value) in &map {
        println!("{:?} has {} as value.", key, value);
    }
}
```
