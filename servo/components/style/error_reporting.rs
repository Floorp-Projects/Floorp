/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Types used to report parsing errors.

#![deny(missing_docs)]

use crate::selector_parser::SelectorImpl;
use crate::stylesheets::UrlExtraData;
use cssparser::{BasicParseErrorKind, ParseErrorKind, SourceLocation, Token};
use selectors::parser::{Component, RelativeSelector, Selector};
use selectors::visitor::{SelectorListKind, SelectorVisitor};
use selectors::SelectorList;
use std::fmt;
use style_traits::ParseError;

/// Errors that can be encountered while parsing CSS.
#[derive(Debug)]
pub enum ContextualParseError<'a> {
    /// A property declaration was not recognized.
    UnsupportedPropertyDeclaration(&'a str, ParseError<'a>, &'a [SelectorList<SelectorImpl>]),
    /// A property descriptor was not recognized.
    UnsupportedPropertyDescriptor(&'a str, ParseError<'a>),
    /// A font face descriptor was not recognized.
    UnsupportedFontFaceDescriptor(&'a str, ParseError<'a>),
    /// A font feature values descriptor was not recognized.
    UnsupportedFontFeatureValuesDescriptor(&'a str, ParseError<'a>),
    /// A font palette values descriptor was not recognized.
    UnsupportedFontPaletteValuesDescriptor(&'a str, ParseError<'a>),
    /// A keyframe rule was not valid.
    InvalidKeyframeRule(&'a str, ParseError<'a>),
    /// A font feature values rule was not valid.
    InvalidFontFeatureValuesRule(&'a str, ParseError<'a>),
    /// A keyframe property declaration was not recognized.
    UnsupportedKeyframePropertyDeclaration(&'a str, ParseError<'a>),
    /// A rule was invalid for some reason.
    InvalidRule(&'a str, ParseError<'a>),
    /// A rule was not recognized.
    UnsupportedRule(&'a str, ParseError<'a>),
    /// A viewport descriptor declaration was not recognized.
    UnsupportedViewportDescriptorDeclaration(&'a str, ParseError<'a>),
    /// A counter style descriptor declaration was not recognized.
    UnsupportedCounterStyleDescriptorDeclaration(&'a str, ParseError<'a>),
    /// A counter style rule had no symbols.
    InvalidCounterStyleWithoutSymbols(String),
    /// A counter style rule had less than two symbols.
    InvalidCounterStyleNotEnoughSymbols(String),
    /// A counter style rule did not have additive-symbols.
    InvalidCounterStyleWithoutAdditiveSymbols,
    /// A counter style rule had extends with symbols.
    InvalidCounterStyleExtendsWithSymbols,
    /// A counter style rule had extends with additive-symbols.
    InvalidCounterStyleExtendsWithAdditiveSymbols,
    /// A media rule was invalid for some reason.
    InvalidMediaRule(&'a str, ParseError<'a>),
    /// A value was not recognized.
    UnsupportedValue(&'a str, ParseError<'a>),
    /// A never-matching `:host` selector was found.
    NeverMatchingHostSelector(String),
}

