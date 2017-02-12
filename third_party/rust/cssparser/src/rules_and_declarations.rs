/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// https://drafts.csswg.org/css-syntax/#parsing

use std::ascii::AsciiExt;
use std::ops::Range;
use std::borrow::Cow;
use super::{Token, Parser, Delimiter, SourcePosition};


/// Parse `!important`.
///
/// Typical usage is `input.try(parse_important).is_ok()`
/// at the end of a `DeclarationParser::parse_value` implementation.
pub fn parse_important(input: &mut Parser) -> Result<(), ()> {
    try!(input.expect_delim('!'));
    input.expect_ident_matching("important")
}


/// The return value for `AtRuleParser::parse_prelude`.
/// Indicates whether the at-rule is expected to have a `{ /* ... */ }` block
/// or end with a `;` semicolon.
pub enum AtRuleType<P, R> {
    /// The at-rule is expected to end with a `;` semicolon. Example: `@import`.
    ///
    /// The value is the finished representation of an at-rule
    /// as returned by `RuleListParser::next` or `DeclarationListParser::next`.
    WithoutBlock(R),

    /// The at-rule is expected to have a a `{ /* ... */ }` block. Example: `@media`
    ///
    /// The value is the representation of the "prelude" part of the rule.
    WithBlock(P),

    /// The at-rule may either have a block or end with a semicolon.
    ///
    /// This is mostly for testing. As of this writing no real CSS at-rule behaves like this.
    ///
    /// The value is the representation of the "prelude" part of the rule.
    OptionalBlock(P),
}


/// A trait to provide various parsing of declaration values.
///
/// For example, there could be different implementations for property declarations in style rules
/// and for descriptors in `@font-face` rules.
pub trait DeclarationParser {
    /// The finished representation of a declaration.
    type Declaration;

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
    /// `input.try(parse_important).is_ok()` should be used at the end
    /// of the implementation of this method and the result should be part of the return value.
    fn parse_value(&mut self, name: &str, input: &mut Parser) -> Result<Self::Declaration, ()>;
}


/// A trait to provide various parsing of at-rules.
///
/// For example, there could be different implementations for top-level at-rules
/// (`@media`, `@font-face`, …)
/// and for page-margin rules inside `@page`.
///
/// Default implementations that reject all at-rules are provided,
/// so that `impl AtRuleParser<(), ()> for ... {}` can be used
/// for using `DeclarationListParser` to parse a declartions list with only qualified rules.
pub trait AtRuleParser {
    /// The intermediate representation of an at-rule prelude.
    type Prelude;

    /// The finished representation of an at-rule.
    type AtRule;

    /// Parse the prelude of an at-rule with the given `name`.
    ///
    /// Return the representation of the prelude and the type of at-rule,
    /// or `Err(())` to ignore the entire at-rule as invalid.
    ///
    /// See `AtRuleType`’s documentation for the return value.
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
    fn parse_prelude(&mut self, name: &str, input: &mut Parser)
                     -> Result<AtRuleType<Self::Prelude, Self::AtRule>, ()> {
        let _ = name;
        let _ = input;
        Err(())
    }

    /// Parse the content of a `{ /* ... */ }` block for the body of the at-rule.
    ///
    /// Return the finished representation of the at-rule
    /// as returned by `RuleListParser::next` or `DeclarationListParser::next`,
    /// or `Err(())` to ignore the entire at-rule as invalid.
    ///
    /// This is only called when `parse_prelude` returned `WithBlock` or `OptionalBlock`,
    /// and a block was indeed found following the prelude.
    fn parse_block(&mut self, prelude: Self::Prelude, input: &mut Parser)
                   -> Result<Self::AtRule, ()> {
        let _ = prelude;
        let _ = input;
        Err(())
    }

