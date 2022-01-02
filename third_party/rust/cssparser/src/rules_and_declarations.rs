/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// https://drafts.csswg.org/css-syntax/#parsing

use super::{BasicParseError, BasicParseErrorKind, Delimiter};
use super::{ParseError, Parser, Token};
use crate::cow_rc_str::CowRcStr;
use crate::parser::{parse_nested_block, parse_until_after, parse_until_before, ParserState};

/// Parse `!important`.
///
/// Typical usage is `input.try_parse(parse_important).is_ok()`
/// at the end of a `DeclarationParser::parse_value` implementation.
pub fn parse_important<'i, 't>(input: &mut Parser<'i, 't>) -> Result<(), BasicParseError<'i>> {
    input.expect_delim('!')?;
    input.expect_ident_matching("important")
}

/// A trait to provide various parsing of declaration values.
///
/// For example, there could be different implementations for property declarations in style rules
/// and for descriptors in `@font-face` rules.
pub trait DeclarationParser<'i> {
    /// The finished representation of a declaration.
    type Declaration;

    /// The error type that is included in the ParseError value that can be returned.
    type Error: 'i;

    /// Parse the value of a declaration with the given `name`.
    ///
    /// Return the finished representation for the declaration
    /// as returned by `DeclarationListParser::next`,
    /// or `Err(())` to ignore the entire declaration as invalid.
    ///
    /// Declaration name matching should be case-insensitive in the ASCII range.
    /// This can be done with `std::ascii::Ascii::eq_ignore_ascii_case`,
    /// or with the `match_ignore_ascii_case!` macro.
    ///
    /// The given `input` is a "delimited" parser
    /// that ends wherever the declaration value should end.
    /// (In declaration lists, before the next semicolon or end of the current block.)
    ///
    /// If `!important` can be used in a given context,
    /// `input.try_parse(parse_important).is_ok()` should be used at the end
    /// of the implementation of this method and the result should be part of the return value.
    fn parse_value<'t>(
        &mut self,
        name: CowRcStr<'i>,
        input: &mut Parser<'i, 't>,
    ) -> Result<Self::Declaration, ParseError<'i, Self::Error>>;
}

/// A trait to provide various parsing of at-rules.
///
/// For example, there could be different implementations for top-level at-rules
/// (`@media`, `@font-face`, …)
/// and for page-margin rules inside `@page`.
///
/// Default implementations that reject all at-rules are provided,
/// so that `impl AtRuleParser<(), ()> for ... {}` can be used
/// for using `DeclarationListParser` to parse a declarations list with only qualified rules.
pub trait AtRuleParser<'i> {
    /// The intermediate representation of prelude of an at-rule.
    type Prelude;

    /// The finished representation of an at-rule.
    type AtRule;

    /// The error type that is included in the ParseError value that can be returned.
    type Error: 'i;

    /// Parse the prelude of an at-rule with the given `name`.
    ///
    /// Return the representation of the prelude and the type of at-rule,
    /// or `Err(())` to ignore the entire at-rule as invalid.
    ///
    /// The prelude is the part after the at-keyword
    /// and before the `;` semicolon or `{ /* ... */ }` block.
    ///
    /// At-rule name matching should be case-insensitive in the ASCII range.
    /// This can be done with `std::ascii::Ascii::eq_ignore_ascii_case`,
    /// or with the `match_ignore_ascii_case!` macro.
    ///
    /// The given `input` is a "delimited" parser
    /// that ends wherever the prelude should end.
    /// (Before the next semicolon, the next `{`, or the end of the current block.)
    fn parse_prelude<'t>(
        &mut self,
        name: CowRcStr<'i>,
        input: &mut Parser<'i, 't>,
    ) -> Result<Self::Prelude, ParseError<'i, Self::Error>> {
        let _ = name;
        let _ = input;
        Err(input.new_error(BasicParseErrorKind::AtRuleInvalid(name)))
    }

    /// End an at-rule which doesn't have block. Return the finished
    /// representation of the at-rule.
    ///
    /// The location passed in is source location of the start of the prelude.
    ///
    /// This is only called when `parse_prelude` returned `WithoutBlock`, and
    /// either the `;` semicolon indeed follows the prelude, or parser is at
    /// the end of the input.
    fn rule_without_block(
        &mut self,
        prelude: Self::Prelude,
        start: &ParserState,
    ) -> Result<Self::AtRule, ()> {
        let _ = prelude;
        let _ = start;
        Err(())
    }

    /// Parse the content of a `{ /* ... */ }` block for the body of the at-rule.
    ///
    /// The location passed in is source location of the start of the prelude.
    ///
    /// Return the finished representation of the at-rule
    /// as returned by `RuleListParser::next` or `DeclarationListParser::next`,
    /// or `Err(())` to ignore the entire at-rule as invalid.
    ///
    /// This is only called when `parse_prelude` returned `WithBlock`, and a block
    /// was indeed found following the prelude.
    fn parse_block<'t>(
        &mut self,
        prelude: Self::Prelude,
        start: &ParserState,
        input: &mut Parser<'i, 't>,
    ) -> Result<Self::AtRule, ParseError<'i, Self::Error>> {
        let _ = prelude;
        let _ = start;
        let _ = input;
        Err(input.new_error(BasicParseErrorKind::AtRuleBodyInvalid))
    }
}

