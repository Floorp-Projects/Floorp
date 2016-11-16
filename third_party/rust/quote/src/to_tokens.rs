use super::Tokens;

pub trait ToTokens {
    fn to_tokens(&self, &mut Tokens);
}

impl<'a, T: ToTokens> ToTokens for &'a T {
    fn to_tokens(&self, tokens: &mut Tokens) {
        (**self).to_tokens(tokens);
    }
}

impl<T: ToTokens> ToTokens for Box<T> {
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
        tokens.append(&escape_str(self));
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

#[derive(Debug)]
pub struct ByteStr<'a>(pub &'a str);

impl<'a> ToTokens for ByteStr<'a> {
    fn to_tokens(&self, tokens: &mut Tokens) {
        tokens.append(&format!("b{}", escape_str(self.0)));
    }
}

fn escape_str(s: &str) -> String {
    let mut escaped = "\"".to_string();
    for ch in s.chars() {
        match ch {
            '\0' => escaped.push_str(r"\0"),
            '\'' => escaped.push_str("'"),
            _ => escaped.extend(ch.escape_default().map(|c| c as char)),
        }
    }
    escaped.push('"');
    escaped
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

macro_rules! impl_to_tokens_integer {
    ($ty:ty) => {
        impl ToTokens for $ty {
            fn to_tokens(&self, tokens: &mut Tokens) {
                tokens.append(&format!(concat!("{}", stringify!($ty)), self));
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