impl<'a> fmt::Display for ContextualParseError<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fn token_to_str(t: &Token, f: &mut fmt::Formatter) -> fmt::Result {
            match *t {
                Token::Ident(ref i) => write!(f, "identifier {}", i),
                Token::AtKeyword(ref kw) => write!(f, "keyword @{}", kw),
                Token::Hash(ref h) => write!(f, "hash #{}", h),
                Token::IDHash(ref h) => write!(f, "id selector #{}", h),
                Token::QuotedString(ref s) => write!(f, "quoted string \"{}\"", s),
                Token::UnquotedUrl(ref u) => write!(f, "url {}", u),
                Token::Delim(ref d) => write!(f, "delimiter {}", d),
                Token::Number {
                    int_value: Some(i), ..
                } => write!(f, "number {}", i),
                Token::Number { value, .. } => write!(f, "number {}", value),
                Token::Percentage {
                    int_value: Some(i), ..
                } => write!(f, "percentage {}", i),
                Token::Percentage { unit_value, .. } => {
                    write!(f, "percentage {}", unit_value * 100.)
                },
                Token::Dimension {
                    value, ref unit, ..
                } => write!(f, "dimension {}{}", value, unit),
                Token::WhiteSpace(_) => write!(f, "whitespace"),
                Token::Comment(_) => write!(f, "comment"),
                Token::Colon => write!(f, "colon (:)"),
                Token::Semicolon => write!(f, "semicolon (;)"),
                Token::Comma => write!(f, "comma (,)"),
                Token::IncludeMatch => write!(f, "include match (~=)"),
                Token::DashMatch => write!(f, "dash match (|=)"),
                Token::PrefixMatch => write!(f, "prefix match (^=)"),
                Token::SuffixMatch => write!(f, "suffix match ($=)"),
                Token::SubstringMatch => write!(f, "substring match (*=)"),
                Token::CDO => write!(f, "CDO (<!--)"),
                Token::CDC => write!(f, "CDC (-->)"),
                Token::Function(ref name) => write!(f, "function {}", name),
                Token::ParenthesisBlock => write!(f, "parenthesis ("),
                Token::SquareBracketBlock => write!(f, "square bracket ["),
                Token::CurlyBracketBlock => write!(f, "curly bracket {{"),
                Token::BadUrl(ref _u) => write!(f, "bad url parse error"),
                Token::BadString(ref _s) => write!(f, "bad string parse error"),
                Token::CloseParenthesis => write!(f, "unmatched close parenthesis"),
                Token::CloseSquareBracket => write!(f, "unmatched close square bracket"),
                Token::CloseCurlyBracket => write!(f, "unmatched close curly bracket"),
            }
        }

        fn parse_error_to_str(err: &ParseError, f: &mut fmt::Formatter) -> fmt::Result {
            match err.kind {
                ParseErrorKind::Basic(BasicParseErrorKind::UnexpectedToken(ref t)) => {
                    write!(f, "found unexpected ")?;
                    token_to_str(t, f)
                },
                ParseErrorKind::Basic(BasicParseErrorKind::EndOfInput) => {
                    write!(f, "unexpected end of input")
                },
                ParseErrorKind::Basic(BasicParseErrorKind::AtRuleInvalid(ref i)) => {
                    write!(f, "@ rule invalid: {}", i)
                },
                ParseErrorKind::Basic(BasicParseErrorKind::AtRuleBodyInvalid) => {
                    write!(f, "@ rule invalid")
                },
                ParseErrorKind::Basic(BasicParseErrorKind::QualifiedRuleInvalid) => {
                    write!(f, "qualified rule invalid")
                },
                ParseErrorKind::Custom(ref err) => write!(f, "{:?}", err),
            }
        }

        match *self {
            ContextualParseError::UnsupportedPropertyDeclaration(decl, ref err, _selectors) => {
                write!(f, "Unsupported property declaration: '{}', ", decl)?;
                parse_error_to_str(err, f)
            },
            ContextualParseError::UnsupportedPropertyDescriptor(decl, ref err) => {
                write!(
                    f,
                    "Unsupported @property descriptor declaration: '{}', ",
                    decl
                )?;
                parse_error_to_str(err, f)
            },
            ContextualParseError::UnsupportedFontFaceDescriptor(decl, ref err) => {
                write!(
                    f,
                    "Unsupported @font-face descriptor declaration: '{}', ",
                    decl
                )?;
                parse_error_to_str(err, f)
            },
            ContextualParseError::UnsupportedFontFeatureValuesDescriptor(decl, ref err) => {
                write!(
                    f,
                    "Unsupported @font-feature-values descriptor declaration: '{}', ",
                    decl
                )?;
                parse_error_to_str(err, f)
            },
            ContextualParseError::UnsupportedFontPaletteValuesDescriptor(decl, ref err) => {
                write!(
                    f,
                    "Unsupported @font-palette-values descriptor declaration: '{}', ",
                    decl
                )?;
                parse_error_to_str(err, f)
            },
            ContextualParseError::InvalidKeyframeRule(rule, ref err) => {
                write!(f, "Invalid keyframe rule: '{}', ", rule)?;
                parse_error_to_str(err, f)
            },
            ContextualParseError::InvalidFontFeatureValuesRule(rule, ref err) => {
                write!(f, "Invalid font feature value rule: '{}', ", rule)?;
                parse_error_to_str(err, f)
            },
            ContextualParseError::UnsupportedKeyframePropertyDeclaration(decl, ref err) => {
                write!(f, "Unsupported keyframe property declaration: '{}', ", decl)?;
                parse_error_to_str(err, f)
            },
            ContextualParseError::InvalidRule(rule, ref err) => {
                write!(f, "Invalid rule: '{}', ", rule)?;
                parse_error_to_str(err, f)
            },
            ContextualParseError::UnsupportedRule(rule, ref err) => {
                write!(f, "Unsupported rule: '{}', ", rule)?;
                parse_error_to_str(err, f)
            },
            ContextualParseError::UnsupportedViewportDescriptorDeclaration(decl, ref err) => {
                write!(
                    f,
                    "Unsupported @viewport descriptor declaration: '{}', ",
                    decl
                )?;
                parse_error_to_str(err, f)
            },
            ContextualParseError::UnsupportedCounterStyleDescriptorDeclaration(decl, ref err) => {
                write!(
                    f,
                    "Unsupported @counter-style descriptor declaration: '{}', ",
                    decl
                )?;
                parse_error_to_str(err, f)
            },
            ContextualParseError::InvalidCounterStyleWithoutSymbols(ref system) => write!(
                f,
                "Invalid @counter-style rule: 'system: {}' without 'symbols'",
                system
            ),
            ContextualParseError::InvalidCounterStyleNotEnoughSymbols(ref system) => write!(
                f,
                "Invalid @counter-style rule: 'system: {}' less than two 'symbols'",
                system
            ),
            ContextualParseError::InvalidCounterStyleWithoutAdditiveSymbols => write!(
                f,
                "Invalid @counter-style rule: 'system: additive' without 'additive-symbols'"
            ),
            ContextualParseError::InvalidCounterStyleExtendsWithSymbols => write!(
                f,
                "Invalid @counter-style rule: 'system: extends …' with 'symbols'"
            ),
            ContextualParseError::InvalidCounterStyleExtendsWithAdditiveSymbols => write!(
                f,
                "Invalid @counter-style rule: 'system: extends …' with 'additive-symbols'"
            ),
            ContextualParseError::InvalidMediaRule(media_rule, ref err) => {
                write!(f, "Invalid media rule: {}, ", media_rule)?;
                parse_error_to_str(err, f)
            },
            ContextualParseError::UnsupportedValue(_value, ref err) => parse_error_to_str(err, f),
            ContextualParseError::NeverMatchingHostSelector(ref selector) => {
                write!(f, ":host selector is not featureless: {}", selector)
            },
        }
    }
}

