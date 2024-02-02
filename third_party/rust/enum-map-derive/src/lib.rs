// SPDX-FileCopyrightText: 2017 - 2022 Kamila Borowska <kamila@borowska.pw>
// SPDX-FileCopyrightText: 2018 hcpl <hcpl.prog@gmail.com>
// SPDX-FileCopyrightText: 2019 mara <vmedea@protonmail.com>
// SPDX-FileCopyrightText: 2021 Bruno Corrêa Zimmermann <brunoczim@gmail.com>
// SPDX-FileCopyrightText: 2021 Dietrich <dietrich@teilgedanken.de>
//
// SPDX-License-Identifier: MIT OR Apache-2.0

//! Procedural macro implementing `#[derive(Enum)]`
//!
//! This is supposed to used with `enum-map` crate, which provides the
//! actual usage documentation.

mod derive_enum;
mod derive_struct;

use proc_macro2::TokenStream;
use quote::quote;
use syn::{Data, DeriveInput, Type};

/// Derive macro generating an implementation of trait `Enum`.
///
/// When using a derive, enum maps are maintained in the order in which
/// enum variants are declared. This is reflected in the value returned
/// by `Enum::into_usize`, iterators of enum map as well as
/// `EnumMap::as_slice` method.
///
/// # Examples
///
/// ## Enums Without Payload
/// ```
/// use enum_map::Enum;
///
/// #[derive(Enum, Debug, PartialEq, Eq)]
/// enum A {
///     B,
///     C,
///     D,
/// }
///
/// assert_eq!(A::B.into_usize(), 0);
/// assert_eq!(A::C.into_usize(), 1);
/// assert_eq!(A::D.into_usize(), 2);
///
/// assert_eq!(A::from_usize(0), A::B);
/// assert_eq!(A::from_usize(1), A::C);
/// assert_eq!(A::from_usize(2), A::D);
/// ```
///
/// ## Enums With Payload
///
/// ```
/// use enum_map::Enum;
///
/// #[derive(Enum, Debug, PartialEq, Eq)]
/// enum A {
///     B,
///     C,
///     D,
/// }
///
/// #[derive(Enum, Debug, PartialEq, Eq)]
/// enum X {
///     Y,
///     Z,
/// }
///
/// #[derive(Enum, Debug, PartialEq, Eq)]
/// enum Foo {
///     Bar(bool, A),
///     Empty,
///     Baz { fa: A, fx: X },
/// }
///
/// assert_eq!(Foo::Bar(false, A::B).into_usize(), 0);
/// assert_eq!(Foo::Bar(false, A::D).into_usize(), 4);
/// assert_eq!(Foo::Bar(true, A::B).into_usize(), 1);
/// assert_eq!(Foo::Bar(true, A::C).into_usize(), 3);
/// assert_eq!(Foo::Empty.into_usize(), 6);
/// assert_eq!(Foo::Baz { fa: A::B, fx: X::Y }.into_usize(), 7);
/// assert_eq!(Foo::Baz { fa: A::B, fx: X::Z }.into_usize(), 10);
/// assert_eq!(Foo::Baz { fa: A::D, fx: X::Y }.into_usize(), 9);
///
/// assert_eq!(Foo::from_usize(0), Foo::Bar(false, A::B));
/// assert_eq!(Foo::from_usize(4), Foo::Bar(false, A::D));
/// assert_eq!(Foo::from_usize(1), Foo::Bar(true, A::B));
/// assert_eq!(Foo::from_usize(3), Foo::Bar(true, A::C));
/// assert_eq!(Foo::from_usize(6), Foo::Empty);
/// assert_eq!(Foo::from_usize(7), Foo::Baz { fa: A::B, fx: X::Y });
/// assert_eq!(Foo::from_usize(10), Foo::Baz { fa: A::B, fx: X::Z });
/// assert_eq!(Foo::from_usize(9), Foo::Baz { fa: A::D, fx: X::Y });
///
/// ```
///
/// ## Structs
///
/// ```
/// use enum_map::Enum;
///
/// #[derive(Enum, Debug, PartialEq, Eq)]
/// enum A {
///     B,
///     C,
///     D,
/// }
///
/// #[derive(Enum, Debug, PartialEq, Eq)]
/// enum X {
///     Y,
///     Z,
/// }
///
/// #[derive(Enum, Debug, PartialEq, Eq)]
/// struct Foo {
///     bar: bool,
///     baz: A,
///     end: X,
/// }
///
/// assert_eq!(Foo { bar: false, baz: A::B, end: X::Y }.into_usize(), 0);
/// assert_eq!(Foo { bar: true, baz: A::B, end: X::Y }.into_usize(), 1);
/// assert_eq!(Foo { bar: false, baz: A::D, end: X::Y }.into_usize(), 4);
/// assert_eq!(Foo { bar: true, baz: A::C, end: X::Z }.into_usize(), 9);
///
/// assert_eq!(Foo::from_usize(0), Foo { bar: false, baz: A::B, end: X::Y });
/// assert_eq!(Foo::from_usize(1), Foo { bar: true, baz: A::B, end: X::Y });
/// assert_eq!(Foo::from_usize(4), Foo { bar: false, baz: A::D, end: X::Y });
/// assert_eq!(Foo::from_usize(9), Foo { bar: true, baz: A::C, end: X::Z });
/// ```
///
/// ## Tuple Structs
///
/// ```
/// use enum_map::Enum;
///
/// #[derive(Enum, Debug, PartialEq, Eq)]
/// enum A {
///     B,
///     C,
///     D,
/// }
///
/// #[derive(Enum, Debug, PartialEq, Eq)]
/// enum X {
///     Y,
///     Z,
/// }
///
/// #[derive(Enum, Debug, PartialEq, Eq)]
/// struct Foo(bool, A, X);
///
/// assert_eq!(Foo(false, A::B, X::Y ).into_usize(), 0);
/// assert_eq!(Foo(true, A::B, X::Y ).into_usize(), 1);
/// assert_eq!(Foo(false, A::D, X::Y ).into_usize(), 4);
/// assert_eq!(Foo(true, A::C, X::Z ).into_usize(), 9);
///
/// assert_eq!(Foo::from_usize(0), Foo(false, A::B, X::Y));
/// assert_eq!(Foo::from_usize(1), Foo(true, A::B, X::Y));
/// assert_eq!(Foo::from_usize(4), Foo(false, A::D, X::Y));
/// assert_eq!(Foo::from_usize(9), Foo(true, A::C, X::Z));
#[proc_macro_derive(Enum)]
pub fn derive_enum_map(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    let input: DeriveInput = syn::parse(input).unwrap();

    let result = match input.data {
        Data::Enum(data_enum) => derive_enum::generate(input.ident, data_enum),
        Data::Struct(data_struct) => derive_struct::generate(input.ident, data_struct),
        _ => quote! { compile_error! {"#[derive(Enum)] is only defined for enums and structs"} },
    };

    result.into()
}

fn type_length(ty: &Type) -> TokenStream {
    quote! {
        <#ty as ::enum_map::Enum>::LENGTH
    }
}
