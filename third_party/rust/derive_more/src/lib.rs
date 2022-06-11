//! # `derive_more`
//!
//! [![Build Status](https://github.com/JelteF/derive_more/workflows/CI/badge.svg)](https://github.com/JelteF/derive_more/actions)
//! [![Latest Version](https://img.shields.io/crates/v/derive_more.svg)](https://crates.io/crates/derive_more)
//! [![Rust Documentation](https://img.shields.io/badge/api-rustdoc-blue.svg)](https://jeltef.github.io/derive_more/derive_more/)
//! [![GitHub license](https://img.shields.io/badge/license-MIT-blue.svg)](https://raw.githubusercontent.com/JelteF/derive_more/master/LICENSE)
//! [![Rust 1.36+](https://img.shields.io/badge/rustc-1.36+-lightgray.svg)](https://blog.rust-lang.org/2019/07/04/Rust-1.36.0.html)
//!
//! Rust has lots of builtin traits that are implemented for its basic types, such
//! as `Add`, `Not`, `From` or `Display`.
//! However, when wrapping these types inside your own structs or enums you lose the
//! implementations of these traits and are required to recreate them.
//! This is especially annoying when your own structures are very simple, such as
//! when using the commonly advised newtype pattern (e.g. `MyInt(i32)`).
//!
//! This library tries to remove these annoyances and the corresponding boilerplate code.
//! It does this by allowing you to derive lots of commonly used traits for both structs and enums.
//!
//! ## Example code
//!
//! By using this library the following code just works:
//!
//! ```rust
//! extern crate derive_more;
//! use derive_more::{Add, Display, From, Into};
//!
//! #[derive(PartialEq, From, Add)]
//! struct MyInt(i32);
//!
//! #[derive(PartialEq, From, Into)]
//! struct Point2D {
//!     x: i32,
//!     y: i32,
//! }
//!
//! #[derive(PartialEq, From, Add, Display)]
//! enum MyEnum {
//!     #[display(fmt = "int: {}", _0)]
//!     Int(i32),
//!     Uint(u32),
//!     #[display(fmt = "nothing")]
//!     Nothing,
//! }
//!
//! assert!(MyInt(11) == MyInt(5) + 6.into());
//! assert!((5, 6) == Point2D { x: 5, y: 6 }.into());
//! assert!(MyEnum::Int(15) == (MyEnum::Int(8) + 7.into()).unwrap());
//! assert!(MyEnum::Int(15).to_string() == "int: 15");
//! assert!(MyEnum::Uint(42).to_string() == "42");
//! assert!(MyEnum::Nothing.to_string() == "nothing");
//! ```
//!
//! ## The derivable traits
//!
//! Below are all the traits that you can derive using this library.
//! Some trait derivations are so similar that the further documentation will only show a single one
//! of them.
//! You can recognize these by the "-like" suffix in their name.
//! The trait name before that will be the only one that is used throughout the further
//! documentation.
//!
//! It is important to understand what code gets generated when using one of the
//! derives from this crate.
//! That is why the links below explain what code gets generated for a trait for
//! each group from before.
//!
//! You can use the [`cargo-expand`] utility to see the exact code that is generated
//! for your specific type.
//! This will show you your code with all macros and derives expanded.
//!
//! **NOTE**: You still have to derive each trait separately. So `#[derive(Mul)]` doesn't
//! automatically derive `Div` as well. To derive both you should do `#[derive(Mul, Div)]`
//!
//! ### Conversion traits
//!
//! These are traits that are used to convert automatically between types.
//!
//! 1. [`From`]
//! 2. [`Into`]
//! 3. [`FromStr`]
//! 4. [`TryInto`]
//! 5. [`IntoIterator`]
//! 6. [`AsRef`]
//! 7. [`AsMut`]
//!
//! ### Formatting traits
//!
//! These traits are used for converting a struct to a string in different ways.
//!
//! 1. [`Display`-like], contains `Display`, `Binary`, `Octal`, `LowerHex`,
//!    `UpperHex`, `LowerExp`, `UpperExp`, `Pointer`
//!
//! ### Error-handling traits
//! These traits are used to define error-types.
//!
//! 1. [`Error`]
//!
//! ### Operators
//!
//! These are traits that can be used for operator overloading.
//!
//! 1. [`Index`]
//! 2. [`Deref`]
//! 3. [`Not`-like], contains `Not` and `Neg`
//! 4. [`Add`-like], contains `Add`, `Sub`, `BitAnd`, `BitOr`, `BitXor`
//! 5. [`Mul`-like], contains `Mul`, `Div`, `Rem`, `Shr` and `Shl`
//! 3. [`Sum`-like], contains `Sum` and `Product`
//! 6. [`IndexMut`]
//! 7. [`DerefMut`]
//! 8. [`AddAssign`-like], contains `AddAssign`, `SubAssign`, `BitAndAssign`,
//!    `BitOrAssign` and `BitXorAssign`
//! 9. [`MulAssign`-like], contains `MulAssign`, `DivAssign`, `RemAssign`,
//!    `ShrAssign` and `ShlAssign`
//!
//! ### Static methods
//!
//! These don't derive traits, but derive static methods instead.
//!
//! 1. [`Constructor`], this derives a `new` method that can be used as a constructor.
//!    This is very basic if you need more customization for your constructor, check
//!    out the [`derive-new`] crate.
//! 2. [`IsVariant`], for each variant `foo` of an enum type, derives a `is_foo` method.
//! 3. [`Unwrap`], for each variant `foo` of an enum type, derives an `unwrap_foo` method.
//!
//! ## Generated code
//!
//! ## Installation
//!
//! This library requires Rust 1.36 or higher and it supports `no_std` out of the box.
//! Then add the following to `Cargo.toml`:
//!
//! ```toml
//! [dependencies]
//! derive_more = "0.99.0"
//! # You can specifiy the types of derives that you need for less time spent
//! # compiling. For the full list of features see this crate its Cargo.toml.
//! default-features = false
//! features = ["from", "add", "iterator"]
//! ```
//!
//! And this to the top of your Rust file for Rust 2018:
//!
//! ```rust
//! extern crate derive_more;
//! // use the derives that you want in the file
//! use derive_more::{Add, Display, From};
//! ```
//! If you're still using Rust 2015 you should add this instead:
//! ```rust
//! extern crate core;
//! #[macro_use]
//! extern crate derive_more;
//! # fn main(){}
//! ```
//!
//! [`cargo-expand`]: https://github.com/dtolnay/cargo-expand
//! [`derive-new`]: https://github.com/nrc/derive-new
//!
//! [`From`]: https://jeltef.github.io/derive_more/derive_more/from.html
//! [`Into`]: https://jeltef.github.io/derive_more/derive_more/into.html
//! [`FromStr`]: https://jeltef.github.io/derive_more/derive_more/from_str.html
//! [`TryInto`]: https://jeltef.github.io/derive_more/derive_more/try_into.html
//! [`IntoIterator`]: https://jeltef.github.io/derive_more/derive_more/into_iterator.html
//! [`AsRef`]: https://jeltef.github.io/derive_more/derive_more/as_ref.html
//! [`AsMut`]: https://jeltef.github.io/derive_more/derive_more/as_mut.html
//!
//! [`Display`-like]: https://jeltef.github.io/derive_more/derive_more/display.html
//!
//! [`Error`]: https://jeltef.github.io/derive_more/derive_more/error.html
//!
//! [`Index`]: https://jeltef.github.io/derive_more/derive_more/index_op.html
//! [`Deref`]: https://jeltef.github.io/derive_more/derive_more/deref.html
//! [`Not`-like]: https://jeltef.github.io/derive_more/derive_more/not.html
//! [`Add`-like]: https://jeltef.github.io/derive_more/derive_more/add.html
//! [`Mul`-like]: https://jeltef.github.io/derive_more/derive_more/mul.html
//! [`Sum`-like]: https://jeltef.github.io/derive_more/derive_more/sum.html
//! [`IndexMut`]: https://jeltef.github.io/derive_more/derive_more/index_mut.html
//! [`DerefMut`]: https://jeltef.github.io/derive_more/derive_more/deref_mut.html
//! [`AddAssign`-like]: https://jeltef.github.io/derive_more/derive_more/add_assign.html
//! [`MulAssign`-like]: https://jeltef.github.io/derive_more/derive_more/mul_assign.html
//!
//! [`Constructor`]: https://jeltef.github.io/derive_more/derive_more/constructor.html
//! [`IsVariant`]: https://jeltef.github.io/derive_more/derive_more/is_variant.html
//! [`Unwrap`]: https://jeltef.github.io/derive_more/derive_more/unwrap.html