/// A generic trait for an error reporter.
pub trait ParseErrorReporter {
    /// Called when the style engine detects an error.
    ///
    /// Returns the current input being parsed, the source location it was
    /// reported from, and a message.
    fn report_error(
        &self,
        url: &UrlExtraData,
        location: SourceLocation,
        error: ContextualParseError,
    );
}

/// An error reporter that uses [the `log` crate](https://github.com/rust-lang-nursery/log)
/// at `info` level.
///
/// This logging is silent by default, and can be enabled with a `RUST_LOG=style=info`
/// environment variable.
/// (See [`env_logger`](https://rust-lang-nursery.github.io/log/env_logger/).)
#[cfg(feature = "servo")]
pub struct RustLogReporter;

#[cfg(feature = "servo")]
impl ParseErrorReporter for RustLogReporter {
    fn report_error(
        &self,
        url: &UrlExtraData,
        location: SourceLocation,
        error: ContextualParseError,
    ) {
        if log_enabled!(log::Level::Info) {
            info!(
                "Url:\t{}\n{}:{} {}",
                url.as_str(),
                location.line,
                location.column,
                error
            )
        }
    }
}

/// Any warning a selector may generate.
/// TODO(dshin): Bug 1860634 - Merge with never matching host selector warning, which is part of the rule parser.
#[repr(u8)]
#[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
pub enum SelectorWarningKind {
    /// Relative Selector with not enough constraint, either outside or inside the selector. e.g. `*:has(.a)`, `.a:has(*)`.
    /// May cause expensive invalidations for every element inserted and/or removed.
    UnconstraintedRelativeSelector,
}

