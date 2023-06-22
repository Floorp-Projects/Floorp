/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! A [`@page`][page] rule.
//!
//! [page]: https://drafts.csswg.org/css2/page.html#page-box

use crate::parser::{Parse, ParserContext};
use crate::properties::PropertyDeclarationBlock;
use crate::shared_lock::{DeepCloneParams, DeepCloneWithLock, Locked};
use crate::shared_lock::{SharedRwLock, SharedRwLockReadGuard, ToCssWithGuard};
use crate::str::CssStringWriter;
use crate::values::{AtomIdent, CustomIdent};
use cssparser::{Parser, SourceLocation, Token};
#[cfg(feature = "gecko")]
use malloc_size_of::{MallocSizeOf, MallocSizeOfOps, MallocUnconditionalShallowSizeOf};
use servo_arc::Arc;
use std::fmt::{self, Write};
use smallvec::SmallVec;
use style_traits::{CssWriter, ParseError, ToCss};

macro_rules! page_pseudo_classes {
    ($($(#[$($meta:tt)+])* $id:ident => $val:literal,)+) => {
        /// [`@page`][page] rule pseudo-classes.
        ///
        /// https://drafts.csswg.org/css-page-3/#page-selectors
        #[derive(Clone, Copy, Debug, Eq, MallocSizeOf, PartialEq, ToShmem)]
        #[repr(u8)]
        pub enum PagePseudoClass {
            $($(#[$($meta)+])* $id,)+
        }
        impl PagePseudoClass {
            fn parse<'i, 't>(
                input: &mut Parser<'i, 't>,
            ) -> Result<Self, ParseError<'i>> {
                let loc = input.current_source_location();
                let colon = input.next_including_whitespace()?;
                if *colon != Token::Colon {
                    return Err(loc.new_unexpected_token_error(colon.clone()));
                }

                let ident = input.next_including_whitespace()?;
                if let Token::Ident(s) = ident {
                    return match_ignore_ascii_case! { &**s,
                        $($val => Ok(PagePseudoClass::$id),)+
                        _ => Err(loc.new_unexpected_token_error(Token::Ident(s.clone()))),
                    };
                }
                Err(loc.new_unexpected_token_error(ident.clone()))
            }
            #[inline]
            fn to_str(&self) -> &'static str {
                match *self {
                    $(PagePseudoClass::$id => concat!(':', $val),)+
                }
            }
        }
    }
}

page_pseudo_classes! {
    /// [`:first`][first] pseudo-class
    ///
    /// [first] https://drafts.csswg.org/css-page-3/#first-pseudo
    First => "first",
    /// [`:blank`][blank] pseudo-class
    ///
    /// [blank] https://drafts.csswg.org/css-page-3/#blank-pseudo
    Blank => "blank",
    /// [`:left`][left] pseudo-class
    ///
    /// [left]: https://drafts.csswg.org/css-page-3/#spread-pseudos
    Left => "left",
    /// [`:right`][right] pseudo-class
    ///
    /// [right]: https://drafts.csswg.org/css-page-3/#spread-pseudos
    Right => "right",
}

bitflags! {
    /// Bit-flags for pseudo-class. This should only be used for querying if a
    /// page-rule applies.
    ///
    /// https://drafts.csswg.org/css-page-3/#page-selectors
    #[derive(Clone, Copy)]
    #[repr(C)]
    pub struct PagePseudoClassFlags : u8 {
        /// No pseudo-classes
        const NONE = 0;
        /// Flag for PagePseudoClass::First
        const FIRST = 1 << 0;
        /// Flag for PagePseudoClass::Blank
        const BLANK = 1 << 1;
        /// Flag for PagePseudoClass::Left
        const LEFT = 1 << 2;
        /// Flag for PagePseudoClass::Right
        const RIGHT = 1 << 3;
    }
}

impl PagePseudoClassFlags {
    /// Creates a pseudo-class flags object with a single pseudo-class.
    #[inline]
    pub fn new(other: &PagePseudoClass) -> Self {
        match *other {
            PagePseudoClass::First => PagePseudoClassFlags::FIRST,
            PagePseudoClass::Blank => PagePseudoClassFlags::BLANK,
            PagePseudoClass::Left => PagePseudoClassFlags::LEFT,
            PagePseudoClass::Right => PagePseudoClassFlags::RIGHT,
        }
    }
    /// Checks if the given pseudo class applies to this set of flags.
    #[inline]
    pub fn contains_class(self, other: &PagePseudoClass) -> bool {
        self.intersects(PagePseudoClassFlags::new(other))
    }
}

type PagePseudoClasses = SmallVec<[PagePseudoClass; 4]>;

/// Type of a single [`@page`][page selector]
///
/// [page-selectors]: https://drafts.csswg.org/css2/page.html#page-selectors
#[derive(Clone, Debug, MallocSizeOf, ToShmem)]
pub struct PageSelector{
    /// Page name
    ///
    /// https://drafts.csswg.org/css-page-3/#page-type-selector
    pub name: AtomIdent,
    /// Pseudo-classes for [`@page`][page-selectors]
    ///
    /// [page-selectors]: https://drafts.csswg.org/css2/page.html#page-selectors
    pub pseudos: PagePseudoClasses,
}

impl PageSelector {
    /// Checks if the ident matches a page-name's ident.
    ///
    /// This does not take pseudo selectors into account.
    #[inline]
    pub fn ident_matches(&self, other: &CustomIdent) -> bool {
        self.name.0 == other.0
    }

    /// Checks that this selector matches the ident and all pseudo classes are
    /// present in the provided flags.
    #[inline]
    pub fn matches(&self, name: &CustomIdent, flags: PagePseudoClassFlags) -> bool {
        self.ident_matches(name) && self.flags_match(flags)
    }

    /// Checks that all pseudo classes in this selector are present in the
    /// provided flags.
    ///
    /// Equivalent to, but may be more efficient than:
    ///
    /// ```
    /// match_specificity(flags).is_some()
    /// ```
    pub fn flags_match(&self, flags: PagePseudoClassFlags) -> bool {
        self.pseudos.iter().all(|pc| flags.contains_class(pc))
    }

    /// Implements specificity calculation for a page selector given a set of
    /// page pseudo-classes to match with.
    /// If this selector includes any pseudo-classes that are not in the flags,
    /// then this will return None.
    ///
    /// To fit the specificity calculation into a 32-bit value, this limits the
    /// maximum count of :first and :blank to 32767, and the maximum count of
    /// :left and :right to 65535.
    ///
    /// https://drafts.csswg.org/css-page-3/#cascading-and-page-context
    pub fn match_specificity(&self, flags: PagePseudoClassFlags) -> Option<u32> {
        let mut g: usize = 0;
        let mut h: usize = 0;
        for pc in self.pseudos.iter() {
            if !flags.contains_class(pc) {
                return None
            }
            match pc {
                PagePseudoClass::First |
                PagePseudoClass::Blank => g += 1,
                PagePseudoClass::Left |
                PagePseudoClass::Right => h += 1,
            }
        }
        let h = h.min(0xFFFF) as u32;
        let g = (g.min(0x7FFF) as u32) << 16;
        let f = if self.name.0.is_empty() { 0 } else { 0x80000000 };
        Some(h + g + f)
    }
}

impl ToCss for PageSelector {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write
    {
        self.name.to_css(dest)?;
        for pc in self.pseudos.iter() {
            dest.write_str(pc.to_str())?;
        }
        Ok(())
    }
}

fn parse_page_name<'i, 't>(
    input: &mut Parser<'i, 't>
) -> Result<AtomIdent, ParseError<'i>> {
    let s = input.expect_ident()?;
    Ok(AtomIdent::from(&**s))
}

impl Parse for PageSelector {
    fn parse<'i, 't>(
        _context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<Self, ParseError<'i>> {
        let name = input.try_parse(parse_page_name).unwrap_or(AtomIdent(atom!("")));
        let mut pseudos = PagePseudoClasses::default();
        while let Ok(pc) = input.try_parse(PagePseudoClass::parse) {
            pseudos.push(pc);
        }
        Ok(PageSelector{name, pseudos})
    }
}

/// A list of [`@page`][page selectors]
///
/// [page-selectors]: https://drafts.csswg.org/css2/page.html#page-selectors
#[derive(Clone, Debug, Default, MallocSizeOf, ToCss, ToShmem)]
#[css(comma)]
pub struct PageSelectors(#[css(iterable)] pub Box<[PageSelector]>);

impl PageSelectors {
    /// Creates a new PageSelectors from a Vec, as from parse_comma_separated
    #[inline]
    pub fn new(s: Vec<PageSelector>) -> Self {
        PageSelectors(s.into())
    }
    /// Returns true iff there are any page selectors
    #[inline]
    pub fn is_empty(&self) -> bool {
        self.as_slice().is_empty()
    }
    /// Get the underlying PageSelector data as a slice
    #[inline]
    pub fn as_slice(&self) -> &[PageSelector] {
        &*self.0
    }
}

impl Parse for PageSelectors {
    fn parse<'i, 't>(
        context: &ParserContext,
        input: &mut Parser<'i, 't>,
    ) -> Result<Self, ParseError<'i>> {
        Ok(PageSelectors::new(input.parse_comma_separated(|i| {
            PageSelector::parse(context, i)
        })?))
    }
}

/// A [`@page`][page] rule.
///
/// This implements only a limited subset of the CSS
/// 2.2 syntax.
///
/// [page]: https://drafts.csswg.org/css2/page.html#page-box
/// [page-selectors]: https://drafts.csswg.org/css2/page.html#page-selectors
#[derive(Clone, Debug, ToShmem)]
pub struct PageRule {
    /// Selectors of the page-rule
    pub selectors: PageSelectors,
    /// The declaration block this page rule contains.
    pub block: Arc<Locked<PropertyDeclarationBlock>>,
    /// The source position this rule was found at.
    pub source_location: SourceLocation,
}

impl PageRule {
    /// Measure heap usage.
    #[cfg(feature = "gecko")]
    pub fn size_of(&self, guard: &SharedRwLockReadGuard, ops: &mut MallocSizeOfOps) -> usize {
        // Measurement of other fields may be added later.
        self.block.unconditional_shallow_size_of(ops) +
            self.block.read_with(guard).size_of(ops) +
            self.selectors.size_of(ops)
    }
    /// Computes the specificity of this page rule when matched with flags.
    ///
    /// Computing this value has linear-complexity with the size of the
    /// selectors, so the caller should usually call this once and cache the
    /// result.
    ///
    /// Returns None if the flags do not match this page rule.
    ///
    /// The return type is ordered by page-rule specificity.
    pub fn match_specificity(&self, flags: PagePseudoClassFlags) -> Option<u32> {
        let mut specificity = None;
        for s in self.selectors.0.iter().map(|s| s.match_specificity(flags)) {
            specificity = s.max(specificity);
        }
        specificity
    }
}

impl ToCssWithGuard for PageRule {
    /// Serialization of PageRule is not specced, adapted from steps for
    /// StyleRule.
    fn to_css(&self, guard: &SharedRwLockReadGuard, dest: &mut CssStringWriter) -> fmt::Result {
        dest.write_str("@page ")?;
        if !self.selectors.is_empty() {
            self.selectors.to_css(&mut CssWriter::new(dest))?;
            dest.write_char(' ')?;
        }
        dest.write_str("{ ")?;
        let declaration_block = self.block.read_with(guard);
        declaration_block.to_css(dest)?;
        if !declaration_block.declarations().is_empty() {
            dest.write_char(' ')?;
        }
        dest.write_char('}')
    }
}

impl DeepCloneWithLock for PageRule {
    fn deep_clone_with_lock(
        &self,
        lock: &SharedRwLock,
        guard: &SharedRwLockReadGuard,
        _params: &DeepCloneParams,
    ) -> Self {
        PageRule {
            selectors: self.selectors.clone(),
            block: Arc::new(lock.wrap(self.block.read_with(&guard).clone())),
            source_location: self.source_location.clone(),
        }
    }
}
