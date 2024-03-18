/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! A [`@scope`][scope] rule.
//!
//! [scope]: https://drafts.csswg.org/css-cascade-6/#scoped-styles

use crate::parser::ParserContext;
use crate::selector_parser::{SelectorImpl, SelectorParser};
use crate::shared_lock::{
    DeepCloneParams, DeepCloneWithLock, Locked, SharedRwLock, SharedRwLockReadGuard, ToCssWithGuard,
};
use crate::str::CssStringWriter;
use crate::stylesheets::CssRules;
use cssparser::{Parser, SourceLocation, ToCss};
#[cfg(feature = "gecko")]
use malloc_size_of::{MallocSizeOfOps, MallocUnconditionalSizeOf, MallocUnconditionalShallowSizeOf};
use selectors::parser::{ParseRelative, SelectorList};
use servo_arc::Arc;
use std::fmt::{self, Write};
use style_traits::CssWriter;

/// A scoped rule.
#[derive(Debug, ToShmem)]
pub struct ScopeRule {
    /// Bounds at which this rule applies.
    pub bounds: ScopeBounds,
    /// The nested rules inside the block.
    pub rules: Arc<Locked<CssRules>>,
    /// The source position where this rule was found.
    pub source_location: SourceLocation,
}

impl DeepCloneWithLock for ScopeRule {
    fn deep_clone_with_lock(
        &self,
        lock: &SharedRwLock,
        guard: &SharedRwLockReadGuard,
        params: &DeepCloneParams,
    ) -> Self {
        let rules = self.rules.read_with(guard);
        Self {
            bounds: self.bounds.clone(),
            rules: Arc::new(lock.wrap(rules.deep_clone_with_lock(lock, guard, params))),
            source_location: self.source_location.clone(),
        }
    }
}

impl ToCssWithGuard for ScopeRule {
    fn to_css(&self, guard: &SharedRwLockReadGuard, dest: &mut CssStringWriter) -> fmt::Result {
        dest.write_str("@scope")?;
        {
            let mut writer = CssWriter::new(dest);
            if let Some(start) = self.bounds.start.as_ref() {
                writer.write_str(" (")?;
                start.to_css(&mut writer)?;
                writer.write_char(')')?;
            }
            if let Some(end) = self.bounds.end.as_ref() {
                writer.write_str(" to (")?;
                end.to_css(&mut writer)?;
                writer.write_char(')')?;
            }
        }
        self.rules.read_with(guard).to_css_block(guard, dest)
    }
}

impl ScopeRule {
    /// Measure heap usage.
    #[cfg(feature = "gecko")]
    pub fn size_of(&self, guard: &SharedRwLockReadGuard, ops: &mut MallocSizeOfOps) -> usize {
        self.rules.unconditional_shallow_size_of(ops) +
            self.rules.read_with(guard).size_of(guard, ops) +
            self.bounds.size_of(ops)
    }
}

/// Bounds of the scope.
#[derive(Debug, Clone, ToShmem)]
pub struct ScopeBounds {
    /// Start of the scope.
    pub start: Option<SelectorList<SelectorImpl>>,
    /// End of the scope.
    pub end: Option<SelectorList<SelectorImpl>>,
}

impl ScopeBounds {
    #[cfg(feature = "gecko")]
    fn size_of(&self, ops: &mut MallocSizeOfOps) -> usize {
        fn bound_size_of(bound: &Option<SelectorList<SelectorImpl>>, ops: &mut MallocSizeOfOps) -> usize {
            bound.as_ref().map(|list| list.unconditional_size_of(ops)).unwrap_or(0)
        }
        bound_size_of(&self.start, ops) + bound_size_of(&self.end, ops)
    }
}

fn parse_scope<'a>(
    context: &ParserContext,
    input: &mut Parser<'a, '_>,
    in_style_rule: bool,
    for_end: bool
) -> Option<SelectorList<SelectorImpl>> {
    input.try_parse(|input| {
        if for_end {
            input.expect_ident_matching("to")?;
        }
        input.expect_parenthesis_block()?;
        input.parse_nested_block(|input| {
            if input.is_exhausted() {
                return Ok(None);
            }
            let selector_parser = SelectorParser {
                stylesheet_origin: context.stylesheet_origin,
                namespaces: &context.namespaces,
                url_data: context.url_data,
                for_supports_rule: false,
            };
            let parse_relative = if for_end {
                // TODO(dshin): scope-end can be a relative selector, with the anchor being `:scope`.
                ParseRelative::No
            } else if in_style_rule {
                ParseRelative::ForNesting
            } else {
                ParseRelative::No
            };
            Ok(Some(SelectorList::parse_forgiving(
                &selector_parser,
                input,
                parse_relative,
            )?))
        })
    })
    .ok()
    .flatten()
}

impl ScopeBounds {
    /// Parse a container condition.
    pub fn parse<'a>(
        context: &ParserContext,
        input: &mut Parser<'a, '_>,
        in_style_rule: bool,
    ) -> Self {
        let start = parse_scope(
            context,
            input,
            in_style_rule,
            false
        );

        let end = parse_scope(
            context,
            input,
            in_style_rule,
            true
        );
        Self { start, end }
    }
}