impl SelectorWarningKind {
    /// Get all warnings for this selector.
    pub fn from_selector(selector: &Selector<SelectorImpl>) -> Vec<Self> {
        let mut result = vec![];
        if UnconstrainedRelativeSelectorVisitor::has_warning(selector, 0, false) {
            result.push(SelectorWarningKind::UnconstraintedRelativeSelector);
        }
        result
    }
}

/// Per-compound state for finding unconstrained relative selectors.
struct PerCompoundState {
    /// Is there a relative selector in this compound?
    relative_selector_found: bool,
    /// Is this compound constrained in any way?
    constrained: bool,
    /// Nested below, or inside relative selector?
    in_relative_selector: bool,
}

impl PerCompoundState {
    fn new(in_relative_selector: bool) -> Self {
        Self {
            relative_selector_found: false,
            constrained: false,
            in_relative_selector,
        }
    }
}

/// Visitor to check if there's any unconstrained relative selector.
struct UnconstrainedRelativeSelectorVisitor {
    compound_state: PerCompoundState,
}

impl UnconstrainedRelativeSelectorVisitor {
    fn new(in_relative_selector: bool) -> Self {
        Self {
            compound_state: PerCompoundState::new(in_relative_selector),
        }
    }

    fn has_warning(
        selector: &Selector<SelectorImpl>,
        offset: usize,
        in_relative_selector: bool,
    ) -> bool {
        let relative_selector = matches!(
            selector.iter_raw_parse_order_from(0).next().unwrap(),
            Component::RelativeSelectorAnchor
        );
        debug_assert!(
            !relative_selector || offset == 0,
            "Checking relative selector from non-rightmost?"
        );
        let mut visitor = Self::new(in_relative_selector);
        let mut iter = if relative_selector {
            selector.iter_skip_relative_selector_anchor()
        } else {
            selector.iter_from(offset)
        };
        loop {
            visitor.compound_state = PerCompoundState::new(in_relative_selector);

            for s in &mut iter {
                s.visit(&mut visitor);
            }

            if (visitor.compound_state.relative_selector_found ||
                visitor.compound_state.in_relative_selector) &&
                !visitor.compound_state.constrained
            {
                return true;
            }

            if iter.next_sequence().is_none() {
                break;
            }
        }
        false
    }
}

impl SelectorVisitor for UnconstrainedRelativeSelectorVisitor {
    type Impl = SelectorImpl;

    fn visit_simple_selector(&mut self, c: &Component<Self::Impl>) -> bool {
        match c {
            // Deferred to visit_selector_list
            Component::Is(..) |
            Component::Where(..) |
            Component::Negation(..) |
            Component::Has(..) => (),
            Component::ExplicitUniversalType => (),
            _ => self.compound_state.constrained |= true,
        };
        true
    }

    fn visit_selector_list(
        &mut self,
        _list_kind: SelectorListKind,
        list: &[Selector<Self::Impl>],
    ) -> bool {
        let mut all_constrained = true;
        for s in list {
            let mut offset = 0;
            // First, check the rightmost compound for constraint at this level.
            if !self.compound_state.in_relative_selector {
                let mut nested = Self::new(false);
                let mut iter = s.iter();
                loop {
                    for c in &mut iter {
                        c.visit(&mut nested);
                        offset += 1;
                    }

                    let c = iter.next_sequence();
                    offset += 1;
                    if c.map_or(true, |c| !c.is_pseudo_element()) {
                        break;
                    }
                }
                // Every single selector in the list must be constrained.
                all_constrained &= nested.compound_state.constrained;
            }

            if offset >= s.len() {
                continue;
            }

            // Then, recurse in to check at the deeper level.
            if Self::has_warning(s, offset, self.compound_state.in_relative_selector) {
                self.compound_state.constrained = false;
                if !self.compound_state.in_relative_selector {
                    self.compound_state.relative_selector_found = true;
                }
                return false;
            }
        }
        self.compound_state.constrained |= all_constrained;
        true
    }

    fn visit_relative_selector_list(&mut self, list: &[RelativeSelector<Self::Impl>]) -> bool {
        debug_assert!(
            !self.compound_state.in_relative_selector,
            "Nested relative selector"
        );
        self.compound_state.relative_selector_found = true;

        for rs in list {
            // If the inside is unconstrained, we are unconstrained no matter what.
            if Self::has_warning(&rs.selector, 0, true) {
                self.compound_state.constrained = false;
                return false;
            }
        }
        true
    }
}
