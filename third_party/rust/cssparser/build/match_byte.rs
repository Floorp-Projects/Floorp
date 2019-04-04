/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use quote::ToTokens;
use std::fs::File;
use std::io::{Read, Write};
use std::path::Path;
use syn;
use syn::fold::Fold;
use syn::parse::{Parse, ParseStream, Result};

use proc_macro2::{Span, TokenStream};

struct MatchByteParser {
}

pub fn expand(from: &Path, to: &Path) {
    let mut source = String::new();
    File::open(from).unwrap().read_to_string(&mut source).unwrap();
    let ast = syn::parse_file(&source).expect("Parsing rules.rs module");
    let mut m = MatchByteParser {};
    let ast = m.fold_file(ast);

    let code = ast.into_token_stream().to_string().replace("{ ", "{\n").replace(" }", "\n}");
    File::create(to).unwrap().write_all(code.as_bytes()).unwrap();
}

struct MatchByte {
    expr: syn::Expr,
    arms: Vec<syn::Arm>,
}

impl Parse for MatchByte {
    fn parse(input: ParseStream) -> Result<Self> {
        Ok(MatchByte {
            expr: {
                let expr = input.parse()?;
                input.parse::<Token![,]>()?;
                expr
            },
            arms: {
                let mut arms = Vec::new();
                while !input.is_empty() {
                    arms.push(input.call(syn::Arm::parse)?);
                }
                arms
            }
        })
    }
}

fn get_byte_from_expr_lit(expr: &Box<syn::Expr>) -> u8 {
    match **expr {
        syn::Expr::Lit(syn::ExprLit { ref lit, .. }) => {
            if let syn::Lit::Byte(ref byte) = *lit {
                byte.value()
            }
            else {
                panic!("Found a pattern that wasn't a byte")
            }
        },
        _ => unreachable!(),
    }
}


/// Expand a TokenStream corresponding to the `match_byte` macro.
///
/// ## Example
///
/// ```rust
/// match_byte! { tokenizer.next_byte_unchecked(),
///     b'a'..b'z' => { ... }
///     b'0'..b'9' => { ... }
///     b'\n' | b'\\' => { ... }
///     foo => { ... }
///  }
///  ```
///
fn expand_match_byte(body: &TokenStream) -> syn::Expr {
    let match_byte: MatchByte = syn::parse2(body.clone()).unwrap();
    let expr = match_byte.expr;
    let mut cases = Vec::new();
    let mut table = [0u8; 256];
    let mut match_body = Vec::new();
    let mut wildcard = None;

    for (i, ref arm) in match_byte.arms.iter().enumerate() {
        let case_id = i + 1;
        let index = case_id as isize;
        let name = syn::Ident::new(&format!("Case{}", case_id), Span::call_site());

        for pat in &arm.pats {
            match pat {
                &syn::Pat::Lit(syn::PatLit{ref expr}) => {
                    let value = get_byte_from_expr_lit(expr);
                    if table[value as usize] == 0 {
                        table[value as usize] = case_id as u8;
                    }
                },
                &syn::Pat::Range(syn::PatRange { ref lo, ref hi, .. }) => {
                    let lo = get_byte_from_expr_lit(lo);
                    let hi = get_byte_from_expr_lit(hi);
                    for value in lo..hi {
                        if table[value as usize] == 0 {
                            table[value as usize] = case_id as u8;
                        }
                    }
                    if table[hi as usize] == 0 {
                        table[hi as usize] = case_id as u8;
                    }
                },
                &syn::Pat::Wild(_) => {
                    for byte in table.iter_mut() {
                        if *byte == 0 {
                            *byte = case_id as u8;
                        }
                    }
                },
                &syn::Pat::Ident(syn::PatIdent { ref ident, .. }) => {
                    assert_eq!(wildcard, None);
                    wildcard = Some(ident);
                    for byte in table.iter_mut() {
                        if *byte == 0 {
                            *byte = case_id as u8;
                        }
                    }
                },
                _ => {
                    panic!("Unexpected pattern: {:?}. Buggy code ?", pat);
                }
            }
        }
        cases.push(quote!(#name = #index));
        let body = &arm.body;
        match_body.push(quote!(Case::#name => { #body }))
    }
    let en = quote!(enum Case {
        #(#cases),*
    });

    let mut table_content = Vec::new();
    for entry in table.iter() {
        let name: syn::Path = syn::parse_str(&format!("Case::Case{}", entry)).unwrap();
        table_content.push(name);
    }
    let table = quote!(static __CASES: [Case; 256] = [#(#table_content),*];);

    let expr = if let Some(binding) = wildcard {
        quote!({ #en #table let #binding = #expr; match __CASES[#binding as usize] { #(#match_body),* }})
    } else {
        quote!({ #en #table match __CASES[#expr as usize] { #(#match_body),* }})
    };

    syn::parse2(expr.into()).unwrap()
}

impl Fold for MatchByteParser {
    fn fold_stmt(&mut self, stmt: syn::Stmt) -> syn::Stmt {
        match stmt {
            syn::Stmt::Item(syn::Item::Macro(syn::ItemMacro{ ref mac, .. })) => {
                if mac.path == parse_quote!(match_byte) {
                    return syn::fold::fold_stmt(self, syn::Stmt::Expr(expand_match_byte(&mac.tts)))
                }
            },
            _ => {}
        }

        syn::fold::fold_stmt(self, stmt)
    }

    fn fold_expr(&mut self, expr: syn::Expr) -> syn::Expr {
        match expr {
            syn::Expr::Macro(syn::ExprMacro{ ref mac, .. }) => {
                if mac.path == parse_quote!(match_byte) {
                    return syn::fold::fold_expr(self, expand_match_byte(&mac.tts))
                }
            },
            _ => {}
        }

        syn::fold::fold_expr(self, expr)
    }
}
