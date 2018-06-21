// Copyright 2018 Syn Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use buffer::Cursor;
use parse_error;
use synom::PResult;

/// Define a parser function with the signature expected by syn parser
/// combinators.
///
/// The function may be the `parse` function of the [`Synom`] trait, or it may
/// be a free-standing function with an arbitrary name. When implementing the
/// `Synom` trait, the function name is `parse` and the return type is `Self`.
///
/// [`Synom`]: synom/trait.Synom.html
///
/// - **Syntax:** `named!(NAME -> TYPE, PARSER)` or `named!(pub NAME -> TYPE, PARSER)`
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::Type;
/// use syn::punctuated::Punctuated;
/// use syn::synom::Synom;
///
/// /// Parses one or more Rust types separated by commas.
/// ///
/// /// Example: `String, Vec<T>, [u8; LEN + 1]`
/// named!(pub comma_separated_types -> Punctuated<Type, Token![,]>,
///     call!(Punctuated::parse_separated_nonempty)
/// );
///
/// /// The same function as a `Synom` implementation.
/// struct CommaSeparatedTypes {
///     types: Punctuated<Type, Token![,]>,
/// }
///
/// impl Synom for CommaSeparatedTypes {
///     /// As the default behavior, we want there to be at least 1 type.
///     named!(parse -> Self, do_parse!(
///         types: call!(Punctuated::parse_separated_nonempty) >>
///         (CommaSeparatedTypes { types })
///     ));
/// }
///
/// impl CommaSeparatedTypes {
///     /// A separate parser that the user can invoke explicitly which allows
///     /// for parsing 0 or more types, rather than the default 1 or more.
///     named!(pub parse0 -> Self, do_parse!(
///         types: call!(Punctuated::parse_separated) >>
///         (CommaSeparatedTypes { types })
///     ));
/// }
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! named {
    ($name:ident -> $o:ty, $submac:ident!( $($args:tt)* )) => {
        fn $name(i: $crate::buffer::Cursor) -> $crate::synom::PResult<$o> {
            $submac!(i, $($args)*)
        }
    };

    (pub $name:ident -> $o:ty, $submac:ident!( $($args:tt)* )) => {
        pub fn $name(i: $crate::buffer::Cursor) -> $crate::synom::PResult<$o> {
            $submac!(i, $($args)*)
        }
    };

    // These two variants are for defining named parsers which have custom
    // arguments, and are called with `call!()`
    ($name:ident($($params:tt)*) -> $o:ty, $submac:ident!( $($args:tt)* )) => {
        fn $name(i: $crate::buffer::Cursor, $($params)*) -> $crate::synom::PResult<$o> {
            $submac!(i, $($args)*)
        }
    };

    (pub $name:ident($($params:tt)*) -> $o:ty, $submac:ident!( $($args:tt)* )) => {
        pub fn $name(i: $crate::buffer::Cursor, $($params)*) -> $crate::synom::PResult<$o> {
            $submac!(i, $($args)*)
        }
    };
}

#[cfg(synom_verbose_trace)]
#[macro_export]
macro_rules! call {
    ($i:expr, $fun:expr $(, $args:expr)*) => {{
        let i = $i;
        eprintln!(concat!(" -> ", stringify!($fun), " @ {:?}"), i);
        let r = $fun(i $(, $args)*);
        match r {
            Ok((_, i)) => eprintln!(concat!("OK  ", stringify!($fun), " @ {:?}"), i),
            Err(_) => eprintln!(concat!("ERR ", stringify!($fun), " @ {:?}"), i),
        }
        r
    }};
}

/// Invoke the given parser function with zero or more arguments.
///
/// - **Syntax:** `call!(FN, ARGS...)`
///
///   where the signature of the function is `fn(Cursor, ARGS...) -> PResult<T>`
///
/// - **Output:** `T`, the result of invoking the function `FN`
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::Type;
/// use syn::punctuated::Punctuated;
/// use syn::synom::Synom;
///
/// /// Parses one or more Rust types separated by commas.
/// ///
/// /// Example: `String, Vec<T>, [u8; LEN + 1]`
/// struct CommaSeparatedTypes {
///     types: Punctuated<Type, Token![,]>,
/// }
///
/// impl Synom for CommaSeparatedTypes {
///     named!(parse -> Self, do_parse!(
///         types: call!(Punctuated::parse_separated_nonempty) >>
///         (CommaSeparatedTypes { types })
///     ));
/// }
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[cfg(not(synom_verbose_trace))]
#[macro_export]
macro_rules! call {
    ($i:expr, $fun:expr $(, $args:expr)*) => {
        $fun($i $(, $args)*)
    };
}

