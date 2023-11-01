/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Helper module to build up a selector safely and efficiently.
//!
//! Our selector representation is designed to optimize matching, and has
//! several requirements:
//! * All simple selectors and combinators are stored inline in the same buffer
//!   as Component instances.
//! * We store the top-level compound selectors from right to left, i.e. in
//!   matching order.
//! * We store the simple selectors for each combinator from left to right, so
//!   that we match the cheaper simple selectors first.
//!
//! Meeting all these constraints without extra memmove traffic during parsing
//! is non-trivial. This module encapsulates those details and presents an
//! easy-to-use API for the parser.

use crate::parser::{Combinator, Component, RelativeSelector, Selector, SelectorImpl};
use crate::sink::Push;
use servo_arc::{Arc, ThinArc};
use smallvec::{self, SmallVec};
use std::cmp;
use std::iter;
use std::ptr;
use std::slice;

/// Top-level SelectorBuilder struct. This should be stack-allocated by the
/// consumer and never moved (because it contains a lot of inline data that
/// would be slow to memmov).
///
/// After instantation, callers may call the push_simple_selector() and
/// push_combinator() methods to append selector data as it is encountered
/// (from left to right). Once the process is complete, callers should invoke
/// build(), which transforms the contents of the SelectorBuilder into a heap-
/// allocated Selector and leaves the builder in a drained state.
#[derive(Debug)]
pub struct SelectorBuilder<Impl: SelectorImpl> {
    /// The entire sequence of simple selectors, from left to right, without combinators.
    ///
    /// We make this large because the result of parsing a selector is fed into a new
    /// Arc-ed allocation, so any spilled vec would be a wasted allocation. Also,
    /// Components are large enough that we don't have much cache locality benefit
    /// from reserving stack space for fewer of them.
    simple_selectors: SmallVec<[Component<Impl>; 32]>,
    /// The combinators, and the length of the compound selector to their left.
    combinators: SmallVec<[(Combinator, usize); 16]>,
    /// The length of the current compount selector.
    current_len: usize,
}

impl<Impl: SelectorImpl> Default for SelectorBuilder<Impl> {
    #[inline(always)]
    fn default() -> Self {
        SelectorBuilder {
            simple_selectors: SmallVec::new(),
            combinators: SmallVec::new(),
            current_len: 0,
        }
    }
}

impl<Impl: SelectorImpl> Push<Component<Impl>> for SelectorBuilder<Impl> {
    fn push(&mut self, value: Component<Impl>) {
        self.push_simple_selector(value);
    }
}

impl<Impl: SelectorImpl> SelectorBuilder<Impl> {
    /// Pushes a simple selector onto the current compound selector.
    #[inline(always)]
    pub fn push_simple_selector(&mut self, ss: Component<Impl>) {
        assert!(!ss.is_combinator());
        self.simple_selectors.push(ss);
        self.current_len += 1;
    }

    /// Completes the current compound selector and starts a new one, delimited
    /// by the given combinator.
    #[inline(always)]
    pub fn push_combinator(&mut self, c: Combinator) {
        self.combinators.push((c, self.current_len));
        self.current_len = 0;
    }

    /// Returns true if combinators have ever been pushed to this builder.
    #[inline(always)]
    pub fn has_combinators(&self) -> bool {
        !self.combinators.is_empty()
    }

    /// Consumes the builder, producing a Selector.
    #[inline(always)]
    pub fn build(&mut self) -> ThinArc<SpecificityAndFlags, Component<Impl>> {
        // Compute the specificity and flags.
        let sf = specificity_and_flags(self.simple_selectors.iter());
        self.build_with_specificity_and_flags(sf)
    }

    /// Builds with an explicit SpecificityAndFlags. This is separated from build() so
    /// that unit tests can pass an explicit specificity.
    #[inline(always)]
    pub(crate) fn build_with_specificity_and_flags(
        &mut self,
        spec: SpecificityAndFlags,
    ) -> ThinArc<SpecificityAndFlags, Component<Impl>> {
        // Create the Arc using an iterator that drains our buffers.
        // Panic-safety: if SelectorBuilderIter is not iterated to the end, some simple selectors
        // will safely leak.
        let raw_simple_selectors = unsafe {
            let simple_selectors_len = self.simple_selectors.len();
            self.simple_selectors.set_len(0);
            std::slice::from_raw_parts(self.simple_selectors.as_ptr(), simple_selectors_len)
        };
        let (rest, current) = split_from_end(raw_simple_selectors, self.current_len);
        let iter = SelectorBuilderIter {
            current_simple_selectors: current.iter(),
            rest_of_simple_selectors: rest,
            combinators: self.combinators.drain(..).rev(),
        };

        Arc::from_header_and_iter(spec, iter)
    }
}

