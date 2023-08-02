# `derive_more`

[![Build Status](https://github.com/JelteF/derive_more/workflows/CI/badge.svg)](https://github.com/JelteF/derive_more/actions)
[![Latest Version](https://img.shields.io/crates/v/derive_more.svg)](https://crates.io/crates/derive_more)
[![Rust Documentation](https://docs.rs/derive_more/badge.svg)](https://docs.rs/derive_more)
[![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/JelteF/derive_more/master/LICENSE)
[![Rust 1.65+](https://img.shields.io/badge/rustc-1.65+-lightgray.svg)](https://blog.rust-lang.org/2021/10/21/Rust-1.65.0.html)
[![Unsafe Forbidden](https://img.shields.io/badge/unsafe-forbidden-success.svg)](https://github.com/rust-secure-code/safety-dance)

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
    #[display("int: {_0}")]
    Int(i32),
    Uint(u32),
    #[display("nothing")]
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

1. [`Debug`]
2. [`Display`-like], contains `Display`, `Binary`, `Octal`, `LowerHex`,
   `UpperHex`, `LowerExp`, `UpperExp`, `Pointer`


### Error-handling traits

These traits are used to define error-types.

1. [`Error`]


### Operators

These are traits that can be used for operator overloading.

1. [`Index`]
2. [`Deref`]
3. [`Not`-like], contains `Not` and `Neg`
4. [`Add`-like], contains `Add`, `Sub`, `BitAnd`, `BitOr`, `BitXor`
5. [`Mul`-like], contains `Mul`, `Div`, `Rem`, `Shr` and `Shl`
6. [`Sum`-like], contains `Sum` and `Product`
7. [`IndexMut`]
8. [`DerefMut`]
9. [`AddAssign`-like], contains `AddAssign`, `SubAssign`, `BitAndAssign`,
   `BitOrAssign` and `BitXorAssign`
10. [`MulAssign`-like], contains `MulAssign`, `DivAssign`, `RemAssign`,
    `ShrAssign` and `ShlAssign`


### Static methods

These don't derive traits, but derive static methods instead.

1. [`Constructor`], this derives a `new` method that can be used as a constructor.
   This is very basic if you need more customization for your constructor, check
   out the [`derive-new`] crate.
2. [`IsVariant`], for each variant `foo` of an enum type, derives a `is_foo` method.
3. [`Unwrap`], for each variant `foo` of an enum type, derives an `unwrap_foo` method.
4. [`TryUnwrap`], for each variant `foo` of an enum type, derives an `try_unwrap_foo` method.




## Installation

This library requires Rust 1.65 or higher. To avoid redundant compilation times, by
default no derives are supported. You have to enable each type of derive as a feature
in `Cargo.toml`:

```toml
[dependencies]
derive_more = "0.99.0"
# You can specify the types of derives that you need for less time spent
# compiling. For the full list of features see this crate its `Cargo.toml`.
features = ["from", "add", "iterator"]

# If you don't care much about compilation times and simply want to have
# support for all the possible derives, you can use the "full" feature.
features = ["full"]

# If you run in a `no_std` environment you should disable the default features,
# because the only default feature is the "std" feature.
# NOTE: You can combine this with "full" feature to get support for all the
#       possible derives in a `no_std` environment.
default-features = false
```

And this to the top of your Rust file:
```rust
// use the derives that you want in the file
use derive_more::{Add, Display, From};
```
If you're still using Rust 2015, add this instead:
```rust,edition2015
extern crate core;
#[macro_use]
extern crate derive_more;
# fn main() {} // omit wrapping statements above into `main()` in tests
```

## Re-exports

This crate also re-exports all of the standard library traits that it adds
derives for. So both the `Display` derive and the `Display` trait will be in
scope when you add the following code:
```rust
use derive_more::Display;
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

[`Debug`]: https://github.com/JelteF/derive_more/blob/master/impl/doc/debug.md
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
[`IsVariant`]: https://jeltef.github.io/derive_more/derive_more/is_variant.html
[`Unwrap`]: https://jeltef.github.io/derive_more/derive_more/unwrap.html
[`TryUnwrap`]: https://jeltef.github.io/derive_more/derive_more/try_unwrap.html