/// Transform the result of a parser by applying a function or closure.
///
/// - **Syntax:** `map!(THING, FN)`
/// - **Output:** the return type of function FN applied to THING
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::{Expr, ExprIf};
///
/// /// Extracts the branch condition of an `if`-expression.
/// fn get_cond(if_: ExprIf) -> Expr {
///     *if_.cond
/// }
///
/// /// Parses a full `if`-expression but returns the condition part only.
/// ///
/// /// Example: `if x > 0xFF { "big" } else { "small" }`
/// /// The return would be the expression `x > 0xFF`.
/// named!(if_condition -> Expr,
///     map!(syn!(ExprIf), get_cond)
/// );
///
/// /// Equivalent using a closure.
/// named!(if_condition2 -> Expr,
///     map!(syn!(ExprIf), |if_| *if_.cond)
/// );
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! map {
    ($i:expr, $submac:ident!( $($args:tt)* ), $g:expr) => {
        match $submac!($i, $($args)*) {
            ::std::result::Result::Err(err) =>
                ::std::result::Result::Err(err),
            ::std::result::Result::Ok((o, i)) =>
                ::std::result::Result::Ok(($crate::parsers::invoke($g, o), i)),
        }
    };

    ($i:expr, $f:expr, $g:expr) => {
        map!($i, call!($f), $g)
    };
}

// Somehow this helps with type inference in `map!` and `alt!`.
//
// Not public API.
#[doc(hidden)]
pub fn invoke<T, R, F: FnOnce(T) -> R>(f: F, t: T) -> R {
    f(t)
}

/// Invert the result of a parser by parsing successfully if the given parser
/// fails to parse and vice versa.
///
/// Does not consume any of the input.
///
/// - **Syntax:** `not!(THING)`
/// - **Output:** `()`
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::{Expr, Ident};
///
/// /// Parses any expression that does not begin with a `-` minus sign.
/// named!(not_negative_expr -> Expr, do_parse!(
///     not!(punct!(-)) >>
///     e: syn!(Expr) >>
///     (e)
/// ));
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! not {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {
        match $submac!($i, $($args)*) {
            ::std::result::Result::Ok(_) => $crate::parse_error(),
            ::std::result::Result::Err(_) =>
                ::std::result::Result::Ok(((), $i)),
        }
    };
}

/// Execute a parser only if a condition is met, otherwise return None.
///
/// If you are familiar with nom, this is nom's `cond_with_error` parser.
///
/// - **Syntax:** `cond!(CONDITION, THING)`
/// - **Output:** `Some(THING)` if the condition is true, else `None`
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::{Ident, MacroDelimiter};
/// use syn::token::{Paren, Bracket, Brace};
/// use syn::synom::Synom;
///
/// /// Parses a macro call with empty input. If the macro is written with
/// /// parentheses or brackets, a trailing semicolon is required.
/// ///
/// /// Example: `my_macro!{}` or `my_macro!();` or `my_macro![];`
/// struct EmptyMacroCall {
///     name: Ident,
///     bang_token: Token![!],
///     empty_body: MacroDelimiter,
///     semi_token: Option<Token![;]>,
/// }
///
/// fn requires_semi(delimiter: &MacroDelimiter) -> bool {
///     match *delimiter {
///         MacroDelimiter::Paren(_) | MacroDelimiter::Bracket(_) => true,
///         MacroDelimiter::Brace(_) => false,
///     }
/// }
///
/// impl Synom for EmptyMacroCall {
///     named!(parse -> Self, do_parse!(
///         name: syn!(Ident) >>
///         bang_token: punct!(!) >>
///         empty_body: alt!(
///             parens!(epsilon!()) => { |d| MacroDelimiter::Paren(d.0) }
///             |
///             brackets!(epsilon!()) => { |d| MacroDelimiter::Bracket(d.0) }
///             |
///             braces!(epsilon!()) => { |d| MacroDelimiter::Brace(d.0) }
///         ) >>
///         semi_token: cond!(requires_semi(&empty_body), punct!(;)) >>
///         (EmptyMacroCall {
///             name,
///             bang_token,
///             empty_body,
///             semi_token,
///         })
///     ));
/// }
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! cond {
    ($i:expr, $cond:expr, $submac:ident!( $($args:tt)* )) => {
        if $cond {
            match $submac!($i, $($args)*) {
                ::std::result::Result::Ok((o, i)) =>
                    ::std::result::Result::Ok((::std::option::Option::Some(o), i)),
                ::std::result::Result::Err(x) => ::std::result::Result::Err(x),
            }
        } else {
            ::std::result::Result::Ok((::std::option::Option::None, $i))
        }
    };

    ($i:expr, $cond:expr, $f:expr) => {
        cond!($i, $cond, call!($f))
    };
}

