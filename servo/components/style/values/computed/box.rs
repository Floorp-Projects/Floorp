/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Computed types for box properties.

use crate::values::animated::{Animate, Procedure, ToAnimatedValue};
use crate::values::computed::font::FixedPoint;
use crate::values::computed::length::{LengthPercentage, NonNegativeLength};
use crate::values::computed::{Context, Integer, Number, ToComputedValue};
use crate::values::generics::box_::{
    GenericContainIntrinsicSize, GenericLineClamp, GenericPerspective, GenericVerticalAlign,
};
use crate::values::specified::box_ as specified;
use std::fmt;
use style_traits::{CssWriter, ToCss};

pub use crate::values::specified::box_::{
    Appearance, BaselineSource, BreakBetween, BreakWithin, Clear as SpecifiedClear, Contain,
    ContainerName, ContainerType, ContentVisibility, Display, Float as SpecifiedFloat, Overflow,
    OverflowAnchor, OverflowClipBox, OverscrollBehavior, ScrollSnapAlign, ScrollSnapAxis,
    ScrollSnapStop, ScrollSnapStrictness, ScrollSnapType, ScrollbarGutter, TouchAction, WillChange,
};

/// A computed value for the `vertical-align` property.
pub type VerticalAlign = GenericVerticalAlign<LengthPercentage>;

/// A computed value for the `contain-intrinsic-size` property.
pub type ContainIntrinsicSize = GenericContainIntrinsicSize<NonNegativeLength>;

impl ContainIntrinsicSize {
    /// Converts contain-intrinsic-size to auto style.
    pub fn add_auto_if_needed(&self) -> Option<Self> {
        Some(match *self {
            Self::None => Self::AutoNone,
            Self::Length(ref l) => Self::AutoLength(*l),
            Self::AutoNone | Self::AutoLength(..) => return None,
        })
    }
}

/// A computed value for the `line-clamp` property.
pub type LineClamp = GenericLineClamp<Integer>;

impl Animate for LineClamp {
    #[inline]
    fn animate(&self, other: &Self, procedure: Procedure) -> Result<Self, ()> {
        if self.is_none() != other.is_none() {
            return Err(());
        }
        if self.is_none() {
            return Ok(Self::none());
        }
        Ok(Self(self.0.animate(&other.0, procedure)?.max(1)))
    }
}

/// A computed value for the `perspective` property.
pub type Perspective = GenericPerspective<NonNegativeLength>;

#[allow(missing_docs)]
#[cfg_attr(feature = "servo", derive(Deserialize, Serialize))]
#[derive(
    Clone,
    Copy,
    Debug,
    Eq,
    FromPrimitive,
    Hash,
    MallocSizeOf,
    Parse,
    PartialEq,
    SpecifiedValueInfo,
    ToCss,
    ToResolvedValue,
)]
#[repr(u8)]
/// A computed value for the `float` property.
pub enum Float {
    Left,
    Right,
    None,
}

impl Float {
    /// Returns true if `self` is not `None`.
    pub fn is_floating(self) -> bool {
        self != Self::None
    }
}

impl ToComputedValue for SpecifiedFloat {
    type ComputedValue = Float;

    #[inline]
    fn to_computed_value(&self, context: &Context) -> Self::ComputedValue {
        let ltr = context.style().writing_mode.is_bidi_ltr();
        // https://drafts.csswg.org/css-logical-props/#float-clear
        match *self {
            SpecifiedFloat::InlineStart => {
                context
                    .rule_cache_conditions
                    .borrow_mut()
                    .set_writing_mode_dependency(context.builder.writing_mode);
                if ltr {
                    Float::Left
                } else {
                    Float::Right
                }
            },
            SpecifiedFloat::InlineEnd => {
                context
                    .rule_cache_conditions
                    .borrow_mut()
                    .set_writing_mode_dependency(context.builder.writing_mode);
                if ltr {
                    Float::Right
                } else {
                    Float::Left
                }
            },
            SpecifiedFloat::Left => Float::Left,
            SpecifiedFloat::Right => Float::Right,
            SpecifiedFloat::None => Float::None,
        }
    }

