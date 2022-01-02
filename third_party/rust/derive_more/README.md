# `derive_more`

[![Build Status](https://github.com/JelteF/derive_more/workflows/CI/badge.svg)](https://github.com/JelteF/derive_more/actions)
[![Latest Version](https://img.shields.io/crates/v/derive_more.svg)](https://crates.io/crates/derive_more)
[![Rust Documentation](https://img.shields.io/badge/api-rustdoc-blue.svg)](https://jeltef.github.io/derive_more/derive_more/)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/JelteF/derive_more/master/LICENSE)
[![Rust 1.36+](https://img.shields.io/badge/rustc-1.36+-lightgray.svg)](https://blog.rust-lang.org/2019/07/04/Rust-1.36.0.html)

Rust has lots of builtin traits that are implemented for its basic types, such
as `Add`, `Not`, `From` or `Display`.
However, when wrapping these types inside your own structs or enums you lose the
implementations of these traits and are required to recreate them.
This is especially annoying when your own structures are very simple, such as
when using the commonly advised newtype pattern (e.g. `MyInt(i32)`).

This library tries to remove these annoyances and the corresponding boilerplate code.
It does this by allowing you to derive lots of commonly used traits for both structs and enums.

## Example code

By using this library the following code just works:

```rust
extern crate derive_more;
use derive_more::{Add, Display, From, Into};

#[derive(PartialEq, From, Add)]
struct MyInt(i32);

#[derive(PartialEq, From, Into)]
struct Point2D {
    x: i32,
    y: i32,
}

#[derive(PartialEq, From, Add, Display)]
enum MyEnum {
    #[display(fmt = "int: {}", _0)]
    Int(i32),
    Uint(u32),
    #[display(fmt = "nothing")]
    Nothing,
}

assert!(MyInt(11) == MyInt(5) + 6.into());
assert!((5, 6) == Point2D { x: 5, y: 6 }.into());
assert!(MyEnum::Int(15) == (MyEnum::Int(8) + 7.into()).unwrap());
assert!(MyEnum::Int(15).to_string() == "int: 15");
assert!(MyEnum::Uint(42).to_string() == "42");
assert!(MyEnum::Nothing.to_string() == "nothing");
```

## The derivable traits

Below are all the traits that you can derive using this library.
Some trait derivations are so similar that the further documentation will only show a single one
of them.
You can recognize these by the "-like" suffix in their name.
The trait name before that will be the only one that is used throughout the further
documentation.

It is important to understand what code gets generated when using one of the
derives from this crate.
That is why the links below explain what code gets generated for a trait for
each group from before.

You can use the [`cargo-expand`] utility to see the exact code that is generated
for your specific type.
This will show you your code with all macros and derives expanded.

**NOTE**: You still have to derive each trait separately. So `#[derive(Mul)]` doesn't
automatically derive `Div` as well. To derive both you should do `#[derive(Mul, Div)]`

### Conversion traits

These are traits that are used to convert automatically between types.

1. [`From`]
2. [`Into`]
3. [`FromStr`]
4. [`TryInto`]
5. [`IntoIterator`]
6. [`AsRef`]
7. [`AsMut`]

### Formatting traits

These traits are used for converting a struct to a string in different ways.

1. [`Display`-like], contains `Display`, `Binary`, `Octal`, `LowerHex`,
   `UpperHex`, `LowerExp`, `UpperExp`, `Pointer`

### Error-handling traits
These traits are used to define error-types.

1. [`Error`]

### Operators

These are traits that can be used for operator overloading.

1. [`Index`]
2. [`Deref`]
3. [`Not`-like], contains `Not` and `Neg`
4. [`Add`-like], contains `Add`, `Sub`, `BitAnd`, `BitOr`, `BitXor`, `MulSelf`,
   `DivSelf`, `RemSelf`, `ShrSelf` and `ShlSelf`
5. [`Mul`-like], contains `Mul`, `Div`, `Rem`, `Shr` and `Shl`
3. [`Sum`-like], contains `Sum` and `Product`
6. [`IndexMut`]
7. [`DerefMut`]
8. [`AddAssign`-like], contains `AddAssign`, `SubAssign`, `BitAndAssign`,
   `BitOrAssign` and `BitXorAssign`
9. [`MulAssign`-like], contains `MulAssign`, `DivAssign`, `RemAssign`,
   `ShrAssign` and `ShlAssign`

### Static methods

These don't derive traits, but derive static methods instead.

1. [`Constructor`], this derives a `new` method that can be used as a constructor.
   This is very basic if you need more customization for your constructor, check
   out the [`derive-new`] crate.

## Generated code

## Installation

This library requires Rust 1.36 or higher and it supports `no_std` out of the box.
Then add the following to `Cargo.toml`:

```toml
[dependencies]
derive_more = "0.99.0"
# You can specifiy the types of derives that you need for less time spent
# compiling. For the full list of features see this crate its Cargo.toml.
default-features = false
features = ["from", "add", "iterator"]
```

And this to the top of your Rust file for Rust 2018:

```rust
extern crate derive_more;
// use the derives that you want in the file
use derive_more::{Add, Display, From};
```
If you're still using Rust 2015 you should add this instead:
```rust
extern crate core;
#[macro_use]
extern crate derive_more;
```

[`cargo-expand`]: https://github.com/dtolnay/cargo-expand
[`derive-new`]: https://github.com/nrc/derive-new

[`From`]: https://jeltef.github.io/derive_more/derive_more/from.html
[`Into`]: https://jeltef.github.io/derive_more/derive_more/into.html
[`FromStr`]: https://jeltef.github.io/derive_more/derive_more/from_str.html
[`TryInto`]: https://jeltef.github.io/derive_more/derive_more/try_into.html
[`IntoIterator`]: https://jeltef.github.io/derive_more/derive_more/into_iterator.html
[`AsRef`]: https://jeltef.github.io/derive_more/derive_more/as_ref.html
[`AsMut`]: https://jeltef.github.io/derive_more/derive_more/as_mut.html

[`Display`-like]: https://jeltef.github.io/derive_more/derive_more/display.html

[`Error`]: https://jeltef.github.io/derive_more/derive_more/error.html

[`Index`]: https://jeltef.github.io/derive_more/derive_more/index_op.html
[`Deref`]: https://jeltef.github.io/derive_more/derive_more/deref.html
[`Not`-like]: https://jeltef.github.io/derive_more/derive_more/not.html
[`Add`-like]: https://jeltef.github.io/derive_more/derive_more/add.html
[`Mul`-like]: https://jeltef.github.io/derive_more/derive_more/mul.html
[`Sum`-like]: https://jeltef.github.io/derive_more/derive_more/sum.html
[`IndexMut`]: https://jeltef.github.io/derive_more/derive_more/index_mut.html
[`DerefMut`]: https://jeltef.github.io/derive_more/derive_more/deref_mut.html
[`AddAssign`-like]: https://jeltef.github.io/derive_more/derive_more/add_assign.html
[`MulAssign`-like]: https://jeltef.github.io/derive_more/derive_more/mul_assign.html

[`Constructor`]: https://jeltef.github.io/derive_more/derive_more/constructor.html