/// Execute a parser only if a condition is met, otherwise fail to parse.
///
/// This is typically used inside of [`option!`] or [`alt!`].
///
/// [`option!`]: macro.option.html
/// [`alt!`]: macro.alt.html
///
/// - **Syntax:** `cond_reduce!(CONDITION, THING)`
/// - **Output:** `THING`
///
/// The subparser may be omitted in which case it defaults to [`epsilon!`].
///
/// [`epsilon!`]: macro.epsilon.html
///
/// - **Syntax:** `cond_reduce!(CONDITION)`
/// - **Output:** `()`
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::Type;
/// use syn::token::Paren;
/// use syn::punctuated::Punctuated;
/// use syn::synom::Synom;
///
/// /// Parses a possibly variadic function signature.
/// ///
/// /// Example: `fn(A) or `fn(A, B, C, ...)` or `fn(...)`
/// /// Rejected: `fn(A, B...)`
/// struct VariadicFn {
///     fn_token: Token![fn],
///     paren_token: Paren,
///     types: Punctuated<Type, Token![,]>,
///     variadic: Option<Token![...]>,
/// }
///
/// // Example of using `cond_reduce!` inside of `option!`.
/// impl Synom for VariadicFn {
///     named!(parse -> Self, do_parse!(
///         fn_token: keyword!(fn) >>
///         params: parens!(do_parse!(
///             types: call!(Punctuated::parse_terminated) >>
///             // Allow, but do not require, an ending `...` but only if the
///             // preceding list of types is empty or ends with a trailing comma.
///             variadic: option!(cond_reduce!(types.empty_or_trailing(), punct!(...))) >>
///             (types, variadic)
///         )) >>
///         ({
///             let (paren_token, (types, variadic)) = params;
///             VariadicFn {
///                 fn_token,
///                 paren_token,
///                 types,
///                 variadic,
///             }
///         })
///     ));
/// }
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! cond_reduce {
    ($i:expr, $cond:expr, $submac:ident!( $($args:tt)* )) => {
        if $cond {
            $submac!($i, $($args)*)
        } else {
            $crate::parse_error()
        }
    };

    ($i:expr, $cond:expr) => {
        cond_reduce!($i, $cond, epsilon!())
    };

    ($i:expr, $cond:expr, $f:expr) => {
        cond_reduce!($i, $cond, call!($f))
    };
}

/// Parse zero or more values using the given parser.
///
/// - **Syntax:** `many0!(THING)`
/// - **Output:** `Vec<THING>`
///
/// You may also be looking for:
///
/// - `call!(Punctuated::parse_separated)` - zero or more values with separator
/// - `call!(Punctuated::parse_separated_nonempty)` - one or more values
/// - `call!(Punctuated::parse_terminated)` - zero or more, allows trailing separator
/// - `call!(Punctuated::parse_terminated_nonempty)` - one or more
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::{Ident, Item};
/// use syn::token::Brace;
/// use syn::synom::Synom;
///
/// /// Parses a module containing zero or more Rust items.
/// ///
/// /// Example: `mod m { type Result<T> = ::std::result::Result<T, MyError>; }`
/// struct SimpleMod {
///     mod_token: Token![mod],
///     name: Ident,
///     brace_token: Brace,
///     items: Vec<Item>,
/// }
///
/// impl Synom for SimpleMod {
///     named!(parse -> Self, do_parse!(
///         mod_token: keyword!(mod) >>
///         name: syn!(Ident) >>
///         body: braces!(many0!(syn!(Item))) >>
///         (SimpleMod {
///             mod_token,
///             name,
///             brace_token: body.0,
///             items: body.1,
///         })
///     ));
/// }
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! many0 {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {{
        let ret;
        let mut res   = ::std::vec::Vec::new();
        let mut input = $i;

        loop {
            if input.eof() {
                ret = ::std::result::Result::Ok((res, input));
                break;
            }

            match $submac!(input, $($args)*) {
                ::std::result::Result::Err(_) => {
                    ret = ::std::result::Result::Ok((res, input));
                    break;
                }
                ::std::result::Result::Ok((o, i)) => {
                    // loop trip must always consume (otherwise infinite loops)
                    if i == input {
                        ret = $crate::parse_error();
                        break;
                    }

                    res.push(o);
                    input = i;
                }
            }
        }

        ret
    }};

    ($i:expr, $f:expr) => {
        $crate::parsers::many0($i, $f)
    };
}