    /// An `OptionalBlock` prelude was followed by `;`.
    ///
    /// Convert the prelude into the finished representation of the at-rule
    /// as returned by `RuleListParser::next` or `DeclarationListParser::next`.
    fn rule_without_block(&mut self, prelude: Self::Prelude) -> Self::AtRule {
        let _ = prelude;
        panic!("The `AtRuleParser::rule_without_block` method must be overriden \
                if `AtRuleParser::parse_prelude` ever returns `AtRuleType::OptionalBlock`.")
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
pub trait QualifiedRuleParser {
    /// The intermediate representation of a qualified rule prelude.
    type Prelude;

    /// The finished representation of a qualified rule.
    type QualifiedRule;

    /// Parse the prelude of a qualified rule. For style rules, this is as Selector list.
    ///
    /// Return the representation of the prelude,
    /// or `Err(())` to ignore the entire at-rule as invalid.
    ///
    /// The prelude is the part before the `{ /* ... */ }` block.
    ///
    /// The given `input` is a "delimited" parser
    /// that ends where the prelude should end (before the next `{`).
    fn parse_prelude(&mut self, input: &mut Parser) -> Result<Self::Prelude, ()> {
        let _ = input;
        Err(())
    }

    /// Parse the content of a `{ /* ... */ }` block for the body of the qualified rule.
    ///
    /// Return the finished representation of the qualified rule
    /// as returned by `RuleListParser::next`,
    /// or `Err(())` to ignore the entire at-rule as invalid.
    fn parse_block(&mut self, prelude: Self::Prelude, input: &mut Parser)
                   -> Result<Self::QualifiedRule, ()> {
        let _ = prelude;
        let _ = input;
        Err(())
    }
}


/// Provides an iterator for declaration list parsing.
pub struct DeclarationListParser<'i: 't, 't: 'a, 'a, P> {
    /// The input given to `DeclarationListParser::new`
    pub input: &'a mut Parser<'i, 't>,

    /// The parser given to `DeclarationListParser::new`
    pub parser: P,
}


impl<'i, 't, 'a, I, P> DeclarationListParser<'i, 't, 'a, P>
where P: DeclarationParser<Declaration = I> + AtRuleParser<AtRule = I> {
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
impl<'i, 't, 'a, I, P> Iterator for DeclarationListParser<'i, 't, 'a, P>
where P: DeclarationParser<Declaration = I> + AtRuleParser<AtRule = I> {
    type Item = Result<I, Range<SourcePosition>>;

    fn next(&mut self) -> Option<Result<I, Range<SourcePosition>>> {
        loop {
            let start_position = self.input.position();
            match self.input.next_including_whitespace_and_comments() {
                Ok(Token::WhiteSpace(_)) | Ok(Token::Comment(_)) | Ok(Token::Semicolon) => {}
                Ok(Token::Ident(name)) => {
                    return Some({
                        let parser = &mut self.parser;
                        self.input.parse_until_after(Delimiter::Semicolon, |input| {
                            try!(input.expect_colon());
                            parser.parse_value(&*name, input)
                        })
                    }.map_err(|()| start_position..self.input.position()))
                }
                Ok(Token::AtKeyword(name)) => {
                    return Some(parse_at_rule(start_position, name, self.input, &mut self.parser))
                }
                Ok(_) => {
                    return Some(self.input.parse_until_after(Delimiter::Semicolon, |_| Err(()))
                                .map_err(|()| start_position..self.input.position()))
                }
                Err(()) => return None,
            }
        }
    }
}


/// Provides an iterator for rule list parsing.
pub struct RuleListParser<'i: 't, 't: 'a, 'a, P> {
    /// The input given to `RuleListParser::new`
    pub input: &'a mut Parser<'i, 't>,

    /// The parser given to `RuleListParser::new`
    pub parser: P,

    is_stylesheet: bool,
    any_rule_so_far: bool,
}


impl<'i: 't, 't: 'a, 'a, R, P> RuleListParser<'i, 't, 'a, P>
where P: QualifiedRuleParser<QualifiedRule = R> + AtRuleParser<AtRule = R> {
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
    /// (This is to deal with legacy work arounds for `<style>` HTML element parsing.)
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
impl<'i, 't, 'a, R, P> Iterator for RuleListParser<'i, 't, 'a, P>
where P: QualifiedRuleParser<QualifiedRule = R> + AtRuleParser<AtRule = R> {
    type Item = Result<R, Range<SourcePosition>>;

    fn next(&mut self) -> Option<Result<R, Range<SourcePosition>>> {
        loop {
            let start_position = self.input.position();
            match self.input.next_including_whitespace_and_comments() {
                Ok(Token::WhiteSpace(_)) | Ok(Token::Comment(_)) => {}
                Ok(Token::CDO) | Ok(Token::CDC) if self.is_stylesheet => {}
                Ok(Token::AtKeyword(name)) => {
                    let first_stylesheet_rule = self.is_stylesheet && !self.any_rule_so_far;
                    self.any_rule_so_far = true;
                    if first_stylesheet_rule && name.eq_ignore_ascii_case("charset") {
                        let delimiters = Delimiter::Semicolon | Delimiter::CurlyBracketBlock;
                        let _ = self.input.parse_until_after(delimiters, |_input| Ok(()));
                    } else {
                        return Some(parse_at_rule(start_position, name, self.input, &mut self.parser))
                    }
                }
                Ok(_) => {
                    self.any_rule_so_far = true;
                    self.input.reset(start_position);
                    return Some(parse_qualified_rule(self.input, &mut self.parser)
                                .map_err(|()| start_position..self.input.position()))
                }
                Err(()) => return None,
            }
        }
    }
}


/// Parse a single declaration, such as an `( /* ... */ )` parenthesis in an `@supports` prelude.
pub fn parse_one_declaration<P>(input: &mut Parser, parser: &mut P)
                                -> Result<<P as DeclarationParser>::Declaration,
                                          Range<SourcePosition>>
                                where P: DeclarationParser {
    let start_position = input.position();
    input.parse_entirely(|input| {
        let name = try!(input.expect_ident());
        try!(input.expect_colon());
        parser.parse_value(&*name, input)
    }).map_err(|()| start_position..input.position())
}


/// Parse a single rule, such as for CSSOM’s `CSSStyleSheet.insertRule`.
pub fn parse_one_rule<R, P>(input: &mut Parser, parser: &mut P) -> Result<R, ()>
where P: QualifiedRuleParser<QualifiedRule = R> + AtRuleParser<AtRule = R> {
    input.parse_entirely(|input| {
        loop {
            let start_position = input.position();
            match try!(input.next_including_whitespace_and_comments()) {
                Token::WhiteSpace(_) | Token::Comment(_) => {}
                Token::AtKeyword(name) => {
                    return parse_at_rule(start_position, name, input, parser).map_err(|_| ())
                }
                _ => {
                    input.reset(start_position);
                    return parse_qualified_rule(input, parser).map_err(|_| ())
                }
            }
        }
    })
}


fn parse_at_rule<P>(start_position: SourcePosition, name: Cow<str>,
                    input: &mut Parser, parser: &mut P)
                    -> Result<<P as AtRuleParser>::AtRule, Range<SourcePosition>>
                    where P: AtRuleParser {
    let delimiters = Delimiter::Semicolon | Delimiter::CurlyBracketBlock;
    let result = input.parse_until_before(delimiters, |input| {
        parser.parse_prelude(&*name, input)
    });
    match result {
        Ok(AtRuleType::WithoutBlock(rule)) => {
            match input.next() {
                Ok(Token::Semicolon) | Err(()) => Ok(rule),
                Ok(Token::CurlyBracketBlock) => Err(start_position..input.position()),
                Ok(_) => unreachable!()
            }
        }
        Ok(AtRuleType::WithBlock(prelude)) => {
            match input.next() {
                Ok(Token::CurlyBracketBlock) => {
                    input.parse_nested_block(move |input| parser.parse_block(prelude, input))
                    .map_err(|()| start_position..input.position())
                }
                Ok(Token::Semicolon) | Err(()) => Err(start_position..input.position()),
                Ok(_) => unreachable!()
            }
        }
        Ok(AtRuleType::OptionalBlock(prelude)) => {
            match input.next() {
                Ok(Token::Semicolon) | Err(()) => Ok(parser.rule_without_block(prelude)),
                Ok(Token::CurlyBracketBlock) => {
                    input.parse_nested_block(move |input| parser.parse_block(prelude, input))
                    .map_err(|()| start_position..input.position())
                }
                _ => unreachable!()
            }
        }
        Err(()) => {
            let end_position = input.position();
            match input.next() {
                Ok(Token::CurlyBracketBlock) | Ok(Token::Semicolon) | Err(()) => {}
                _ => unreachable!()
            }
            Err(start_position..end_position)
        }
    }
}


fn parse_qualified_rule<P>(input: &mut Parser, parser: &mut P)
                           -> Result<<P as QualifiedRuleParser>::QualifiedRule, ()>
                           where P: QualifiedRuleParser {
    let prelude = input.parse_until_before(Delimiter::CurlyBracketBlock, |input| {
        parser.parse_prelude(input)
    });
    match try!(input.next()) {
        Token::CurlyBracketBlock => {
            // Do this here so that we consume the `{` even if the prelude is `Err`.
            let prelude = try!(prelude);
            input.parse_nested_block(move |input| parser.parse_block(prelude, input))
        }
        _ => unreachable!()
    }
}