    #[inline]
    fn from_computed_value(computed: &Self::ComputedValue) -> SpecifiedFloat {
        match *computed {
            Float::Left => SpecifiedFloat::Left,
            Float::Right => SpecifiedFloat::Right,
            Float::None => SpecifiedFloat::None,
        }
    }
}

#[allow(missing_docs)]
#[cfg_attr(feature = "servo", derive(Deserialize, Serialize))]
#[derive(
    Clone,
    Copy,
    Debug,
    Eq,
    FromPrimitive,
    Hash,
    MallocSizeOf,
    Parse,
    PartialEq,
    SpecifiedValueInfo,
    ToCss,
    ToResolvedValue,
)]
/// A computed value for the `clear` property.
#[repr(u8)]
pub enum Clear {
    None,
    Left,
    Right,
    Both,
}

impl ToComputedValue for SpecifiedClear {
    type ComputedValue = Clear;

    #[inline]
    fn to_computed_value(&self, context: &Context) -> Self::ComputedValue {
        let ltr = context.style().writing_mode.is_bidi_ltr();
        // https://drafts.csswg.org/css-logical-props/#float-clear
        match *self {
            SpecifiedClear::InlineStart => {
                context
                    .rule_cache_conditions
                    .borrow_mut()
                    .set_writing_mode_dependency(context.builder.writing_mode);
                if ltr {
                    Clear::Left
                } else {
                    Clear::Right
                }
            },
            SpecifiedClear::InlineEnd => {
                context
                    .rule_cache_conditions
                    .borrow_mut()
                    .set_writing_mode_dependency(context.builder.writing_mode);
                if ltr {
                    Clear::Right
                } else {
                    Clear::Left
                }
            },
            SpecifiedClear::None => Clear::None,
            SpecifiedClear::Left => Clear::Left,
            SpecifiedClear::Right => Clear::Right,
            SpecifiedClear::Both => Clear::Both,
        }
    }

    #[inline]
    fn from_computed_value(computed: &Self::ComputedValue) -> SpecifiedClear {
        match *computed {
            Clear::None => SpecifiedClear::None,
            Clear::Left => SpecifiedClear::Left,
            Clear::Right => SpecifiedClear::Right,
            Clear::Both => SpecifiedClear::Both,
        }
    }
}

/// A computed value for the `resize` property.
#[allow(missing_docs)]
#[cfg_attr(feature = "servo", derive(Deserialize, Serialize))]
#[derive(Clone, Copy, Debug, Eq, Hash, MallocSizeOf, Parse, PartialEq, ToCss, ToResolvedValue)]
#[repr(u8)]
pub enum Resize {
    None,
    Both,
    Horizontal,
    Vertical,
}

impl ToComputedValue for specified::Resize {
    type ComputedValue = Resize;

    #[inline]
    fn to_computed_value(&self, context: &Context) -> Resize {
        let is_vertical = context.style().writing_mode.is_vertical();
        match self {
            specified::Resize::Inline => {
                context
                    .rule_cache_conditions
                    .borrow_mut()
                    .set_writing_mode_dependency(context.builder.writing_mode);
                if is_vertical {
                    Resize::Vertical
                } else {
                    Resize::Horizontal
                }
            },
            specified::Resize::Block => {
                context
                    .rule_cache_conditions
                    .borrow_mut()
                    .set_writing_mode_dependency(context.builder.writing_mode);
                if is_vertical {
                    Resize::Horizontal
                } else {
                    Resize::Vertical
                }
            },
            specified::Resize::None => Resize::None,
            specified::Resize::Both => Resize::Both,
            specified::Resize::Horizontal => Resize::Horizontal,
            specified::Resize::Vertical => Resize::Vertical,
        }
    }