#![recursion_limit = "128"]

extern crate proc_macro;

use proc_macro::TokenStream;
use syn::parse::Error as ParseError;

mod utils;

#[cfg(any(feature = "add_assign", feature = "mul_assign"))]
mod add_assign_like;
#[cfg(any(
    feature = "add",
    feature = "add_assign",
    feature = "mul",
    feature = "mul_assign",
))]
mod add_helpers;
#[cfg(any(feature = "add", feature = "mul"))]
mod add_like;
#[cfg(feature = "as_mut")]
mod as_mut;
#[cfg(feature = "as_ref")]
mod as_ref;
#[cfg(feature = "constructor")]
mod constructor;
#[cfg(feature = "deref")]
mod deref;
#[cfg(feature = "deref_mut")]
mod deref_mut;
#[cfg(feature = "display")]
mod display;
#[cfg(feature = "error")]
mod error;
#[cfg(feature = "from")]
mod from;
#[cfg(feature = "from_str")]
mod from_str;
#[cfg(feature = "index")]
mod index;
#[cfg(feature = "index_mut")]
mod index_mut;
#[cfg(feature = "into")]
mod into;
#[cfg(feature = "into_iterator")]
mod into_iterator;
#[cfg(feature = "is_variant")]
mod is_variant;
#[cfg(feature = "mul_assign")]
mod mul_assign_like;
#[cfg(any(feature = "mul", feature = "mul_assign"))]
mod mul_helpers;
#[cfg(feature = "mul")]
mod mul_like;
#[cfg(feature = "not")]
mod not_like;
#[cfg(feature = "display")]
#[allow(ellipsis_inclusive_range_patterns)]
#[allow(clippy::all)]
mod parsing;
#[cfg(feature = "sum")]
mod sum_like;
#[cfg(feature = "try_into")]
mod try_into;
#[cfg(feature = "unwrap")]
mod unwrap;