// Improve compile time by compiling this loop only once per type it is used
// with.
//
// Not public API.
#[doc(hidden)]
pub fn many0<T>(mut input: Cursor, f: fn(Cursor) -> PResult<T>) -> PResult<Vec<T>> {
    let mut res = Vec::new();

    loop {
        if input.eof() {
            return Ok((res, input));
        }

        match f(input) {
            Err(_) => {
                return Ok((res, input));
            }
            Ok((o, i)) => {
                // loop trip must always consume (otherwise infinite loops)
                if i == input {
                    return parse_error();
                }

                res.push(o);
                input = i;
            }
        }
    }
}

/// Pattern-match the result of a parser to select which other parser to run.
///
/// - **Syntax:** `switch!(TARGET, PAT1 => THEN1 | PAT2 => THEN2 | ...)`
/// - **Output:** `T`, the return type of `THEN1` and `THEN2` and ...
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::Ident;
/// use syn::token::Brace;
/// use syn::synom::Synom;
///
/// /// Parse a unit struct or enum: either `struct S;` or `enum E { V }`.
/// enum UnitType {
///     Struct {
///         struct_token: Token![struct],
///         name: Ident,
///         semi_token: Token![;],
///     },
///     Enum {
///         enum_token: Token![enum],
///         name: Ident,
///         brace_token: Brace,
///         variant: Ident,
///     },
/// }
///
/// enum StructOrEnum {
///     Struct(Token![struct]),
///     Enum(Token![enum]),
/// }
///
/// impl Synom for StructOrEnum {
///     named!(parse -> Self, alt!(
///         keyword!(struct) => { StructOrEnum::Struct }
///         |
///         keyword!(enum) => { StructOrEnum::Enum }
///     ));
/// }
///
/// impl Synom for UnitType {
///     named!(parse -> Self, do_parse!(
///         which: syn!(StructOrEnum) >>
///         name: syn!(Ident) >>
///         item: switch!(value!(which),
///             StructOrEnum::Struct(struct_token) => map!(
///                 punct!(;),
///                 |semi_token| UnitType::Struct {
///                     struct_token,
///                     name,
///                     semi_token,
///                 }
///             )
///             |
///             StructOrEnum::Enum(enum_token) => map!(
///                 braces!(syn!(Ident)),
///                 |(brace_token, variant)| UnitType::Enum {
///                     enum_token,
///                     name,
///                     brace_token,
///                     variant,
///                 }
///             )
///         ) >>
///         (item)
///     ));
/// }
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! switch {
    ($i:expr, $submac:ident!( $($args:tt)* ), $($p:pat => $subrule:ident!( $($args2:tt)* ))|* ) => {
        match $submac!($i, $($args)*) {
            ::std::result::Result::Err(err) => ::std::result::Result::Err(err),
            ::std::result::Result::Ok((o, i)) => match o {
                $(
                    $p => $subrule!(i, $($args2)*),
                )*
            }
        }
    };
}

/// Produce the given value without parsing anything.
///
/// This can be needed where you have an existing parsed value but a parser
/// macro's syntax expects you to provide a submacro, such as in the first
/// argument of [`switch!`] or one of the branches of [`alt!`].
///
/// [`switch!`]: macro.switch.html
/// [`alt!`]: macro.alt.html
///
/// - **Syntax:** `value!(VALUE)`
/// - **Output:** `VALUE`
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::Ident;
/// use syn::token::Brace;
/// use syn::synom::Synom;
///
/// /// Parse a unit struct or enum: either `struct S;` or `enum E { V }`.
/// enum UnitType {
///     Struct {
///         struct_token: Token![struct],
///         name: Ident,
///         semi_token: Token![;],
///     },
///     Enum {
///         enum_token: Token![enum],
///         name: Ident,
///         brace_token: Brace,
///         variant: Ident,
///     },
/// }
///
/// enum StructOrEnum {
///     Struct(Token![struct]),
///     Enum(Token![enum]),
/// }
///
/// impl Synom for StructOrEnum {
///     named!(parse -> Self, alt!(
///         keyword!(struct) => { StructOrEnum::Struct }
///         |
///         keyword!(enum) => { StructOrEnum::Enum }
///     ));
/// }
///
/// impl Synom for UnitType {
///     named!(parse -> Self, do_parse!(
///         which: syn!(StructOrEnum) >>
///         name: syn!(Ident) >>
///         item: switch!(value!(which),
///             StructOrEnum::Struct(struct_token) => map!(
///                 punct!(;),
///                 |semi_token| UnitType::Struct {
///                     struct_token,
///                     name,
///                     semi_token,
///                 }
///             )
///             |
///             StructOrEnum::Enum(enum_token) => map!(
///                 braces!(syn!(Ident)),
///                 |(brace_token, variant)| UnitType::Enum {
///                     enum_token,
///                     name,
///                     brace_token,
///                     variant,
///                 }
///             )
///         ) >>
///         (item)
///     ));
/// }
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! value {
    ($i:expr, $res:expr) => {
        ::std::result::Result::Ok(($res, $i))
    };
}