/// A trait to provide various parsing of qualified rules.
///
/// For example, there could be different implementations
/// for top-level qualified rules (i.e. style rules with Selectors as prelude)
/// and for qualified rules inside `@keyframes` (keyframe rules with keyframe selectors as prelude).
///
/// Default implementations that reject all qualified rules are provided,
/// so that `impl QualifiedRuleParser<(), ()> for ... {}` can be used
/// for example for using `RuleListParser` to parse a rule list with only at-rules
/// (such as inside `@font-feature-values`).
pub trait QualifiedRuleParser<'i> {
    /// The intermediate representation of a qualified rule prelude.
    type Prelude;

    /// The finished representation of a qualified rule.
    type QualifiedRule;

    /// The error type that is included in the ParseError value that can be returned.
    type Error: 'i;

    /// Parse the prelude of a qualified rule. For style rules, this is as Selector list.
    ///
    /// Return the representation of the prelude,
    /// or `Err(())` to ignore the entire at-rule as invalid.
    ///
    /// The prelude is the part before the `{ /* ... */ }` block.
    ///
    /// The given `input` is a "delimited" parser
    /// that ends where the prelude should end (before the next `{`).
    fn parse_prelude<'t>(
        &mut self,
        input: &mut Parser<'i, 't>,
    ) -> Result<Self::Prelude, ParseError<'i, Self::Error>> {
        let _ = input;
        Err(input.new_error(BasicParseErrorKind::QualifiedRuleInvalid))
    }

    /// Parse the content of a `{ /* ... */ }` block for the body of the qualified rule.
    ///
    /// The location passed in is source location of the start of the prelude.
    ///
    /// Return the finished representation of the qualified rule
    /// as returned by `RuleListParser::next`,
    /// or `Err(())` to ignore the entire at-rule as invalid.
    fn parse_block<'t>(
        &mut self,
        prelude: Self::Prelude,
        start: &ParserState,
        input: &mut Parser<'i, 't>,
    ) -> Result<Self::QualifiedRule, ParseError<'i, Self::Error>> {
        let _ = prelude;
        let _ = start;
        let _ = input;
        Err(input.new_error(BasicParseErrorKind::QualifiedRuleInvalid))
    }
}

/// Provides an iterator for declaration list parsing.
pub struct DeclarationListParser<'i, 't, 'a, P> {
    /// The input given to `DeclarationListParser::new`
    pub input: &'a mut Parser<'i, 't>,

    /// The parser given to `DeclarationListParser::new`
    pub parser: P,
}

impl<'i, 't, 'a, I, P, E: 'i> DeclarationListParser<'i, 't, 'a, P>
where
    P: DeclarationParser<'i, Declaration = I, Error = E> + AtRuleParser<'i, AtRule = I, Error = E>,
{
    /// Create a new `DeclarationListParser` for the given `input` and `parser`.
    ///
    /// Note that all CSS declaration lists can on principle contain at-rules.
    /// Even if no such valid at-rule exists (yet),
    /// this affects error handling: at-rules end at `{}` blocks, not just semicolons.
    ///
    /// The given `parser` therefore needs to implement
    /// both `DeclarationParser` and `AtRuleParser` traits.
    /// However, the latter can be an empty `impl`
    /// since `AtRuleParser` provides default implementations of its methods.
    ///
    /// The return type for finished declarations and at-rules also needs to be the same,
    /// since `<DeclarationListParser as Iterator>::next` can return either.
    /// It could be a custom enum.
    pub fn new(input: &'a mut Parser<'i, 't>, parser: P) -> Self {
        DeclarationListParser {
            input: input,
            parser: parser,
        }
    }
}