    #[inline]
    fn from_computed_value(computed: &Resize) -> specified::Resize {
        match computed {
            Resize::None => specified::Resize::None,
            Resize::Both => specified::Resize::Both,
            Resize::Horizontal => specified::Resize::Horizontal,
            Resize::Vertical => specified::Resize::Vertical,
        }
    }
}

/// We use an unsigned 10.6 fixed-point value (range 0.0 - 1023.984375).
pub const ZOOM_FRACTION_BITS: u16 = 6;

/// This is an alias which is useful mostly as a cbindgen / C++ inference workaround.
pub type ZoomFixedPoint = FixedPoint<u16, ZOOM_FRACTION_BITS>;

/// The computed `zoom` property value. We store it as a 16-bit fixed point because we need to
/// store it efficiently in the ComputedStyle representation. The assumption being that zooms over
/// 1000 aren't quite useful.
#[derive(
    Clone,
    ComputeSquaredDistance,
    Copy,
    Debug,
    Hash,
    MallocSizeOf,
    PartialEq,
    PartialOrd,
    ToResolvedValue,
)]
#[cfg_attr(feature = "servo", derive(Deserialize, Serialize))]
#[repr(C)]
pub struct Zoom(ZoomFixedPoint);

impl ToComputedValue for specified::Zoom {
    type ComputedValue = Zoom;

    #[inline]
    fn to_computed_value(&self, _: &Context) -> Self::ComputedValue {
        let n = match *self {
            Self::Normal => return Zoom::ONE,
            Self::Document => return Zoom::DOCUMENT,
            Self::Value(ref n) => n.0.to_number().get(),
        };
        if n == 0.0 {
            // For legacy reasons, zoom: 0 (and 0%) computes to 1. ¯\_(ツ)_/¯
            return Zoom::ONE;
        }
        Zoom(ZoomFixedPoint::from_float(n))
    }

    #[inline]
    fn from_computed_value(computed: &Self::ComputedValue) -> Self {
        Self::new_number(computed.value())
    }
}

impl ToCss for Zoom {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: fmt::Write,
    {
        use std::fmt::Write;
        if *self == Self::DOCUMENT {
            return dest.write_str("document");
        }
        self.value().to_css(dest)
    }
}

impl ToAnimatedValue for Zoom {
    type AnimatedValue = Number;

    #[inline]
    fn to_animated_value(self) -> Self::AnimatedValue {
        self.value()
    }

    #[inline]
    fn from_animated_value(animated: Self::AnimatedValue) -> Self {
        Zoom(ZoomFixedPoint::from_float(animated.max(0.0)))
    }
}

impl Zoom {
    /// The value 1. This is by far the most common value.
    pub const ONE: Zoom = Zoom(ZoomFixedPoint {
        value: 1 << ZOOM_FRACTION_BITS,
    });

    /// The `document` value. This can appear in the computed zoom property value, but not in the
    /// `effective_zoom` field.
    pub const DOCUMENT: Zoom = Zoom(ZoomFixedPoint { value: 0 });

    /// Returns whether we're the number 1.
    #[inline]
    pub fn is_one(self) -> bool {
        self == Self::ONE
    }

    /// Returns the value as a float.
    #[inline]
    pub fn value(&self) -> f32 {
        self.0.to_float()
    }

    /// Computes the effective zoom for a given new zoom value in rhs.
    pub fn compute_effective(self, specified: Self) -> Self {
        if specified == Self::DOCUMENT {
            return Self::ONE;
        }
        if self == Self::ONE {
            return specified;
        }
        if specified == Self::ONE {
            return self;
        }
        Zoom(self.0 * specified.0)
    }

    /// Returns the zoomed value.
    #[inline]
    pub fn zoom(self, value: f32) -> f32 {
        if self == Self::ONE {
            return value;
        }
        self.value() * value
    }
}
