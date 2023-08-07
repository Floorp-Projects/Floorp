/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Generic types for CSS Motion Path.

use crate::values::animated::ToAnimatedZero;
use crate::values::generics::position::{GenericPosition, GenericPositionOrAuto};
use crate::values::specified::motion::CoordBox;
use serde::Deserializer;
use std::fmt::{self, Write};
use style_traits::{CssWriter, ToCss};

/// The <size> in ray() function.
///
/// https://drafts.fxtf.org/motion-1/#valdef-offsetpath-size
#[allow(missing_docs)]
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Copy,
    Debug,
    Deserialize,
    MallocSizeOf,
    Parse,
    PartialEq,
    Serialize,
    SpecifiedValueInfo,
    ToAnimatedValue,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[repr(u8)]
pub enum RaySize {
    ClosestSide,
    ClosestCorner,
    FarthestSide,
    FarthestCorner,
    Sides,
}

/// The `ray()` function, `ray( [ <angle> && <size> && contain? && [at <position>]? ] )`
///
/// https://drafts.fxtf.org/motion-1/#valdef-offsetpath-ray
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Debug,
    Deserialize,
    MallocSizeOf,
    PartialEq,
    Serialize,
    SpecifiedValueInfo,
    ToAnimatedValue,
    ToComputedValue,
    ToResolvedValue,
    ToShmem,
)]
#[repr(C)]
pub struct GenericRayFunction<Angle, Position> {
    /// The bearing angle with `0deg` pointing up and positive angles
    /// representing clockwise rotation.
    pub angle: Angle,
    /// Decide the path length used when `offset-distance` is expressed
    /// as a percentage.
    pub size: RaySize,
    /// Clamp `offset-distance` so that the box is entirely contained
    /// within the path.
    #[animation(constant)]
    pub contain: bool,
    /// The "at <position>" part. If omitted, we use auto to represent it.
    pub position: GenericPositionOrAuto<Position>,
}

pub use self::GenericRayFunction as RayFunction;

impl<Angle, Position> ToCss for RayFunction<Angle, Position>
where
    Angle: ToCss,
    Position: ToCss,
{
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        self.angle.to_css(dest)?;

        if !matches!(self.size, RaySize::ClosestSide) {
            dest.write_char(' ')?;
            self.size.to_css(dest)?;
        }

        if self.contain {
            dest.write_str(" contain")?;
        }

        if !matches!(self.position, GenericPositionOrAuto::Auto) {
            dest.write_str(" at ")?;
            self.position.to_css(dest)?;
        }

        Ok(())
    }
}

/// Return error if we try to deserialize the url, for Gecko IPC purposes.
// Note: we cannot use #[serde(skip_deserializing)] variant attribute, which may cause the fatal
// error when trying to read the parameters because it cannot deserialize the input byte buffer,
// even if the type of OffsetPathFunction is not an url(), in our tests. This may be an issue of
// #[serde(skip_deserializing)] on enum, at least in the version (1.0) we are using. So we have to
// manually implement this deseriailzing function, but return error.
// FIXME: Bug 1847620, fiure out this is a serde issue or a gecko bug.
fn deserialize_url<'de, D, T>(_deserializer: D) -> Result<T, D::Error>
where
    D: Deserializer<'de>,
{
    use crate::serde::de::Error;
    // Return Err() so the IPC will catch it and assert this as a fetal error.
    Err(<D as Deserializer>::Error::custom("we don't support the deserializing for url"))
}

/// The <offset-path> value.
/// <offset-path> = <ray()> | <url> | <basic-shape>
///
/// https://drafts.fxtf.org/motion-1/#typedef-offset-path
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Debug,
    Deserialize,
    MallocSizeOf,
    PartialEq,
    Serialize,
    SpecifiedValueInfo,
    ToAnimatedValue,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[animation(no_bound(U))]
#[repr(C, u8)]
pub enum GenericOffsetPathFunction<Shapes, RayFunction, U> {
    /// ray() function, which defines a path in the polar coordinate system.
    /// Use Box<> to make sure the size of offset-path is not too large.
    #[css(function)]
    Ray(RayFunction),
    /// A URL reference to an SVG shape element. If the URL does not reference a shape element,
    /// this behaves as path("m 0 0") instead.
    #[animation(error)]
    #[serde(deserialize_with = "deserialize_url")]
    #[serde(skip_serializing)]
    Url(U),
    /// The <basic-shape> value.
    Shape(Shapes),
}

pub use self::GenericOffsetPathFunction as OffsetPathFunction;

/// The offset-path property.
/// offset-path: none | <offset-path> || <coord-box>
///
/// https://drafts.fxtf.org/motion-1/#offset-path-property
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Debug,
    Deserialize,
    MallocSizeOf,
    PartialEq,
    Serialize,
    SpecifiedValueInfo,
    ToAnimatedValue,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[repr(C, u8)]
pub enum GenericOffsetPath<Function> {
    /// <offset-path> || <coord-box>.
    OffsetPath {
        /// <offset-path> part.
        // Note: Use Box<> to make sure the size of this property doesn't go over the threshold.
        path: Box<Function>,
        /// <coord-box> part.
        #[css(skip_if = "CoordBox::is_default")]
        coord_box: CoordBox,
    },
    /// Only <coord-box>. This represents that <offset-path> is omitted, so we use the default
    /// value, inset(0 round X), where X is the value of border-radius on the element that
    /// establishes the containing block for this element.
    CoordBox(CoordBox),
    /// None value.
    #[animation(error)]
    None,
}

pub use self::GenericOffsetPath as OffsetPath;

impl<Function> OffsetPath<Function> {
    /// Return None.
    #[inline]
    pub fn none() -> Self {
        OffsetPath::None
    }
}

impl<Function> ToAnimatedZero for OffsetPath<Function> {
    #[inline]
    fn to_animated_zero(&self) -> Result<Self, ()> {
        Err(())
    }
}

/// The offset-position property, which specifies the offset starting position that is used by the
/// <offset-path> functions if they don’t specify their own starting position.
///
/// https://drafts.fxtf.org/motion-1/#offset-position-property
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Copy,
    Debug,
    Deserialize,
    MallocSizeOf,
    Parse,
    PartialEq,
    Serialize,
    SpecifiedValueInfo,
    ToAnimatedValue,
    ToAnimatedZero,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[repr(C, u8)]
pub enum GenericOffsetPosition<H, V> {
    /// The element does not have an offset starting position.
    Normal,
    /// The offset starting position is the top-left corner of the box.
    Auto,
    /// The offset starting position is the result of using the <position> to position a 0x0 object
    /// area within the box’s containing block.
    Position(
        #[css(field_bound)]
        #[parse(field_bound)]
        GenericPosition<H, V>,
    ),
}

pub use self::GenericOffsetPosition as OffsetPosition;

impl<H, V> OffsetPosition<H, V> {
    /// Returns the initial value, normal.
    #[inline]
    pub fn normal() -> Self {
        Self::Normal
    }
}