/// Unconditionally fail to parse anything.
///
/// This may be useful in rejecting some arms of a `switch!` parser.
///
/// - **Syntax:** `reject!()`
/// - **Output:** never succeeds
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::Item;
///
/// // Parse any item, except for a module.
/// named!(almost_any_item -> Item,
///     switch!(syn!(Item),
///         Item::Mod(_) => reject!()
///         |
///         ok => value!(ok)
///     )
/// );
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! reject {
    ($i:expr,) => {{
        let _ = $i;
        $crate::parse_error()
    }}
}

/// Run a series of parsers and produce all of the results in a tuple.
///
/// - **Syntax:** `tuple!(A, B, C, ...)`
/// - **Output:** `(A, B, C, ...)`
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::Type;
///
/// named!(two_types -> (Type, Type), tuple!(syn!(Type), syn!(Type)));
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! tuple {
    ($i:expr, $($rest:tt)*) => {
        tuple_parser!($i, (), $($rest)*)
    };
}

// Internal parser, do not use directly.
#[doc(hidden)]
#[macro_export]
macro_rules! tuple_parser {
    ($i:expr, ($($parsed:tt),*), $e:ident, $($rest:tt)*) => {
        tuple_parser!($i, ($($parsed),*), call!($e), $($rest)*)
    };

    ($i:expr, (), $submac:ident!( $($args:tt)* ), $($rest:tt)*) => {
        match $submac!($i, $($args)*) {
            ::std::result::Result::Err(err) =>
                ::std::result::Result::Err(err),
            ::std::result::Result::Ok((o, i)) =>
                tuple_parser!(i, (o), $($rest)*),
        }
    };

    ($i:expr, ($($parsed:tt)*), $submac:ident!( $($args:tt)* ), $($rest:tt)*) => {
        match $submac!($i, $($args)*) {
            ::std::result::Result::Err(err) =>
                ::std::result::Result::Err(err),
            ::std::result::Result::Ok((o, i)) =>
                tuple_parser!(i, ($($parsed)* , o), $($rest)*),
        }
    };

    ($i:expr, ($($parsed:tt),*), $e:ident) => {
        tuple_parser!($i, ($($parsed),*), call!($e))
    };

    ($i:expr, (), $submac:ident!( $($args:tt)* )) => {
        $submac!($i, $($args)*)
    };

    ($i:expr, ($($parsed:expr),*), $submac:ident!( $($args:tt)* )) => {
        match $submac!($i, $($args)*) {
            ::std::result::Result::Err(err) =>
                ::std::result::Result::Err(err),
            ::std::result::Result::Ok((o, i)) =>
                ::std::result::Result::Ok((($($parsed),*, o), i)),
        }
    };

    ($i:expr, ($($parsed:expr),*)) => {
        ::std::result::Result::Ok((($($parsed),*), $i))
    };
}

