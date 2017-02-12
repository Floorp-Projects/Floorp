/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use quote::{ToTokens, Tokens};
use std::fs::File;
use std::io::{Read, Write};
use std::path::Path;
use std::vec;
use std::iter;
use syn;

pub fn expand(from: &Path, to: &Path) {
    let mut source = String::new();
    File::open(from).unwrap().read_to_string(&mut source).unwrap();
    let tts = syn::parse_token_trees(&source).expect("Parsing rules.rs module");
    let mut tokens = Tokens::new();
    tokens.append_all(expand_tts(tts));

    let code = tokens.to_string().replace("{ ", "{\n").replace(" }", "\n}");
    File::create(to).unwrap().write_all(code.as_bytes()).unwrap();
}

fn expand_tts(tts: Vec<syn::TokenTree>) -> Vec<syn::TokenTree> {
    use syn::*;
    let mut expanded = Vec::new();
    let mut tts = tts.into_iter();
    while let Some(tt) = tts.next() {
        match tt {
            TokenTree::Token(Token::Ident(ident)) => {
                if ident != "match_byte" {
                    expanded.push(TokenTree::Token(Token::Ident(ident)));
                    continue;
                }

                match tts.next() {
                    Some(TokenTree::Token(Token::Not)) => {},
                    other => {
                        expanded.push(TokenTree::Token(Token::Ident(ident)));
                        if let Some(other) = other {
                            expanded.push(other);
                        }
                        continue;
                    }
                }

                let tts = match tts.next() {
                    Some(TokenTree::Delimited(Delimited { tts, .. })) => tts,
                    other => {
                        expanded.push(TokenTree::Token(Token::Ident(ident)));
                        expanded.push(TokenTree::Token(Token::Not));
                        if let Some(other) = other {
                            expanded.push(other);
                        }
                        continue;
                    }
                };

                let (to_be_matched, table, cases, wildcard_binding) = parse_match_bytes_macro(tts);
                let expr = expand_match_bytes_macro(to_be_matched,
                                                    &table,
                                                    cases,
                                                    wildcard_binding);

                let tts = syn::parse_token_trees(&expr)
                    .expect("parsing macro expansion as token trees");
                expanded.extend(expand_tts(tts));
            }
            TokenTree::Delimited(Delimited { delim, tts }) => {
                expanded.push(TokenTree::Delimited(Delimited {
                    delim: delim,
                    tts: expand_tts(tts),
                }))
            }
            other => expanded.push(other),
        }
    }
    expanded
}

/// Parses a token tree corresponding to the `match_byte` macro.
///
/// ## Example
///
/// ```rust
/// match_byte! { tokenizer.next_byte_unchecked(),
///     b'a'..b'z' => { ... }
///     b'0'..b'9' => { ... }
///     b'\n' | b'\\' => { ... }
///     foo => { ... }
/// }
///
/// Returns:
///  * The token tree that contains the expression to be matched (in this case
///    `tokenizer.next_byte_unchecked()`.
///
///  * The table with the different cases per byte, each entry in the table
///    contains a non-zero integer representing a different arm of the
///    match expression.
///
///  * The list of cases containing the expansion of the arms of the match
///    expression.
///
///  * An optional identifier to which the wildcard pattern is matched (`foo` in
///    this case).
///
fn parse_match_bytes_macro(tts: Vec<syn::TokenTree>) -> (Vec<syn::TokenTree>, [u8; 256], Vec<Case>, Option<syn::Ident>) {
    let mut tts = tts.into_iter();

    // Grab the thing we're matching, until we find a comma.
    let mut left_hand_side = vec![];
    loop {
        match tts.next() {
            Some(syn::TokenTree::Token(syn::Token::Comma)) => break,
            Some(other) => left_hand_side.push(other),
            None => panic!("Expected not to run out of tokens looking for a comma"),
        }
    }

    let mut cases = vec![];
    let mut table = [0u8; 256];

    let mut tts = tts.peekable();
    let mut case_id: u8 = 1;
    let mut binding = None;
    while tts.len() > 0 {
        cases.push(parse_case(&mut tts, &mut table, &mut binding, case_id));

        // Allow an optional comma between cases.
        match tts.peek() {
            Some(&syn::TokenTree::Token(syn::Token::Comma)) => {
                tts.next();
            },
            _ => {},
        }

        case_id += 1;
    }

    (left_hand_side, table, cases, binding)
}

#[derive(Debug)]
struct Case(Vec<syn::TokenTree>);

