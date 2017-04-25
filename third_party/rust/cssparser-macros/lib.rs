/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#[macro_use] extern crate procedural_masquerade;
extern crate phf_codegen;
extern crate proc_macro;
#[macro_use] extern crate quote;
extern crate syn;

use std::ascii::AsciiExt;

define_proc_macros! {
    /// Input: the arms of a `match` expression.
    ///
    /// Output: a `MAX_LENGTH` constant with the length of the longest string pattern.
    ///
    /// Panic if the arms contain non-string patterns,
    /// or string patterns that contains ASCII uppercase letters.
    #[allow(non_snake_case)]
    pub fn cssparser_internal__assert_ascii_lowercase__max_len(input: &str) -> String {
        let expr = syn::parse_expr(&format!("match x {{ {} }}", input)).unwrap();
        let arms = match expr {
            syn::Expr { node: syn::ExprKind::Match(_, ref arms), .. } => arms,
            _ => panic!("expected a match expression, got {:?}", expr)
        };
        max_len(arms.iter().flat_map(|arm| &arm.pats).filter_map(|pattern| {
            let expr = match *pattern {
                syn::Pat::Lit(ref expr) => expr,
                syn::Pat::Wild => return None,
                _ => panic!("expected string or wildcard pattern, got {:?}", pattern)
            };
            match **expr {
                syn::Expr { node: syn::ExprKind::Lit(syn::Lit::Str(ref string, _)), .. } => {
                    assert_eq!(*string, string.to_ascii_lowercase(),
                               "string patterns must be given in ASCII lowercase");
                    Some(string.len())
                }
                _ => panic!("expected string pattern, got {:?}", expr)
            }
        }))
    }

    /// Input: string literals with no separator
    ///
    /// Output: a `MAX_LENGTH` constant with the length of the longest string.
    #[allow(non_snake_case)]
    pub fn cssparser_internal__max_len(input: &str) -> String {
        max_len(syn::parse_token_trees(input).unwrap().iter().map(|tt| string_literal(tt).len()))
    }

    /// Input: parsed as token trees. The first TT is a type. (Can be wrapped in parens.)
    /// following TTs are grouped in pairs, each pair being a key as a string literal
    /// and the corresponding value as a const expression.
    ///
    /// Output: a rust-phf map, with keys ASCII-lowercased:
    /// ```text
    /// static MAP: &'static ::cssparser::phf::Map<&'static str, $ValueType> = …;
    /// ```
    #[allow(non_snake_case)]
    pub fn cssparser_internal__phf_map(input: &str) -> String {
        let token_trees = syn::parse_token_trees(input).unwrap();
        let value_type = &token_trees[0];
        let pairs: Vec<_> = token_trees[1..].chunks(2).map(|chunk| {
            let key = string_literal(&chunk[0]);
            let value = &chunk[1];
            (key.to_ascii_lowercase(), quote!(#value).to_string())
        }).collect();

        let mut map = phf_codegen::Map::new();
        map.phf_path("::cssparser::_internal__phf");
        for &(ref key, ref value) in &pairs {
            map.entry(&**key, &**value);
        }

        let mut tokens = quote! {
            static MAP: ::cssparser::_internal__phf::Map<&'static str, #value_type> =
        };
        let mut initializer_bytes = Vec::new();
        map.build(&mut initializer_bytes).unwrap();
        tokens.append(::std::str::from_utf8(&initializer_bytes).unwrap());
        tokens.append(";");
        tokens.into_string()
    }
}

fn max_len<I: Iterator<Item=usize>>(lengths: I) -> String {
    let max_length = lengths.max().expect("expected at least one string");
    quote!( const MAX_LENGTH: usize = #max_length; ).into_string()
}

fn string_literal(token: &syn::TokenTree) -> &str {
    match *token {
        syn::TokenTree::Token(syn::Token::Literal(syn::Lit::Str(ref string, _))) => string,
        _ => panic!("expected string literal, got {:?}", token)
    }
}
