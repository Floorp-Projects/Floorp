/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! A [`@scope`][scope] rule.
//!
//! [scope]: https://drafts.csswg.org/css-cascade-6/#scoped-styles

use crate::applicable_declarations::ScopeProximity;
use crate::dom::TElement;
use crate::parser::ParserContext;
use crate::selector_parser::{SelectorImpl, SelectorParser};
use crate::shared_lock::{
    DeepCloneParams, DeepCloneWithLock, Locked, SharedRwLock, SharedRwLockReadGuard, ToCssWithGuard,
};
use crate::str::CssStringWriter;
use crate::stylesheets::CssRules;
use cssparser::{Parser, SourceLocation, ToCss};
#[cfg(feature = "gecko")]
use malloc_size_of::{
    MallocSizeOfOps, MallocUnconditionalShallowSizeOf, MallocUnconditionalSizeOf,
};
use selectors::context::MatchingContext;
use selectors::matching::matches_selector;
use selectors::parser::{AncestorHashes, ParseRelative, Selector, SelectorList};
use selectors::OpaqueElement;
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
        fn bound_size_of(
            bound: &Option<SelectorList<SelectorImpl>>,
            ops: &mut MallocSizeOfOps,
        ) -> usize {
            bound
                .as_ref()
                .map(|list| list.unconditional_size_of(ops))
                .unwrap_or(0)
        }
        bound_size_of(&self.start, ops) + bound_size_of(&self.end, ops)
    }
}

fn parse_scope<'a>(
    context: &ParserContext,
    input: &mut Parser<'a, '_>,
    in_style_rule: bool,
    for_end: bool,
) -> Option<SelectorList<SelectorImpl>> {
    input
        .try_parse(|input| {
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
                    ParseRelative::ForScope
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
        let start = parse_scope(context, input, in_style_rule, false);

        let end = parse_scope(context, input, in_style_rule, true);
        Self { start, end }
    }
}

/// Types of implicit scope root.
#[derive(Debug, Clone, MallocSizeOf)]
pub enum ImplicitScopeRoot {
    /// This implicit scope root is in the light tree.
    InLightTree(OpaqueElement),
    /// This implicit scope root is in the shadow tree.
    InShadowTree(OpaqueElement),
    /// This implicit scope root is the shadow host of the stylesheet-containing shadow tree.
    ShadowHost(OpaqueElement),
    /// The implicit scope root is in a constructed stylesheet - the scope root the element
    /// under consideration's shadow root (If one exists).
    Constructed,
}

impl ImplicitScopeRoot {
    /// Return true if this matches the shadow host.
    pub fn matches_shadow_host(&self) -> bool {
        match self {
            Self::InLightTree(..) | Self::InShadowTree(..) => false,
            Self::ShadowHost(..) | Self::Constructed => true,
        }
    }

    /// Return the scope root element, given the element to be styled.
    pub fn element(&self, current_host: Option<OpaqueElement>) -> Option<OpaqueElement> {
        match self {
            Self::InLightTree(e) | Self::InShadowTree(e) | Self::ShadowHost(e) => Some(*e),
            Self::Constructed => current_host,
        }
    }
}

/// Target of this scope.
pub enum ScopeTarget<'a> {
    /// Target matches an element matching the specified selector list.
    Selector(&'a SelectorList<SelectorImpl>, &'a [AncestorHashes]),
    /// Target matches only the specified element.
    Element(OpaqueElement),
}

impl<'a> ScopeTarget<'a> {
    /// Check if the given element is the scope.
    pub fn check<E: TElement>(
        &self,
        element: E,
        scope: Option<OpaqueElement>,
        context: &mut MatchingContext<E::Impl>,
    ) -> bool {
        match self {
            Self::Selector(list, hashes_list) => {
                context.nest_for_scope_condition(scope, |context| {
                    for (selector, hashes) in list.slice().iter().zip(hashes_list.iter()) {
                        if matches_selector(selector, 0, Some(hashes), &element, context) {
                            return true;
                        }
                    }
                    false
                })
            },
            Self::Element(e) => element.opaque() == *e,
        }
    }
}

/// A scope root candidate.
#[derive(Clone, Copy, Debug)]
pub struct ScopeRootCandidate {
    /// This candidate's scope root.
    pub root: OpaqueElement,
    /// Ancestor hop from the element under consideration to this scope root.
    pub proximity: ScopeProximity,
}

/// Collect potential scope roots for a given element and its scope target.
/// The check may not pass the ceiling, if specified.
pub fn collect_scope_roots<E>(
    element: E,
    ceiling: Option<OpaqueElement>,
    context: &mut MatchingContext<E::Impl>,
    target: &ScopeTarget,
    matches_shadow_host: bool,
) -> Vec<ScopeRootCandidate>
where
    E: TElement,
{
    let mut result = vec![];
    let mut parent = Some(element);
    let mut proximity = 0usize;
    while let Some(p) = parent {
        if ceiling == Some(p.opaque()) {
            break;
        }
        if target.check(p, ceiling, context) {
            result.push(ScopeRootCandidate {
                root: p.opaque(),
                proximity: ScopeProximity::new(proximity),
            });
            // Note that we can't really break here - we need to consider
            // ALL scope roots to figure out whch one didn't end.
        }
        parent = p.parent_element();
        proximity += 1;
        // We we got to the top of the shadow tree - keep going
        // if we may match the shadow host.
        if parent.is_none() && matches_shadow_host {
            parent = p.containing_shadow_host();
        }
    }
    result
}

/// Given the scope-end selector, check if the element is outside of the scope.
/// That is, check if any ancestor to the root matches the scope-end selector.
pub fn element_is_outside_of_scope<E>(
    selector: &Selector<E::Impl>,
    hashes: &AncestorHashes,
    element: E,
    root: OpaqueElement,
    context: &mut MatchingContext<E::Impl>,
    root_may_be_shadow_host: bool,
) -> bool
where
    E: TElement,
{
    let mut parent = Some(element);
    context.nest_for_scope_condition(Some(root), |context| {
        while let Some(p) = parent {
            if matches_selector(selector, 0, Some(hashes), &p, context) {
                return true;
            }
            if p.opaque() == root {
                // Reached the top, not lying outside of scope.
                break;
            }
            parent = p.parent_element();
            if parent.is_none() && root_may_be_shadow_host {
                if let Some(host) = p.containing_shadow_host() {
                    // Pretty much an edge case where user specified scope-start and -end of :host
                    return host.opaque() == root;
                }
            }
        }
        return false;
    })
}
