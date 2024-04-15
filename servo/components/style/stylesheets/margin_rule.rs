/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! A [`@margin`][margin] rule.
//!
//! [margin]: https://drafts.csswg.org/css-page-3/#margin-boxes

use crate::properties::PropertyDeclarationBlock;
use crate::shared_lock::{DeepCloneParams, DeepCloneWithLock, Locked};
use crate::shared_lock::{SharedRwLock, SharedRwLockReadGuard, ToCssWithGuard};
use crate::str::CssStringWriter;
use cssparser::SourceLocation;
#[cfg(feature = "gecko")]
use malloc_size_of::{MallocSizeOf, MallocSizeOfOps, MallocUnconditionalShallowSizeOf};
use servo_arc::Arc;
use std::fmt::{self, Write};

macro_rules! margin_rule_types {
    ($($(#[$($meta:tt)+])* $id:ident => $val:literal,)+) => {
        /// [`@margin`][margin] rule names.
        ///
        /// https://drafts.csswg.org/css-page-3/#margin-at-rules
        #[derive(Clone, Copy, Debug, Eq, MallocSizeOf, PartialEq, ToShmem)]
        #[repr(u8)]
        pub enum MarginRuleType {
            $($(#[$($meta)+])* $id,)+
        }

        impl MarginRuleType {
            #[inline]
            fn to_str(&self) -> &'static str {
                match *self {
                    $(MarginRuleType::$id => concat!('@', $val),)+
                }
            }
            /// Matches the rule type for this name. This does not expect a
            /// leading '@'.
            pub fn match_name(name: &str) -> Option<Self> {
                Some(match_ignore_ascii_case! { name,
                    $( $val => MarginRuleType::$id, )+
                    _ => return None,
                })
            }
        }
    }
}

margin_rule_types! {
    /// [`@top-left-corner`][top-left-corner] margin rule
    ///
    /// [top-left-corner] https://drafts.csswg.org/css-page-3/#top-left-corner-box-def
    TopLeftCorner => "top-left-corner",
    /// [`@top-left`][top-left] margin rule
    ///
    /// [top-left] https://drafts.csswg.org/css-page-3/#top-left-box-def
    TopLeft => "top-left",
    /// [`@top-center`][top-center] margin rule
    ///
    /// [top-center] https://drafts.csswg.org/css-page-3/#top-center-box-def
    TopCenter => "top-center",
    /// [`@top-right`][top-right] margin rule
    ///
    /// [top-right] https://drafts.csswg.org/css-page-3/#top-right-box-def
    TopRight => "top-right",
    /// [`@top-right-corner`][top-right-corner] margin rule
    ///
    /// [top-right-corner] https://drafts.csswg.org/css-page-3/#top-right-corner-box-def
    TopRightCorner => "top-right-corner",
    /// [`@bottom-left-corner`][bottom-left-corner] margin rule
    ///
    /// [bottom-left-corner] https://drafts.csswg.org/css-page-3/#bottom-left-corner-box-def
    BottomLeftCorner => "bottom-left-corner",
    /// [`@bottom-left`][bottom-left] margin rule
    ///
    /// [bottom-left] https://drafts.csswg.org/css-page-3/#bottom-left-box-def
    BottomLeft => "bottom-left",
    /// [`@bottom-center`][bottom-center] margin rule
    ///
    /// [bottom-center] https://drafts.csswg.org/css-page-3/#bottom-center-box-def
    BottomCenter => "bottom-center",
    /// [`@bottom-right`][bottom-right] margin rule
    ///
    /// [bottom-right] https://drafts.csswg.org/css-page-3/#bottom-right-box-def
    BottomRight => "bottom-right",
    /// [`@bottom-right-corner`][bottom-right-corner] margin rule
    ///
    /// [bottom-right-corner] https://drafts.csswg.org/css-page-3/#bottom-right-corner-box-def
    BottomRightCorner => "bottom-right-corner",
    /// [`@left-top`][left-top] margin rule
    ///
    /// [left-top] https://drafts.csswg.org/css-page-3/#left-top-box-def
    LeftTop => "left-top",
    /// [`@left-middle`][left-middle] margin rule
    ///
    /// [left-middle] https://drafts.csswg.org/css-page-3/#left-middle-box-def
    LeftMiddle => "left-middle",
    /// [`@left-bottom`][left-bottom] margin rule
    ///
    /// [left-bottom] https://drafts.csswg.org/css-page-3/#left-bottom-box-def
    LeftBottom => "left-bottom",
    /// [`@right-top`][right-top] margin rule
    ///
    /// [right-top] https://drafts.csswg.org/css-page-3/#right-top-box-def
    RightTop => "right-top",
    /// [`@right-middle`][right-middle] margin rule
    ///
    /// [right-middle] https://drafts.csswg.org/css-page-3/#right-middle-box-def
    RightMiddle => "right-middle",
    /// [`@right-bottom`][right-bottom] margin rule
    ///
    /// [right-bottom] https://drafts.csswg.org/css-page-3/#right-bottom-box-def
    RightBottom => "right-bottom",
}

/// A [`@margin`][margin] rule.
///
/// [margin]: https://drafts.csswg.org/css-page-3/#margin-at-rules
#[derive(Clone, Debug, ToShmem)]
pub struct MarginRule {
    /// Type of this margin rule.
    pub rule_type: MarginRuleType,
    /// The declaration block this margin rule contains.
    pub block: Arc<Locked<PropertyDeclarationBlock>>,
    /// The source position this rule was found at.
    pub source_location: SourceLocation,
}

impl MarginRule {
    /// Measure heap usage.
    #[cfg(feature = "gecko")]
    pub fn size_of(&self, guard: &SharedRwLockReadGuard, ops: &mut MallocSizeOfOps) -> usize {
        // Measurement of other fields may be added later.
        self.block.unconditional_shallow_size_of(ops) +
            self.block.read_with(guard).size_of(ops)
    }
}

impl ToCssWithGuard for MarginRule {
    /// Serialization of a margin-rule is not specced, this is adapted from how
    /// page-rules and style-rules are serialized.
    fn to_css(&self, guard: &SharedRwLockReadGuard, dest: &mut CssStringWriter) -> fmt::Result {
        dest.write_str(self.rule_type.to_str())?;
        dest.write_str(" { ")?;
        let declaration_block = self.block.read_with(guard);
        declaration_block.to_css(dest)?;
        if !declaration_block.declarations().is_empty() {
            dest.write_char(' ')?;
        }
        dest.write_char('}')
    }
}

impl DeepCloneWithLock for MarginRule {
    fn deep_clone_with_lock(
        &self,
        lock: &SharedRwLock,
        guard: &SharedRwLockReadGuard,
        _params: &DeepCloneParams,
    ) -> Self {
        MarginRule {
            rule_type: self.rule_type,
            block: Arc::new(lock.wrap(self.block.read_with(&guard).clone())),
            source_location: self.source_location.clone(),
        }
    }
}