/// Run a series of parsers, returning the result of the first one which
/// succeeds.
///
/// Optionally allows for the result to be transformed.
///
/// - **Syntax:** `alt!(THING1 | THING2 => { FUNC } | ...)`
/// - **Output:** `T`, the return type of `THING1` and `FUNC(THING2)` and ...
///
/// # Example
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::Ident;
///
/// // Parse any identifier token, or the `!` token in which case the
/// // identifier is treated as `"BANG"`.
/// named!(ident_or_bang -> Ident, alt!(
///     syn!(Ident)
///     |
///     punct!(!) => { |_| "BANG".into() }
/// ));
/// #
/// # fn main() {}
/// ```
///
/// The `alt!` macro is most commonly seen when parsing a syntax tree enum such
/// as the [`Item`] enum.
///
/// [`Item`]: enum.Item.html
///
/// ```
/// # #[macro_use]
/// # extern crate syn;
/// #
/// # use syn::synom::Synom;
/// #
/// # struct Item;
/// #
/// impl Synom for Item {
///     named!(parse -> Self, alt!(
/// #       epsilon!() => { |_| unimplemented!() }
/// #   ));
/// # }
/// #
/// # mod example {
/// #   use syn::*;
/// #
/// #   named!(parse -> Item, alt!(
///         syn!(ItemExternCrate) => { Item::ExternCrate }
///         |
///         syn!(ItemUse) => { Item::Use }
///         |
///         syn!(ItemStatic) => { Item::Static }
///         |
///         syn!(ItemConst) => { Item::Const }
///         |
///         /* ... */
/// #       syn!(ItemFn) => { Item::Fn }
///     ));
/// }
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! alt {
    ($i:expr, $e:ident | $($rest:tt)*) => {
        alt!($i, call!($e) | $($rest)*)
    };

    ($i:expr, $subrule:ident!( $($args:tt)*) | $($rest:tt)*) => {
        match $subrule!($i, $($args)*) {
            res @ ::std::result::Result::Ok(_) => res,
            _ => alt!($i, $($rest)*)
        }
    };

    ($i:expr, $subrule:ident!( $($args:tt)* ) => { $gen:expr } | $($rest:tt)+) => {
        match $subrule!($i, $($args)*) {
            ::std::result::Result::Ok((o, i)) =>
                ::std::result::Result::Ok(($crate::parsers::invoke($gen, o), i)),
            ::std::result::Result::Err(_) => alt!($i, $($rest)*),
        }
    };

    ($i:expr, $e:ident => { $gen:expr } | $($rest:tt)*) => {
        alt!($i, call!($e) => { $gen } | $($rest)*)
    };

    ($i:expr, $e:ident => { $gen:expr }) => {
        alt!($i, call!($e) => { $gen })
    };

    ($i:expr, $subrule:ident!( $($args:tt)* ) => { $gen:expr }) => {
        match $subrule!($i, $($args)*) {
            ::std::result::Result::Ok((o, i)) =>
                ::std::result::Result::Ok(($crate::parsers::invoke($gen, o), i)),
            ::std::result::Result::Err(err) =>
                ::std::result::Result::Err(err),
        }
    };

    ($i:expr, $e:ident) => {
        alt!($i, call!($e))
    };

    ($i:expr, $subrule:ident!( $($args:tt)*)) => {
        $subrule!($i, $($args)*)
    };
}

/// Run a series of parsers, optionally naming each intermediate result,
/// followed by a step to combine the intermediate results.
///
/// Produces the result of evaluating the final expression in parentheses with
/// all of the previously named results bound.
///
/// - **Syntax:** `do_parse!(name: THING1 >> THING2 >> (RESULT))`
/// - **Output:** `RESULT`
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
/// extern crate proc_macro2;
///
/// use syn::Ident;
/// use syn::token::Paren;
/// use syn::synom::Synom;
/// use proc_macro2::TokenStream;
///
/// /// Parse a macro invocation that uses `(` `)` parentheses.
/// ///
/// /// Example: `stringify!($args)`.
/// struct Macro {
///     name: Ident,
///     bang_token: Token![!],
///     paren_token: Paren,
///     tts: TokenStream,
/// }
///
/// impl Synom for Macro {
///     named!(parse -> Self, do_parse!(
///         name: syn!(Ident) >>
///         bang_token: punct!(!) >>
///         body: parens!(syn!(TokenStream)) >>
///         (Macro {
///             name,
///             bang_token,
///             paren_token: body.0,
///             tts: body.1,
///         })
///     ));
/// }
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! do_parse {
    ($i:expr, ( $($rest:expr),* )) => {
        ::std::result::Result::Ok((( $($rest),* ), $i))
    };

    ($i:expr, $e:ident >> $($rest:tt)*) => {
        do_parse!($i, call!($e) >> $($rest)*)
    };

    ($i:expr, $submac:ident!( $($args:tt)* ) >> $($rest:tt)*) => {
        match $submac!($i, $($args)*) {
            ::std::result::Result::Err(err) =>
                ::std::result::Result::Err(err),
            ::std::result::Result::Ok((_, i)) =>
                do_parse!(i, $($rest)*),
        }
    };

    ($i:expr, $field:ident : $e:ident >> $($rest:tt)*) => {
        do_parse!($i, $field: call!($e) >> $($rest)*)
    };

    ($i:expr, $field:ident : $submac:ident!( $($args:tt)* ) >> $($rest:tt)*) => {
        match $submac!($i, $($args)*) {
            ::std::result::Result::Err(err) =>
                ::std::result::Result::Err(err),
            ::std::result::Result::Ok((o, i)) => {
                let $field = o;
                do_parse!(i, $($rest)*)
            },
        }
    };

    ($i:expr, mut $field:ident : $e:ident >> $($rest:tt)*) => {
        do_parse!($i, mut $field: call!($e) >> $($rest)*)
    };

    ($i:expr, mut $field:ident : $submac:ident!( $($args:tt)* ) >> $($rest:tt)*) => {
        match $submac!($i, $($args)*) {
            ::std::result::Result::Err(err) =>
                ::std::result::Result::Err(err),
            ::std::result::Result::Ok((o, i)) => {
                let mut $field = o;
                do_parse!(i, $($rest)*)
            },
        }
    };
}