/// Parses a single pattern => expression, and returns the case, filling in the
/// table with the case id for every byte that matched.
///
/// The `binding` parameter is the identifier that is used by the wildcard
/// pattern.
fn parse_case(tts: &mut iter::Peekable<vec::IntoIter<syn::TokenTree>>,
              table: &mut [u8; 256],
              binding: &mut Option<syn::Ident>,
              case_id: u8)
              -> Case {
    // The last byte checked, as part of this pattern, to properly detect
    // ranges.
    let mut last_byte: Option<u8> = None;

    // Loop through the pattern filling with bytes the table.
    loop {
        match tts.next() {
            Some(syn::TokenTree::Token(syn::Token::Literal(syn::Lit::Byte(byte)))) => {
                table[byte as usize] = case_id;
                last_byte = Some(byte);
            }
            Some(syn::TokenTree::Token(syn::Token::BinOp(syn::BinOpToken::Or))) => {
                last_byte = None; // This pattern is over.
            },
            Some(syn::TokenTree::Token(syn::Token::DotDotDot)) => {
                assert!(last_byte.is_some(), "Expected closed range!");
                match tts.next() {
                    Some(syn::TokenTree::Token(syn::Token::Literal(syn::Lit::Byte(byte)))) => {
                        for b in last_byte.take().unwrap()..byte {
                            if table[b as usize] == 0 {
                                table[b as usize] = case_id;
                            }
                        }
                        if table[byte as usize] == 0 {
                            table[byte as usize] = case_id;
                        }
                    }
                    other => panic!("Expected closed range, got: {:?}", other),
                }
            },
            Some(syn::TokenTree::Token(syn::Token::FatArrow)) => break,
            Some(syn::TokenTree::Token(syn::Token::Ident(ident))) => {
                assert_eq!(last_byte, None, "I don't support ranges with identifiers!");
                assert_eq!(*binding, None);
                for mut byte in table.iter_mut() {
                    if *byte == 0 {
                        *byte = case_id;
                    }
                }
                *binding = Some(ident)
            }
            Some(syn::TokenTree::Token(syn::Token::Underscore)) => {
                assert_eq!(last_byte, None);
                for mut byte in table.iter_mut() {
                    if *byte == 0 {
                        *byte = case_id;
                    }
                }
            },
            other => panic!("Expected literal byte, got: {:?}", other),
        }
    }

    match tts.next() {
        Some(syn::TokenTree::Delimited(syn::Delimited { delim: syn::DelimToken::Brace, tts })) => {
            Case(tts)
        }
        other => panic!("Expected case with braces after fat arrow, got: {:?}", other),
    }
}

fn expand_match_bytes_macro(to_be_matched: Vec<syn::TokenTree>,
                            table: &[u8; 256],
                            cases: Vec<Case>,
                            binding: Option<syn::Ident>)
                            -> String {
    use std::fmt::Write;

    assert!(!to_be_matched.is_empty());
    assert!(table.iter().all(|b| *b != 0), "Incomplete pattern? Bogus code!");

    // We build the expression with text since it's easier.
    let mut expr = "{\n".to_owned();
    expr.push_str("enum Case {\n");
    for (i, _) in cases.iter().enumerate() {
        write!(&mut expr, "Case{} = {},", i + 1, i + 1).unwrap();
    }
    expr.push_str("}\n"); // enum Case

    expr.push_str("static __CASES: [Case; 256] = [");
    for byte in table.iter() {
        write!(&mut expr, "Case::Case{}, ", *byte).unwrap();
    }
    expr.push_str("];\n");

    let mut tokens = Tokens::new();
    let to_be_matched = syn::Delimited {
        delim: if binding.is_some() { syn::DelimToken::Brace } else { syn::DelimToken::Paren },
        tts: to_be_matched
    };
    to_be_matched.to_tokens(&mut tokens);

    if let Some(ref binding) = binding {
        write!(&mut expr, "let {} = {};\n", binding.to_string(), tokens.as_str()).unwrap();
    }

    write!(&mut expr, "match __CASES[{} as usize] {{", match binding {
        Some(binding) => binding.to_string(),
        None => tokens.to_string(),
    }).unwrap();

    for (i, case) in cases.into_iter().enumerate() {
        let mut case_tokens = Tokens::new();
        let case = syn::Delimited {
            delim: syn::DelimToken::Brace,
            tts: case.0
        };
        case.to_tokens(&mut case_tokens);
        write!(&mut expr, "Case::Case{} => {},\n", i + 1, case_tokens.as_str()).unwrap();
    }
    expr.push_str("}\n"); // match

    expr.push_str("}\n"); // top

    expr
}