struct SelectorBuilderIter<'a, Impl: SelectorImpl> {
    current_simple_selectors: slice::Iter<'a, Component<Impl>>,
    rest_of_simple_selectors: &'a [Component<Impl>],
    combinators: iter::Rev<smallvec::Drain<'a, [(Combinator, usize); 16]>>,
}

impl<'a, Impl: SelectorImpl> ExactSizeIterator for SelectorBuilderIter<'a, Impl> {
    fn len(&self) -> usize {
        self.current_simple_selectors.len() +
            self.rest_of_simple_selectors.len() +
            self.combinators.len()
    }
}

impl<'a, Impl: SelectorImpl> Iterator for SelectorBuilderIter<'a, Impl> {
    type Item = Component<Impl>;
    #[inline(always)]
    fn next(&mut self) -> Option<Self::Item> {
        if let Some(simple_selector_ref) = self.current_simple_selectors.next() {
            // Move a simple selector out of this slice iterator.
            // This is safe because we’ve called SmallVec::set_len(0) above,
            // so SmallVec::drop won’t drop this simple selector.
            unsafe { Some(ptr::read(simple_selector_ref)) }
        } else {
            self.combinators.next().map(|(combinator, len)| {
                let (rest, current) = split_from_end(self.rest_of_simple_selectors, len);
                self.rest_of_simple_selectors = rest;
                self.current_simple_selectors = current.iter();
                Component::Combinator(combinator)
            })
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        (self.len(), Some(self.len()))
    }
}

fn split_from_end<T>(s: &[T], at: usize) -> (&[T], &[T]) {
    s.split_at(s.len() - at)
}

/// Flags that indicate at which point of parsing a selector are we.
#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, ToShmem)]
pub (crate) struct SelectorFlags(u8);

bitflags! {
    impl SelectorFlags: u8 {
        const HAS_PSEUDO = 1 << 0;
        const HAS_SLOTTED = 1 << 1;
        const HAS_PART = 1 << 2;
        const HAS_PARENT = 1 << 3;
        const HAS_NON_FEATURELESS_COMPONENT = 1 << 4;
        const HAS_HOST = 1 << 5;
    }
}

impl SelectorFlags {
    /// When you nest a pseudo-element with something like:
    ///
    ///   ::before { & { .. } }
    ///
    /// It is not supposed to work, because :is(::before) is invalid. We can't propagate the
    /// pseudo-flags from inner to outer selectors, to avoid breaking our invariants.
    pub (crate) fn for_nesting() -> Self {
        Self::all() - (Self::HAS_PSEUDO | Self::HAS_SLOTTED | Self::HAS_PART)
    }
}

#[derive(Clone, Copy, Debug, Default, Eq, PartialEq, ToShmem)]
pub struct SpecificityAndFlags {
    /// There are two free bits here, since we use ten bits for each specificity
    /// kind (id, class, element).
    pub(crate) specificity: u32,
    /// There's padding after this field due to the size of the flags.
    pub(crate) flags: SelectorFlags,
}

const MAX_10BIT: u32 = (1u32 << 10) - 1;

#[derive(Add, AddAssign, Clone, Copy, Default, Eq, Ord, PartialEq, PartialOrd)]
pub(crate) struct Specificity {
    id_selectors: u32,
    class_like_selectors: u32,
    element_selectors: u32,
}

impl From<u32> for Specificity {
    #[inline]
    fn from(value: u32) -> Specificity {
        assert!(value <= MAX_10BIT << 20 | MAX_10BIT << 10 | MAX_10BIT);
        Specificity {
            id_selectors: value >> 20,
            class_like_selectors: (value >> 10) & MAX_10BIT,
            element_selectors: value & MAX_10BIT,
        }
    }
}

impl From<Specificity> for u32 {
    #[inline]
    fn from(specificity: Specificity) -> u32 {
        cmp::min(specificity.id_selectors, MAX_10BIT) << 20 |
            cmp::min(specificity.class_like_selectors, MAX_10BIT) << 10 |
            cmp::min(specificity.element_selectors, MAX_10BIT)
    }
}

pub(crate) fn specificity_and_flags<Impl>(iter: slice::Iter<Component<Impl>>) -> SpecificityAndFlags
where
    Impl: SelectorImpl,
{
    complex_selector_specificity_and_flags(iter).into()
}

fn complex_selector_specificity_and_flags<Impl>(
    iter: slice::Iter<Component<Impl>>,
) -> SpecificityAndFlags
where
    Impl: SelectorImpl,
{
    fn component_specificity<Impl>(
        simple_selector: &Component<Impl>,
        specificity: &mut Specificity,
        flags: &mut SelectorFlags,
    ) where
        Impl: SelectorImpl,
    {
        match *simple_selector {
            Component::Combinator(..) => {},
            Component::ParentSelector => flags.insert(SelectorFlags::HAS_PARENT),
            Component::Part(..) => {
                flags.insert(SelectorFlags::HAS_PART);
                specificity.element_selectors += 1
            },
            Component::PseudoElement(..) => {
                flags.insert(SelectorFlags::HAS_PSEUDO);
                specificity.element_selectors += 1
            },
            Component::LocalName(..) => {
                flags.insert(SelectorFlags::HAS_NON_FEATURELESS_COMPONENT);
                specificity.element_selectors += 1
            },
            Component::Slotted(ref selector) => {
                flags.insert(SelectorFlags::HAS_SLOTTED | SelectorFlags::HAS_NON_FEATURELESS_COMPONENT);
                specificity.element_selectors += 1;
                // Note that due to the way ::slotted works we only compete with
                // other ::slotted rules, so the above rule doesn't really
                // matter, but we do it still for consistency with other
                // pseudo-elements.
                //
                // See: https://github.com/w3c/csswg-drafts/issues/1915
                *specificity += Specificity::from(selector.specificity());
                flags.insert(selector.flags());
            },
            Component::Host(ref selector) => {
                flags.insert(SelectorFlags::HAS_HOST);
                specificity.class_like_selectors += 1;
                if let Some(ref selector) = *selector {
                    // See: https://github.com/w3c/csswg-drafts/issues/1915
                    *specificity += Specificity::from(selector.specificity());
                    flags.insert(selector.flags() - SelectorFlags::HAS_NON_FEATURELESS_COMPONENT);
                }
            },
            Component::ID(..) => {
                flags.insert(SelectorFlags::HAS_NON_FEATURELESS_COMPONENT);
                specificity.id_selectors += 1;
            },
            Component::Class(..) |
            Component::AttributeInNoNamespace { .. } |
            Component::AttributeInNoNamespaceExists { .. } |
            Component::AttributeOther(..) |
            Component::Root |
            Component::Empty |
            Component::Scope |
            Component::Nth(..) |
            Component::NonTSPseudoClass(..) => {
                flags.insert(SelectorFlags::HAS_NON_FEATURELESS_COMPONENT);
                specificity.class_like_selectors += 1;
            },
            Component::NthOf(ref nth_of_data) => {
                // https://drafts.csswg.org/selectors/#specificity-rules:
                //
                //     The specificity of the :nth-last-child() pseudo-class,
                //     like the :nth-child() pseudo-class, combines the
                //     specificity of a regular pseudo-class with that of its
                //     selector argument S.
                specificity.class_like_selectors += 1;
                let sf = selector_list_specificity_and_flags(nth_of_data.selectors().iter());
                *specificity += Specificity::from(sf.specificity);
                flags.insert(sf.flags | SelectorFlags::HAS_NON_FEATURELESS_COMPONENT);
            },
            // https://drafts.csswg.org/selectors/#specificity-rules:
            //
            //     The specificity of an :is(), :not(), or :has() pseudo-class
            //     is replaced by the specificity of the most specific complex
            //     selector in its selector list argument.
            Component::Where(ref list) |
            Component::Negation(ref list) |
            Component::Is(ref list) => {
                let sf = selector_list_specificity_and_flags(list.slice().iter());
                if !matches!(*simple_selector, Component::Where(..)) {
                    *specificity += Specificity::from(sf.specificity);
                }
                flags.insert(sf.flags);
            },
            Component::Has(ref relative_selectors) => {
                let sf = relative_selector_list_specificity_and_flags(relative_selectors);
                *specificity += Specificity::from(sf.specificity);
                flags.insert(sf.flags | SelectorFlags::HAS_NON_FEATURELESS_COMPONENT);
            },
            Component::ExplicitUniversalType |
            Component::ExplicitAnyNamespace |
            Component::ExplicitNoNamespace |
            Component::DefaultNamespace(..) |
            Component::Namespace(..) |
            Component::RelativeSelectorAnchor |
            Component::Invalid(..) => {
                // Does not affect specificity
                flags.insert(SelectorFlags::HAS_NON_FEATURELESS_COMPONENT);
            },
        }
    }

    let mut specificity = Default::default();
    let mut flags = Default::default();
    for simple_selector in iter {
        component_specificity(&simple_selector, &mut specificity, &mut flags);
    }
    SpecificityAndFlags {
        specificity: specificity.into(),
        flags,
    }
}

/// Finds the maximum specificity of elements in the list and returns it.
pub(crate) fn selector_list_specificity_and_flags<'a, Impl: SelectorImpl>(
    itr: impl Iterator<Item = &'a Selector<Impl>>,
) -> SpecificityAndFlags {
    let mut specificity = 0;
    let mut flags = SelectorFlags::empty();
    for selector in itr {
        specificity = std::cmp::max(specificity, selector.specificity());
        flags.insert(selector.flags());
    }
    SpecificityAndFlags { specificity, flags }
}

pub(crate) fn relative_selector_list_specificity_and_flags<Impl: SelectorImpl>(
    list: &[RelativeSelector<Impl>],
) -> SpecificityAndFlags {
    selector_list_specificity_and_flags(list.iter().map(|rel| &rel.selector))
}