/// `DeclarationListParser` is an iterator that yields `Ok(_)` for a valid declaration or at-rule
/// or `Err(())` for an invalid one.
impl<'i, 't, 'a, I, P, E: 'i> Iterator for DeclarationListParser<'i, 't, 'a, P>
where
    P: DeclarationParser<'i, Declaration = I, Error = E> + AtRuleParser<'i, AtRule = I, Error = E>,
{
    type Item = Result<I, (ParseError<'i, E>, &'i str)>;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            let start = self.input.state();
            match self.input.next_including_whitespace_and_comments() {
                Ok(&Token::WhiteSpace(_)) | Ok(&Token::Comment(_)) | Ok(&Token::Semicolon) => {
                    continue
                }
                Ok(&Token::Ident(ref name)) => {
                    let name = name.clone();
                    let result = {
                        let parser = &mut self.parser;
                        // FIXME: https://github.com/servo/rust-cssparser/issues/254
                        let callback = |input: &mut Parser<'i, '_>| {
                            input.expect_colon()?;
                            parser.parse_value(name, input)
                        };
                        parse_until_after(self.input, Delimiter::Semicolon, callback)
                    };
                    return Some(result.map_err(|e| (e, self.input.slice_from(start.position()))));
                }
                Ok(&Token::AtKeyword(ref name)) => {
                    let name = name.clone();
                    return Some(parse_at_rule(&start, name, self.input, &mut self.parser));
                }
                Ok(token) => {
                    let token = token.clone();
                    let result = self.input.parse_until_after(Delimiter::Semicolon, |_| {
                        Err(start.source_location().new_unexpected_token_error(token))
                    });
                    return Some(result.map_err(|e| (e, self.input.slice_from(start.position()))));
                }
                Err(..) => return None,
            }
        }
    }
}

/// Provides an iterator for rule list parsing.
pub struct RuleListParser<'i, 't, 'a, P> {
    /// The input given to `RuleListParser::new`
    pub input: &'a mut Parser<'i, 't>,

    /// The parser given to `RuleListParser::new`
    pub parser: P,

    is_stylesheet: bool,
    any_rule_so_far: bool,
}

impl<'i, 't, 'a, R, P, E: 'i> RuleListParser<'i, 't, 'a, P>
where
    P: QualifiedRuleParser<'i, QualifiedRule = R, Error = E>
        + AtRuleParser<'i, AtRule = R, Error = E>,
{
    /// Create a new `RuleListParser` for the given `input` at the top-level of a stylesheet
    /// and the given `parser`.
    ///
    /// The given `parser` needs to implement both `QualifiedRuleParser` and `AtRuleParser` traits.
    /// However, either of them can be an empty `impl`
    /// since the traits provide default implementations of their methods.
    ///
    /// The return type for finished qualified rules and at-rules also needs to be the same,
    /// since `<RuleListParser as Iterator>::next` can return either.
    /// It could be a custom enum.
    pub fn new_for_stylesheet(input: &'a mut Parser<'i, 't>, parser: P) -> Self {
        RuleListParser {
            input: input,
            parser: parser,
            is_stylesheet: true,
            any_rule_so_far: false,
        }
    }

    /// Same is `new_for_stylesheet`, but should be used for rule lists inside a block
    /// such as the body of an `@media` rule.
    ///
    /// This differs in that `<!--` and `-->` tokens
    /// should only be ignored at the stylesheet top-level.
    /// (This is to deal with legacy workarounds for `<style>` HTML element parsing.)
    pub fn new_for_nested_rule(input: &'a mut Parser<'i, 't>, parser: P) -> Self {
        RuleListParser {
            input: input,
            parser: parser,
            is_stylesheet: false,
            any_rule_so_far: false,
        }
    }
}

/// `RuleListParser` is an iterator that yields `Ok(_)` for a rule or `Err(())` for an invalid one.
impl<'i, 't, 'a, R, P, E: 'i> Iterator for RuleListParser<'i, 't, 'a, P>
where
    P: QualifiedRuleParser<'i, QualifiedRule = R, Error = E>
        + AtRuleParser<'i, AtRule = R, Error = E>,
{
    type Item = Result<R, (ParseError<'i, E>, &'i str)>;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if self.is_stylesheet {
                self.input.skip_cdc_and_cdo()
            } else {
                self.input.skip_whitespace()
            }
            let start = self.input.state();

            let at_keyword = match self.input.next_byte()? {
                b'@' => match self.input.next_including_whitespace_and_comments() {
                    Ok(&Token::AtKeyword(ref name)) => Some(name.clone()),
                    _ => {
                        self.input.reset(&start);
                        None
                    }
                },
                _ => None,
            };

            if let Some(name) = at_keyword {
                let first_stylesheet_rule = self.is_stylesheet && !self.any_rule_so_far;
                self.any_rule_so_far = true;
                if first_stylesheet_rule && name.eq_ignore_ascii_case("charset") {
                    let delimiters = Delimiter::Semicolon | Delimiter::CurlyBracketBlock;
                    let _: Result<(), ParseError<()>> =
                        self.input.parse_until_after(delimiters, |_| Ok(()));
                } else {
                    return Some(parse_at_rule(
                        &start,
                        name.clone(),
                        self.input,
                        &mut self.parser,
                    ));
                }
            } else {
                self.any_rule_so_far = true;
                let result = parse_qualified_rule(self.input, &mut self.parser);
                return Some(result.map_err(|e| (e, self.input.slice_from(start.position()))));
            }
        }
    }
}