/// Parse nothing and succeed only if the end of the enclosing block has been
/// reached.
///
/// The enclosing block may be the full input if we are parsing at the top
/// level, or the surrounding parenthesis/bracket/brace if we are parsing within
/// those.
///
/// - **Syntax:** `input_end!()`
/// - **Output:** `()`
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::Expr;
/// use syn::synom::Synom;
///
/// /// Parses any Rust expression followed either by a semicolon or by the end
/// /// of the input.
/// ///
/// /// For example `many0!(syn!(TerminatedExpr))` would successfully parse the
/// /// following input into three expressions.
/// ///
/// ///     1 + 1; second.two(); third!()
/// ///
/// /// Similarly within a block, `braced!(many0!(syn!(TerminatedExpr)))` would
/// /// successfully parse three expressions.
/// ///
/// ///     { 1 + 1; second.two(); third!() }
/// struct TerminatedExpr {
///     expr: Expr,
///     semi_token: Option<Token![;]>,
/// }
///
/// impl Synom for TerminatedExpr {
///     named!(parse -> Self, do_parse!(
///         expr: syn!(Expr) >>
///         semi_token: alt!(
///             input_end!() => { |_| None }
///             |
///             punct!(;) => { Some }
///         ) >>
///         (TerminatedExpr {
///             expr,
///             semi_token,
///         })
///     ));
/// }
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! input_end {
    ($i:expr,) => {
        $crate::parsers::input_end($i)
    };
}

// Not a public API
#[doc(hidden)]
pub fn input_end(input: Cursor) -> PResult<'static, ()> {
    if input.eof() {
        Ok(((), Cursor::empty()))
    } else {
        parse_error()
    }
}

/// Turn a failed parse into `None` and a successful parse into `Some`.
///
/// A failed parse consumes none of the input.
///
/// - **Syntax:** `option!(THING)`
/// - **Output:** `Option<THING>`
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::{Label, Block};
/// use syn::synom::Synom;
///
/// /// Parses a Rust loop. Equivalent to syn::ExprLoop.
/// ///
/// /// Examples:
/// ///     loop { println!("y"); }
/// ///     'x: loop { break 'x; }
/// struct ExprLoop {
///     label: Option<Label>,
///     loop_token: Token![loop],
///     body: Block,
/// }
///
/// impl Synom for ExprLoop {
///     named!(parse -> Self, do_parse!(
///         // Loop may or may not have a label.
///         label: option!(syn!(Label)) >>
///         loop_token: keyword!(loop) >>
///         body: syn!(Block) >>
///         (ExprLoop {
///             label,
///             loop_token,
///             body,
///         })
///     ));
/// }
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! option {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {
        match $submac!($i, $($args)*) {
            ::std::result::Result::Ok((o, i)) =>
                ::std::result::Result::Ok((Some(o), i)),
            ::std::result::Result::Err(_) =>
                ::std::result::Result::Ok((None, $i)),
        }
    };

    ($i:expr, $f:expr) => {
        option!($i, call!($f));
    };
}

/// Parses nothing and always succeeds.
///
/// This can be useful as a fallthrough case in [`alt!`], as shown below. Also
/// useful for parsing empty delimiters using [`parens!`] or [`brackets!`] or
/// [`braces!`] by parsing for example `braces!(epsilon!())` for an empty `{}`.
///
/// [`alt!`]: macro.alt.html
/// [`parens!`]: macro.parens.html
/// [`brackets!`]: macro.brackets.html
/// [`braces!`]: macro.braces.html
///
/// - **Syntax:** `epsilon!()`
/// - **Output:** `()`
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::synom::Synom;
///
/// enum Mutability {
///     Mutable(Token![mut]),
///     Immutable,
/// }
///
/// impl Synom for Mutability {
///     named!(parse -> Self, alt!(
///         keyword!(mut) => { Mutability::Mutable }
///         |
///         epsilon!() => { |_| Mutability::Immutable }
///     ));
/// }
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! epsilon {
    ($i:expr,) => {
        ::std::result::Result::Ok(((), $i))
    };
}

/// Run a parser, binding the result to a name, and then evaluating an
/// expression.
///
/// Discards the result of the expression and parser.
///
/// - **Syntax:** `tap!(NAME : THING => EXPR)`
/// - **Output:** `()`
#[doc(hidden)]
#[macro_export]
macro_rules! tap {
    ($i:expr, $name:ident : $submac:ident!( $($args:tt)* ) => $e:expr) => {
        match $submac!($i, $($args)*) {
            ::std::result::Result::Ok((o, i)) => {
                let $name = o;
                $e;
                ::std::result::Result::Ok(((), i))
            }
            ::std::result::Result::Err(err) =>
                ::std::result::Result::Err(err),
        }
    };

    ($i:expr, $name:ident : $f:expr => $e:expr) => {
        tap!($i, $name: call!($f) => $e);
    };
}

