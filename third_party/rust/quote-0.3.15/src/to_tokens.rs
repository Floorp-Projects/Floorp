use super::Tokens;

use std::borrow::Cow;

/// Types that can be interpolated inside a `quote!(...)` invocation.
pub trait ToTokens {
    /// Write `self` to the given `Tokens`.
    ///
    /// Example implementation for a struct representing Rust paths like
    /// `std::cmp::PartialEq`:
    ///
    /// ```ignore
    /// pub struct Path {
    ///     pub global: bool,
    ///     pub segments: Vec<PathSegment>,
    /// }
    ///
    /// impl ToTokens for Path {
    ///     fn to_tokens(&self, tokens: &mut Tokens) {
    ///         for (i, segment) in self.segments.iter().enumerate() {
    ///             if i > 0 || self.global {
    ///                 tokens.append("::");
    ///             }
    ///             segment.to_tokens(tokens);
    ///         }
    ///     }
    /// }
    /// ```
    fn to_tokens(&self, &mut Tokens);
}

impl<'a, T: ?Sized + ToTokens> ToTokens for &'a T {
    fn to_tokens(&self, tokens: &mut Tokens) {
        (**self).to_tokens(tokens);
    }
}

impl<'a, T: ?Sized + ToOwned + ToTokens> ToTokens for Cow<'a, T> {
    fn to_tokens(&self, tokens: &mut Tokens) {
        (**self).to_tokens(tokens);
    }
}

impl<T: ?Sized + ToTokens> ToTokens for Box<T> {
    fn to_tokens(&self, tokens: &mut Tokens) {
        (**self).to_tokens(tokens);
    }
}

impl<T: ToTokens> ToTokens for Option<T> {
    fn to_tokens(&self, tokens: &mut Tokens) {
        if let Some(ref t) = *self {
            t.to_tokens(tokens);
        }
    }
}

impl ToTokens for str {
    fn to_tokens(&self, tokens: &mut Tokens) {
        let mut escaped = "\"".to_string();
        for ch in self.chars() {
            match ch {
                '\0' => escaped.push_str(r"\0"),
                '\'' => escaped.push_str("'"),
                _ => escaped.extend(ch.escape_default().map(|c| c as char)),
            }
        }
        escaped.push('"');

        tokens.append(&escaped);
    }
}

impl ToTokens for String {
    fn to_tokens(&self, tokens: &mut Tokens) {
        self.as_str().to_tokens(tokens);
    }
}

impl ToTokens for char {
    fn to_tokens(&self, tokens: &mut Tokens) {
        match *self {
            '\0' => tokens.append(r"'\0'"),
            '"' => tokens.append("'\"'"),
            _ => tokens.append(&format!("{:?}", self)),
        }
    }
}

/// Wrap a `&str` so it interpolates as a byte-string: `b"abc"`.
#[derive(Debug)]
pub struct ByteStr<'a>(pub &'a str);

impl<'a> ToTokens for ByteStr<'a> {
    fn to_tokens(&self, tokens: &mut Tokens) {
        let mut escaped = "b\"".to_string();
        for b in self.0.bytes() {
            match b {
                b'\0' => escaped.push_str(r"\0"),
                b'\t' => escaped.push_str(r"\t"),
                b'\n' => escaped.push_str(r"\n"),
                b'\r' => escaped.push_str(r"\r"),
                b'"' => escaped.push_str("\\\""),
                b'\\' => escaped.push_str("\\\\"),
                b'\x20' ... b'\x7E' => escaped.push(b as char),
                _ => escaped.push_str(&format!("\\x{:02X}", b)),
            }
        }
        escaped.push('"');

        tokens.append(&escaped);
    }
}

macro_rules! impl_to_tokens_display {
    ($ty:ty) => {
        impl ToTokens for $ty {
            fn to_tokens(&self, tokens: &mut Tokens) {
                tokens.append(&self.to_string());
            }
        }
    };
}

impl_to_tokens_display!(Tokens);
impl_to_tokens_display!(bool);

/// Wrap an integer so it interpolates as a hexadecimal.
#[derive(Debug)]
pub struct Hex<T>(pub T);

macro_rules! impl_to_tokens_integer {
    ($ty:ty) => {
        impl ToTokens for $ty {
            fn to_tokens(&self, tokens: &mut Tokens) {
                tokens.append(&format!(concat!("{}", stringify!($ty)), self));
            }
        }

        impl ToTokens for Hex<$ty> {
            fn to_tokens(&self, tokens: &mut Tokens) {
                tokens.append(&format!(concat!("0x{:X}", stringify!($ty)), self.0));
            }
        }
    };
}