// This trait describes the possible return types of
// the derives. A derive can generally be infallible and
// return a TokenStream, or it can be fallible and return
// a Result<TokenStream, syn::parse::Error>.
trait Output {
    fn process(self) -> TokenStream;
}

impl Output for proc_macro2::TokenStream {
    fn process(self) -> TokenStream {
        self.into()
    }
}

impl Output for Result<proc_macro2::TokenStream, ParseError> {
    fn process(self) -> TokenStream {
        match self {
            Ok(ts) => ts.into(),
            Err(e) => e.to_compile_error().into(),
        }
    }
}

macro_rules! create_derive(
    ($feature:literal, $mod_:ident, $trait_:ident, $fn_name: ident $(,$attribute:ident)* $(,)?) => {
        #[cfg(feature = $feature)]
        #[proc_macro_derive($trait_, attributes($($attribute),*))]
        #[doc(hidden)]
        pub fn $fn_name(input: TokenStream) -> TokenStream {
            let ast = syn::parse(input).unwrap();
            Output::process($mod_::expand(&ast, stringify!($trait_)))
        }
    }
);

create_derive!("from", from, From, from_derive, from);

create_derive!("into", into, Into, into_derive, into);

create_derive!("constructor", constructor, Constructor, constructor_derive);

create_derive!("not", not_like, Not, not_derive);
create_derive!("not", not_like, Neg, neg_derive);