/// Parse any type that implements the `Synom` trait.
///
/// Any type implementing [`Synom`] can be used with this parser, whether the
/// implementation is provided by Syn or is one that you write.
///
/// [`Synom`]: synom/trait.Synom.html
///
/// - **Syntax:** `syn!(TYPE)`
/// - **Output:** `TYPE`
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::{Ident, Item};
/// use syn::token::Brace;
/// use syn::synom::Synom;
///
/// /// Parses a module containing zero or more Rust items.
/// ///
/// /// Example: `mod m { type Result<T> = ::std::result::Result<T, MyError>; }`
/// struct SimpleMod {
///     mod_token: Token![mod],
///     name: Ident,
///     brace_token: Brace,
///     items: Vec<Item>,
/// }
///
/// impl Synom for SimpleMod {
///     named!(parse -> Self, do_parse!(
///         mod_token: keyword!(mod) >>
///         name: syn!(Ident) >>
///         body: braces!(many0!(syn!(Item))) >>
///         (SimpleMod {
///             mod_token,
///             name,
///             brace_token: body.0,
///             items: body.1,
///         })
///     ));
/// }
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! syn {
    ($i:expr, $t:ty) => {
        <$t as $crate::synom::Synom>::parse($i)
    };
}

/// Parse inside of `(` `)` parentheses.
///
/// This macro parses a set of balanced parentheses and invokes a sub-parser on
/// the content inside. The sub-parser is required to consume all tokens within
/// the parentheses in order for this parser to return successfully.
///
/// - **Syntax:** `parens!(CONTENT)`
/// - **Output:** `(token::Paren, CONTENT)`
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::Expr;
/// use syn::token::Paren;
///
/// /// Parses an expression inside of parentheses.
/// ///
/// /// Example: `(1 + 1)`
/// named!(expr_paren -> (Paren, Expr), parens!(syn!(Expr)));
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! parens {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {
        $crate::token::Paren::parse($i, |i| $submac!(i, $($args)*))
    };

    ($i:expr, $f:expr) => {
        parens!($i, call!($f));
    };
}

/// Parse inside of `[` `]` square brackets.
///
/// This macro parses a set of balanced brackets and invokes a sub-parser on the
/// content inside. The sub-parser is required to consume all tokens within the
/// brackets in order for this parser to return successfully.
///
/// - **Syntax:** `brackets!(CONTENT)`
/// - **Output:** `(token::Bracket, CONTENT)`
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::Expr;
/// use syn::token::Bracket;
///
/// /// Parses an expression inside of brackets.
/// ///
/// /// Example: `[1 + 1]`
/// named!(expr_paren -> (Bracket, Expr), brackets!(syn!(Expr)));
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! brackets {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {
        $crate::token::Bracket::parse($i, |i| $submac!(i, $($args)*))
    };

    ($i:expr, $f:expr) => {
        brackets!($i, call!($f));
    };
}

/// Parse inside of `{` `}` curly braces.
///
/// This macro parses a set of balanced braces and invokes a sub-parser on the
/// content inside. The sub-parser is required to consume all tokens within the
/// braces in order for this parser to return successfully.
///
/// - **Syntax:** `braces!(CONTENT)`
/// - **Output:** `(token::Brace, CONTENT)`
///
/// ```rust
/// #[macro_use]
/// extern crate syn;
///
/// use syn::Expr;
/// use syn::token::Brace;
///
/// /// Parses an expression inside of braces.
/// ///
/// /// Example: `{1 + 1}`
/// named!(expr_paren -> (Brace, Expr), braces!(syn!(Expr)));
/// #
/// # fn main() {}
/// ```
///
/// *This macro is available if Syn is built with the `"parsing"` feature.*
#[macro_export]
macro_rules! braces {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {
        $crate::token::Brace::parse($i, |i| $submac!(i, $($args)*))
    };

    ($i:expr, $f:expr) => {
        braces!($i, call!($f));
    };
}

// Not public API.
#[doc(hidden)]
#[macro_export]
macro_rules! grouped {
    ($i:expr, $submac:ident!( $($args:tt)* )) => {
        $crate::token::Group::parse($i, |i| $submac!(i, $($args)*))
    };

    ($i:expr, $f:expr) => {
        grouped!($i, call!($f));
    };
}
