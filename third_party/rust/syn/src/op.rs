// Copyright 2018 Syn Developers
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

ast_enum! {
    /// A binary operator: `+`, `+=`, `&`.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    #[cfg_attr(feature = "clone-impls", derive(Copy))]
    pub enum BinOp {
        /// The `+` operator (addition)
        Add(Token![+]),
        /// The `-` operator (subtraction)
        Sub(Token![-]),
        /// The `*` operator (multiplication)
        Mul(Token![*]),
        /// The `/` operator (division)
        Div(Token![/]),
        /// The `%` operator (modulus)
        Rem(Token![%]),
        /// The `&&` operator (logical and)
        And(Token![&&]),
        /// The `||` operator (logical or)
        Or(Token![||]),
        /// The `^` operator (bitwise xor)
        BitXor(Token![^]),
        /// The `&` operator (bitwise and)
        BitAnd(Token![&]),
        /// The `|` operator (bitwise or)
        BitOr(Token![|]),
        /// The `<<` operator (shift left)
        Shl(Token![<<]),
        /// The `>>` operator (shift right)
        Shr(Token![>>]),
        /// The `==` operator (equality)
        Eq(Token![==]),
        /// The `<` operator (less than)
        Lt(Token![<]),
        /// The `<=` operator (less than or equal to)
        Le(Token![<=]),
        /// The `!=` operator (not equal to)
        Ne(Token![!=]),
        /// The `>=` operator (greater than or equal to)
        Ge(Token![>=]),
        /// The `>` operator (greater than)
        Gt(Token![>]),
        /// The `+=` operator
        AddEq(Token![+=]),
        /// The `-=` operator
        SubEq(Token![-=]),
        /// The `*=` operator
        MulEq(Token![*=]),
        /// The `/=` operator
        DivEq(Token![/=]),
        /// The `%=` operator
        RemEq(Token![%=]),
        /// The `^=` operator
        BitXorEq(Token![^=]),
        /// The `&=` operator
        BitAndEq(Token![&=]),
        /// The `|=` operator
        BitOrEq(Token![|=]),
        /// The `<<=` operator
        ShlEq(Token![<<=]),
        /// The `>>=` operator
        ShrEq(Token![>>=]),
    }
}

ast_enum! {
    /// A unary operator: `*`, `!`, `-`.
    ///
    /// *This type is available if Syn is built with the `"derive"` or `"full"`
    /// feature.*
    #[cfg_attr(feature = "clone-impls", derive(Copy))]
    pub enum UnOp {
        /// The `*` operator for dereferencing
        Deref(Token![*]),
        /// The `!` operator for logical inversion
        Not(Token![!]),
        /// The `-` operator for negation
        Neg(Token![-]),
    }
}

#[cfg(feature = "parsing")]
pub mod parsing {
    use super::*;
    use synom::Synom;

    impl BinOp {
        named!(pub parse_binop -> Self, alt!(
            punct!(&&) => { BinOp::And }
            |
            punct!(||) => { BinOp::Or }
            |
            punct!(<<) => { BinOp::Shl }
            |
            punct!(>>) => { BinOp::Shr }
            |
            punct!(==) => { BinOp::Eq }
            |
            punct!(<=) => { BinOp::Le }
            |
            punct!(!=) => { BinOp::Ne }
            |
            punct!(>=) => { BinOp::Ge }
            |
            punct!(+) => { BinOp::Add }
            |
            punct!(-) => { BinOp::Sub }
            |
            punct!(*) => { BinOp::Mul }
            |
            punct!(/) => { BinOp::Div }
            |
            punct!(%) => { BinOp::Rem }
            |
            punct!(^) => { BinOp::BitXor }
            |
            punct!(&) => { BinOp::BitAnd }
            |
            punct!(|) => { BinOp::BitOr }
            |
            punct!(<) => { BinOp::Lt }
            |
            punct!(>) => { BinOp::Gt }
        ));

        #[cfg(feature = "full")]
        named!(pub parse_assign_op -> Self, alt!(
            punct!(+=) => { BinOp::AddEq }
            |
            punct!(-=) => { BinOp::SubEq }
            |
            punct!(*=) => { BinOp::MulEq }
            |
            punct!(/=) => { BinOp::DivEq }
            |
            punct!(%=) => { BinOp::RemEq }
            |
            punct!(^=) => { BinOp::BitXorEq }
            |
            punct!(&=) => { BinOp::BitAndEq }
            |
            punct!(|=) => { BinOp::BitOrEq }
            |
            punct!(<<=) => { BinOp::ShlEq }
            |
            punct!(>>=) => { BinOp::ShrEq }
        ));
    }

    impl Synom for UnOp {
        named!(parse -> Self, alt!(
            punct!(*) => { UnOp::Deref }
            |
            punct!(!) => { UnOp::Not }
            |
            punct!(-) => { UnOp::Neg }
        ));

        fn description() -> Option<&'static str> {
            Some("unary operator: `*`, `!`, or `-`")
        }
    }
}

#[cfg(feature = "printing")]
mod printing {
    use super::*;
    use proc_macro2::TokenStream;
    use quote::ToTokens;

    impl ToTokens for BinOp {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            match *self {
                BinOp::Add(ref t) => t.to_tokens(tokens),
                BinOp::Sub(ref t) => t.to_tokens(tokens),
                BinOp::Mul(ref t) => t.to_tokens(tokens),
                BinOp::Div(ref t) => t.to_tokens(tokens),
                BinOp::Rem(ref t) => t.to_tokens(tokens),
                BinOp::And(ref t) => t.to_tokens(tokens),
                BinOp::Or(ref t) => t.to_tokens(tokens),
                BinOp::BitXor(ref t) => t.to_tokens(tokens),
                BinOp::BitAnd(ref t) => t.to_tokens(tokens),
                BinOp::BitOr(ref t) => t.to_tokens(tokens),
                BinOp::Shl(ref t) => t.to_tokens(tokens),
                BinOp::Shr(ref t) => t.to_tokens(tokens),
                BinOp::Eq(ref t) => t.to_tokens(tokens),
                BinOp::Lt(ref t) => t.to_tokens(tokens),
                BinOp::Le(ref t) => t.to_tokens(tokens),
                BinOp::Ne(ref t) => t.to_tokens(tokens),
                BinOp::Ge(ref t) => t.to_tokens(tokens),
                BinOp::Gt(ref t) => t.to_tokens(tokens),
                BinOp::AddEq(ref t) => t.to_tokens(tokens),
                BinOp::SubEq(ref t) => t.to_tokens(tokens),
                BinOp::MulEq(ref t) => t.to_tokens(tokens),
                BinOp::DivEq(ref t) => t.to_tokens(tokens),
                BinOp::RemEq(ref t) => t.to_tokens(tokens),
                BinOp::BitXorEq(ref t) => t.to_tokens(tokens),
                BinOp::BitAndEq(ref t) => t.to_tokens(tokens),
                BinOp::BitOrEq(ref t) => t.to_tokens(tokens),
                BinOp::ShlEq(ref t) => t.to_tokens(tokens),
                BinOp::ShrEq(ref t) => t.to_tokens(tokens),
            }
        }
    }

    impl ToTokens for UnOp {
        fn to_tokens(&self, tokens: &mut TokenStream) {
            match *self {
                UnOp::Deref(ref t) => t.to_tokens(tokens),
                UnOp::Not(ref t) => t.to_tokens(tokens),
                UnOp::Neg(ref t) => t.to_tokens(tokens),
            }
        }
    }
}