create_derive!("add", add_like, Add, add_derive);
create_derive!("add", add_like, Sub, sub_derive);
create_derive!("add", add_like, BitAnd, bit_and_derive);
create_derive!("add", add_like, BitOr, bit_or_derive);
create_derive!("add", add_like, BitXor, bit_xor_derive);

create_derive!("mul", mul_like, Mul, mul_derive, mul);
create_derive!("mul", mul_like, Div, div_derive, div);
create_derive!("mul", mul_like, Rem, rem_derive, rem);
create_derive!("mul", mul_like, Shr, shr_derive, shr);
create_derive!("mul", mul_like, Shl, shl_derive, shl);

create_derive!("add_assign", add_assign_like, AddAssign, add_assign_derive,);
create_derive!("add_assign", add_assign_like, SubAssign, sub_assign_derive,);
create_derive!(
    "add_assign",
    add_assign_like,
    BitAndAssign,
    bit_and_assign_derive,
);
create_derive!(
    "add_assign",
    add_assign_like,
    BitOrAssign,
    bit_or_assign_derive,
);
create_derive!(
    "add_assign",
    add_assign_like,
    BitXorAssign,
    bit_xor_assign_derive,
);

create_derive!(
    "mul_assign",
    mul_assign_like,
    MulAssign,
    mul_assign_derive,
    mul_assign,
);
create_derive!(
    "mul_assign",
    mul_assign_like,
    DivAssign,
    div_assign_derive,
    div_assign,
);
create_derive!(
    "mul_assign",
    mul_assign_like,
    RemAssign,
    rem_assign_derive,
    rem_assign,
);
create_derive!(
    "mul_assign",
    mul_assign_like,
    ShrAssign,
    shr_assign_derive,
    shr_assign,
);
create_derive!(
    "mul_assign",
    mul_assign_like,
    ShlAssign,
    shl_assign_derive,
    shl_assign,
);

create_derive!("sum", sum_like, Sum, sum_derive);
create_derive!("sum", sum_like, Product, product_derive);

create_derive!("error", error, Error, error_derive, error);

create_derive!("from_str", from_str, FromStr, from_str_derive);

create_derive!("display", display, Display, display_derive, display);
create_derive!("display", display, Binary, binary_derive, binary);
create_derive!("display", display, Octal, octal_derive, octal);
create_derive!("display", display, LowerHex, lower_hex_derive, lower_hex);
create_derive!("display", display, UpperHex, upper_hex_derive, upper_hex);
create_derive!("display", display, LowerExp, lower_exp_derive, lower_exp);
create_derive!("display", display, UpperExp, upper_exp_derive, upper_exp);
create_derive!("display", display, Pointer, pointer_derive, pointer);
create_derive!("display", display, DebugCustom, debug_custom_derive, debug);

create_derive!("index", index, Index, index_derive, index);
create_derive!(
    "index_mut",
    index_mut,
    IndexMut,
    index_mut_derive,
    index_mut,
);

create_derive!(
    "into_iterator",
    into_iterator,
    IntoIterator,
    into_iterator_derive,
    into_iterator,
);

create_derive!("try_into", try_into, TryInto, try_into_derive, try_into);

create_derive!("deref", deref, Deref, deref_derive, deref);
create_derive!(
    "deref_mut",
    deref_mut,
    DerefMut,
    deref_mut_derive,
    deref_mut,
);

create_derive!("as_ref", as_ref, AsRef, as_ref_derive, as_ref);
create_derive!("as_mut", as_mut, AsMut, as_mut_derive, as_mut);

create_derive!(
    "is_variant",
    is_variant,
    IsVariant,
    is_variant_derive,
    is_variant
);

create_derive!("unwrap", unwrap, Unwrap, unwrap_derive, unwrap);