/// Parse a single declaration, such as an `( /* ... */ )` parenthesis in an `@supports` prelude.
pub fn parse_one_declaration<'i, 't, P, E>(
    input: &mut Parser<'i, 't>,
    parser: &mut P,
) -> Result<<P as DeclarationParser<'i>>::Declaration, (ParseError<'i, E>, &'i str)>
where
    P: DeclarationParser<'i, Error = E>,
{
    let start_position = input.position();
    input
        .parse_entirely(|input| {
            let name = input.expect_ident()?.clone();
            input.expect_colon()?;
            parser.parse_value(name, input)
        })
        .map_err(|e| (e, input.slice_from(start_position)))
}

/// Parse a single rule, such as for CSSOM’s `CSSStyleSheet.insertRule`.
pub fn parse_one_rule<'i, 't, R, P, E>(
    input: &mut Parser<'i, 't>,
    parser: &mut P,
) -> Result<R, ParseError<'i, E>>
where
    P: QualifiedRuleParser<'i, QualifiedRule = R, Error = E>
        + AtRuleParser<'i, AtRule = R, Error = E>,
{
    input.parse_entirely(|input| {
        input.skip_whitespace();
        let start = input.state();
        let at_keyword = if input.next_byte() == Some(b'@') {
            match *input.next_including_whitespace_and_comments()? {
                Token::AtKeyword(ref name) => Some(name.clone()),
                _ => {
                    input.reset(&start);
                    None
                }
            }
        } else {
            None
        };

        if let Some(name) = at_keyword {
            parse_at_rule(&start, name, input, parser).map_err(|e| e.0)
        } else {
            parse_qualified_rule(input, parser)
        }
    })
}

fn parse_at_rule<'i, 't, P, E>(
    start: &ParserState,
    name: CowRcStr<'i>,
    input: &mut Parser<'i, 't>,
    parser: &mut P,
) -> Result<<P as AtRuleParser<'i>>::AtRule, (ParseError<'i, E>, &'i str)>
where
    P: AtRuleParser<'i, Error = E>,
{
    let delimiters = Delimiter::Semicolon | Delimiter::CurlyBracketBlock;
    // FIXME: https://github.com/servo/rust-cssparser/issues/254
    let callback = |input: &mut Parser<'i, '_>| parser.parse_prelude(name, input);
    let result = parse_until_before(input, delimiters, callback);
    match result {
        Ok(prelude) => {
            let result = match input.next() {
                Ok(&Token::Semicolon) | Err(_) => {
                    parser.rule_without_block(prelude, start)
                        .map_err(|()| input.new_unexpected_token_error(Token::Semicolon))
                },
                Ok(&Token::CurlyBracketBlock) => {
                    // FIXME: https://github.com/servo/rust-cssparser/issues/254
                    let callback =
                        |input: &mut Parser<'i, '_>| parser.parse_block(prelude, start, input);
                    parse_nested_block(input, callback)
                },
                Ok(_) => unreachable!(),
            };
            result.map_err(|e| (e, input.slice_from(start.position())))
        },
        Err(error) => {
            let end_position = input.position();
            match input.next() {
                Ok(&Token::CurlyBracketBlock) | Ok(&Token::Semicolon) | Err(_) => {}
                _ => unreachable!(),
            };
            Err((error, input.slice(start.position()..end_position)))
        },
    }
}

fn parse_qualified_rule<'i, 't, P, E>(
    input: &mut Parser<'i, 't>,
    parser: &mut P,
) -> Result<<P as QualifiedRuleParser<'i>>::QualifiedRule, ParseError<'i, E>>
where
    P: QualifiedRuleParser<'i, Error = E>,
{
    let start = input.state();
    // FIXME: https://github.com/servo/rust-cssparser/issues/254
    let callback = |input: &mut Parser<'i, '_>| parser.parse_prelude(input);
    let prelude = parse_until_before(input, Delimiter::CurlyBracketBlock, callback);
    match *input.next()? {
        Token::CurlyBracketBlock => {
            // Do this here so that we consume the `{` even if the prelude is `Err`.
            let prelude = prelude?;
            // FIXME: https://github.com/servo/rust-cssparser/issues/254
            let callback = |input: &mut Parser<'i, '_>| parser.parse_block(prelude, &start, input);
            parse_nested_block(input, callback)
        }
        _ => unreachable!(),
    }
}