impl_to_tokens_integer!(i8);
impl_to_tokens_integer!(i16);
impl_to_tokens_integer!(i32);
impl_to_tokens_integer!(i64);
impl_to_tokens_integer!(isize);
impl_to_tokens_integer!(u8);
impl_to_tokens_integer!(u16);
impl_to_tokens_integer!(u32);
impl_to_tokens_integer!(u64);
impl_to_tokens_integer!(usize);

macro_rules! impl_to_tokens_floating {
    ($ty:ty) => {
        impl ToTokens for $ty {
            fn to_tokens(&self, tokens: &mut Tokens) {
                use std::num::FpCategory::*;
                match self.classify() {
                    Zero | Subnormal | Normal => {
                        tokens.append(&format!(concat!("{}", stringify!($ty)), self));
                    }
                    Nan => {
                        tokens.append("::");
                        tokens.append("std");
                        tokens.append("::");
                        tokens.append(stringify!($ty));
                        tokens.append("::");
                        tokens.append("NAN");
                    }
                    Infinite => {
                        tokens.append("::");
                        tokens.append("std");
                        tokens.append("::");
                        tokens.append(stringify!($ty));
                        tokens.append("::");
                        if self.is_sign_positive() {
                            tokens.append("INFINITY");
                        } else {
                            tokens.append("NEG_INFINITY");
                        }
                    }
                }
            }
        }
    };
}
impl_to_tokens_floating!(f32);
impl_to_tokens_floating!(f64);

impl<T: ToTokens> ToTokens for [T] {
    fn to_tokens(&self, tokens: &mut Tokens) {
        tokens.append("[");
        for item in self {
            item.to_tokens(tokens);
            tokens.append(",");
        }
        tokens.append("]");
    }
}

impl<T: ToTokens> ToTokens for Vec<T> {
    fn to_tokens(&self, tokens: &mut Tokens) {
        self[..].to_tokens(tokens)
    }
}

macro_rules! array_impls {
    ($($N:expr)+) => {
        $(
            impl<T: ToTokens> ToTokens for [T; $N] {
                fn to_tokens(&self, tokens: &mut Tokens) {
                    self[..].to_tokens(tokens)
                }
            }
        )+
    }
}

array_impls! {
     0  1  2  3  4  5  6  7  8  9
    10 11 12 13 14 15 16 17 18 19
    20 21 22 23 24 25 26 27 28 29
    30 31 32
}

macro_rules! tuple_impls {
    ($(
        $Tuple:ident {
            $(($idx:tt) -> $T:ident)*
        }
    )+) => {
        $(
            impl<$($T: ToTokens),*> ToTokens for ($($T,)*) {
                fn to_tokens(&self, tokens: &mut Tokens) {
                    tokens.append("(");
                    $(
                        self.$idx.to_tokens(tokens);
                        tokens.append(",");
                    )*
                    tokens.append(")");
                }
            }
        )+
    }
}

tuple_impls! {
    Tuple0 {}
    Tuple1 {
        (0) -> A
    }
    Tuple2 {
        (0) -> A
        (1) -> B
    }
    Tuple3 {
        (0) -> A
        (1) -> B
        (2) -> C
    }
    Tuple4 {
        (0) -> A
        (1) -> B
        (2) -> C
        (3) -> D
    }
    Tuple5 {
        (0) -> A
        (1) -> B
        (2) -> C
        (3) -> D
        (4) -> E
    }
    Tuple6 {
        (0) -> A
        (1) -> B
        (2) -> C
        (3) -> D
        (4) -> E
        (5) -> F
    }
    Tuple7 {
        (0) -> A
        (1) -> B
        (2) -> C
        (3) -> D
        (4) -> E
        (5) -> F
        (6) -> G
    }
    Tuple8 {
        (0) -> A
        (1) -> B
        (2) -> C
        (3) -> D
        (4) -> E
        (5) -> F
        (6) -> G
        (7) -> H
    }
    Tuple9 {
        (0) -> A
        (1) -> B
        (2) -> C
        (3) -> D
        (4) -> E
        (5) -> F
        (6) -> G
        (7) -> H
        (8) -> I
    }
    Tuple10 {
        (0) -> A
        (1) -> B
        (2) -> C
        (3) -> D
        (4) -> E
        (5) -> F
        (6) -> G
        (7) -> H
        (8) -> I
        (9) -> J
    }
    Tuple11 {
        (0) -> A
        (1) -> B
        (2) -> C
        (3) -> D
        (4) -> E
        (5) -> F
        (6) -> G
        (7) -> H
        (8) -> I
        (9) -> J
        (10) -> K
    }
    Tuple12 {
        (0) -> A
        (1) -> B
        (2) -> C
        (3) -> D
        (4) -> E
        (5) -> F
        (6) -> G
        (7) -> H
        (8) -> I
        (9) -> J
        (10) -> K
        (11) -> L
    }
}
