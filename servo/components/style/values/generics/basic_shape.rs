/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! CSS handling for the [`basic-shape`](https://drafts.csswg.org/css-shapes/#typedef-basic-shape)
//! types that are generic over their `ToCss` implementations.

use crate::values::animated::{lists, Animate, Procedure, ToAnimatedZero};
use crate::values::distance::{ComputeSquaredDistance, SquaredDistance};
use crate::values::generics::border::GenericBorderRadius;
use crate::values::generics::position::GenericPositionOrAuto;
use crate::values::generics::rect::Rect;
use crate::values::specified::SVGPathData;
use crate::Zero;
use std::fmt::{self, Write};
use style_traits::{CssWriter, ToCss};

/// <https://drafts.fxtf.org/css-masking-1/#typedef-geometry-box>
#[allow(missing_docs)]
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Copy,
    Debug,
    MallocSizeOf,
    PartialEq,
    Parse,
    SpecifiedValueInfo,
    ToAnimatedValue,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[repr(u8)]
pub enum ShapeGeometryBox {
    /// Depending on which kind of element this style value applied on, the
    /// default value of the reference-box can be different.  For an HTML
    /// element, the default value of reference-box is border-box; for an SVG
    /// element, the default value is fill-box.  Since we can not determine the
    /// default value at parsing time, we keep this value to make a decision on
    /// it.
    #[css(skip)]
    ElementDependent,
    FillBox,
    StrokeBox,
    ViewBox,
    ShapeBox(ShapeBox),
}

impl Default for ShapeGeometryBox {
    fn default() -> Self {
        Self::ElementDependent
    }
}

/// Skip the serialization if the author omits the box or specifies border-box.
#[inline]
fn is_default_box_for_clip_path(b: &ShapeGeometryBox) -> bool {
    // Note: for clip-path, ElementDependent is always border-box, so we have to check both of them
    // for serialization.
    matches!(b, ShapeGeometryBox::ElementDependent) ||
        matches!(b, ShapeGeometryBox::ShapeBox(ShapeBox::BorderBox))
}

/// https://drafts.csswg.org/css-shapes-1/#typedef-shape-box
#[allow(missing_docs)]
#[cfg_attr(feature = "servo", derive(Deserialize, Serialize))]
#[derive(
    Animate,
    Clone,
    Copy,
    ComputeSquaredDistance,
    Debug,
    Eq,
    MallocSizeOf,
    Parse,
    PartialEq,
    SpecifiedValueInfo,
    ToAnimatedValue,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[repr(u8)]
pub enum ShapeBox {
    MarginBox,
    BorderBox,
    PaddingBox,
    ContentBox,
}

impl Default for ShapeBox {
    fn default() -> Self {
        ShapeBox::MarginBox
    }
}

/// A value for the `clip-path` property.
#[allow(missing_docs)]
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Debug,
    MallocSizeOf,
    PartialEq,
    SpecifiedValueInfo,
    ToAnimatedValue,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[animation(no_bound(U))]
#[repr(u8)]
pub enum GenericClipPath<BasicShape, U> {
    #[animation(error)]
    None,
    #[animation(error)]
    Url(U),
    Shape(
        Box<BasicShape>,
        #[css(skip_if = "is_default_box_for_clip_path")] ShapeGeometryBox,
    ),
    #[animation(error)]
    Box(ShapeGeometryBox),
}

pub use self::GenericClipPath as ClipPath;

/// A value for the `shape-outside` property.
#[allow(missing_docs)]
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Debug,
    MallocSizeOf,
    PartialEq,
    SpecifiedValueInfo,
    ToAnimatedValue,
    ToComputedValue,
    ToCss,
    ToResolvedValue,
    ToShmem,
)]
#[animation(no_bound(I))]
#[repr(u8)]
pub enum GenericShapeOutside<BasicShape, I> {
    #[animation(error)]
    None,
    #[animation(error)]
    Image(I),
    Shape(Box<BasicShape>, #[css(skip_if = "is_default")] ShapeBox),
    #[animation(error)]
    Box(ShapeBox),
}

pub use self::GenericShapeOutside as ShapeOutside;

/// The <basic-shape>.
///
/// https://drafts.csswg.org/css-shapes-1/#supported-basic-shapes
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
pub enum GenericBasicShape<
    Angle,
    Position,
    LengthPercentage,
    NonNegativeLengthPercentage,
    BasicShapeRect,
> {
    /// The <basic-shape-rect>.
    Rect(BasicShapeRect),
    /// Defines a circle with a center and a radius.
    Circle(
        #[css(field_bound)]
        #[shmem(field_bound)]
        Circle<Position, NonNegativeLengthPercentage>,
    ),
    /// Defines an ellipse with a center and x-axis/y-axis radii.
    Ellipse(
        #[css(field_bound)]
        #[shmem(field_bound)]
        Ellipse<Position, NonNegativeLengthPercentage>,
    ),
    /// Defines a polygon with pair arguments.
    Polygon(GenericPolygon<LengthPercentage>),
    /// Defines a path with SVG path syntax.
    Path(Path),
    /// Defines a shape function, which is identical to path(() but it uses the CSS syntax.
    Shape(#[css(field_bound)] Shape<Angle, LengthPercentage>),
}

pub use self::GenericBasicShape as BasicShape;

/// <https://drafts.csswg.org/css-shapes/#funcdef-inset>
#[allow(missing_docs)]
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
#[css(function = "inset")]
#[repr(C)]
pub struct GenericInsetRect<LengthPercentage, NonNegativeLengthPercentage> {
    pub rect: Rect<LengthPercentage>,
    #[shmem(field_bound)]
    pub round: GenericBorderRadius<NonNegativeLengthPercentage>,
}

pub use self::GenericInsetRect as InsetRect;

/// <https://drafts.csswg.org/css-shapes/#funcdef-circle>
#[allow(missing_docs)]
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Copy,
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
#[css(function)]
#[repr(C)]
pub struct Circle<Position, NonNegativeLengthPercentage> {
    pub position: GenericPositionOrAuto<Position>,
    pub radius: GenericShapeRadius<NonNegativeLengthPercentage>,
}

/// <https://drafts.csswg.org/css-shapes/#funcdef-ellipse>
#[allow(missing_docs)]
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Copy,
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
#[css(function)]
#[repr(C)]
pub struct Ellipse<Position, NonNegativeLengthPercentage> {
    pub position: GenericPositionOrAuto<Position>,
    pub semiaxis_x: GenericShapeRadius<NonNegativeLengthPercentage>,
    pub semiaxis_y: GenericShapeRadius<NonNegativeLengthPercentage>,
}

/// <https://drafts.csswg.org/css-shapes/#typedef-shape-radius>
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
#[repr(C, u8)]
pub enum GenericShapeRadius<NonNegativeLengthPercentage> {
    Length(NonNegativeLengthPercentage),
    #[animation(error)]
    ClosestSide,
    #[animation(error)]
    FarthestSide,
}

pub use self::GenericShapeRadius as ShapeRadius;

/// A generic type for representing the `polygon()` function
///
/// <https://drafts.csswg.org/css-shapes/#funcdef-polygon>
#[derive(
    Clone,
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
#[css(comma, function = "polygon")]
#[repr(C)]
pub struct GenericPolygon<LengthPercentage> {
    /// The filling rule for a polygon.
    #[css(skip_if = "is_default")]
    pub fill: FillRule,
    /// A collection of (x, y) coordinates to draw the polygon.
    #[css(iterable)]
    pub coordinates: crate::OwnedSlice<PolygonCoord<LengthPercentage>>,
}

pub use self::GenericPolygon as Polygon;

/// Coordinates for Polygon.
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
#[repr(C)]
pub struct PolygonCoord<LengthPercentage>(pub LengthPercentage, pub LengthPercentage);

// https://drafts.csswg.org/css-shapes/#typedef-fill-rule
// NOTE: Basic shapes spec says that these are the only two values, however
// https://www.w3.org/TR/SVG/painting.html#FillRuleProperty
// says that it can also be `inherit`
#[allow(missing_docs)]
#[cfg_attr(feature = "servo", derive(Deserialize, Serialize))]
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Copy,
    Debug,
    Deserialize,
    Eq,
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
pub enum FillRule {
    Nonzero,
    Evenodd,
}

/// The path function.
///
/// https://drafts.csswg.org/css-shapes-1/#funcdef-basic-shape-path
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
#[css(comma, function = "path")]
#[repr(C)]
pub struct Path {
    /// The filling rule for the svg path.
    #[css(skip_if = "is_default")]
    pub fill: FillRule,
    /// The svg path data.
    pub path: SVGPathData,
}

impl<B, U> ToAnimatedZero for ClipPath<B, U> {
    fn to_animated_zero(&self) -> Result<Self, ()> {
        Err(())
    }
}

impl<B, U> ToAnimatedZero for ShapeOutside<B, U> {
    fn to_animated_zero(&self) -> Result<Self, ()> {
        Err(())
    }
}

impl<Length, NonNegativeLength> ToCss for InsetRect<Length, NonNegativeLength>
where
    Length: ToCss + PartialEq,
    NonNegativeLength: ToCss + PartialEq + Zero,
{
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        dest.write_str("inset(")?;
        self.rect.to_css(dest)?;
        if !self.round.is_zero() {
            dest.write_str(" round ")?;
            self.round.to_css(dest)?;
        }
        dest.write_char(')')
    }
}

impl<Position, NonNegativeLengthPercentage> ToCss for Circle<Position, NonNegativeLengthPercentage>
where
    Position: ToCss,
    NonNegativeLengthPercentage: ToCss + PartialEq,
{
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        let has_radius = self.radius != Default::default();

        dest.write_str("circle(")?;
        if has_radius {
            self.radius.to_css(dest)?;
        }

        // Preserve the `at <position>` even if it specified the default value.
        // https://github.com/w3c/csswg-drafts/issues/8695
        if !matches!(self.position, GenericPositionOrAuto::Auto) {
            if has_radius {
                dest.write_char(' ')?;
            }
            dest.write_str("at ")?;
            self.position.to_css(dest)?;
        }
        dest.write_char(')')
    }
}

impl<Position, NonNegativeLengthPercentage> ToCss for Ellipse<Position, NonNegativeLengthPercentage>
where
    Position: ToCss,
    NonNegativeLengthPercentage: ToCss + PartialEq,
{
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        let has_radii =
            self.semiaxis_x != Default::default() || self.semiaxis_y != Default::default();

        dest.write_str("ellipse(")?;
        if has_radii {
            self.semiaxis_x.to_css(dest)?;
            dest.write_char(' ')?;
            self.semiaxis_y.to_css(dest)?;
        }

        // Preserve the `at <position>` even if it specified the default value.
        // https://github.com/w3c/csswg-drafts/issues/8695
        if !matches!(self.position, GenericPositionOrAuto::Auto) {
            if has_radii {
                dest.write_char(' ')?;
            }
            dest.write_str("at ")?;
            self.position.to_css(dest)?;
        }
        dest.write_char(')')
    }
}

impl<L> Default for ShapeRadius<L> {
    #[inline]
    fn default() -> Self {
        ShapeRadius::ClosestSide
    }
}

impl<L> Animate for Polygon<L>
where
    L: Animate,
{
    fn animate(&self, other: &Self, procedure: Procedure) -> Result<Self, ()> {
        if self.fill != other.fill {
            return Err(());
        }
        let coordinates =
            lists::by_computed_value::animate(&self.coordinates, &other.coordinates, procedure)?;
        Ok(Polygon {
            fill: self.fill,
            coordinates,
        })
    }
}

impl<L> ComputeSquaredDistance for Polygon<L>
where
    L: ComputeSquaredDistance,
{
    fn compute_squared_distance(&self, other: &Self) -> Result<SquaredDistance, ()> {
        if self.fill != other.fill {
            return Err(());
        }
        lists::by_computed_value::squared_distance(&self.coordinates, &other.coordinates)
    }
}

impl Default for FillRule {
    #[inline]
    fn default() -> Self {
        FillRule::Nonzero
    }
}

#[inline]
fn is_default<T: Default + PartialEq>(fill: &T) -> bool {
    *fill == Default::default()
}

/// The shape function defined in css-shape-2.
/// shape() = shape(<fill-rule>? from <coordinate-pair>, <shape-command>#)
///
/// https://drafts.csswg.org/css-shapes-2/#shape-function
#[derive(
    Clone,
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
pub struct Shape<Angle, LengthPercentage> {
    /// The filling rule for this shape.
    pub fill: FillRule,
    /// The shape command data. Note that the starting point will be the first command in this
    /// slice.
    // Note: The first command is always GenericShapeCommand::Move.
    pub commands: crate::OwnedSlice<GenericShapeCommand<Angle, LengthPercentage>>,
}

impl<Angle, LengthPercentage> Animate for Shape<Angle, LengthPercentage>
where
    Angle: Animate,
    LengthPercentage: Animate,
{
    fn animate(&self, other: &Self, procedure: Procedure) -> Result<Self, ()> {
        if self.fill != other.fill {
            return Err(());
        }
        let commands =
            lists::by_computed_value::animate(&self.commands, &other.commands, procedure)?;
        Ok(Self {
            fill: self.fill,
            commands,
        })
    }
}

impl<Angle, LengthPercentage> ComputeSquaredDistance for Shape<Angle, LengthPercentage>
where
    Angle: ComputeSquaredDistance,
    LengthPercentage: ComputeSquaredDistance,
{
    fn compute_squared_distance(&self, other: &Self) -> Result<SquaredDistance, ()> {
        if self.fill != other.fill {
            return Err(());
        }
        lists::by_computed_value::squared_distance(&self.commands, &other.commands)
    }
}

impl<Angle, LengthPercentage> ToCss for Shape<Angle, LengthPercentage>
where
    Angle: ToCss + Zero,
    LengthPercentage: PartialEq + ToCss,
{
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        use style_traits::values::SequenceWriter;

        // Per spec, we must have the first move command and at least one following command.
        debug_assert!(self.commands.len() > 1);

        dest.write_str("shape(")?;
        if !is_default(&self.fill) {
            self.fill.to_css(dest)?;
            dest.write_char(' ')?;
        }
        dest.write_str("from ")?;
        match self.commands[0] {
            ShapeCommand::Move {
                by_to: _,
                ref point,
            } => point.to_css(dest)?,
            _ => unreachable!("The first command must be move"),
        }
        dest.write_str(", ")?;
        {
            let mut writer = SequenceWriter::new(dest, ", ");
            for command in self.commands.iter().skip(1) {
                writer.item(command)?;
            }
        }
        dest.write_char(')')
    }
}

/// This is a more general shape(path) command type, for both shape() and path().
///
/// https://www.w3.org/TR/SVG11/paths.html#PathData
/// https://drafts.csswg.org/css-shapes-2/#shape-function
#[derive(
    Animate,
    Clone,
    ComputeSquaredDistance,
    Copy,
    Debug,
    Deserialize,
    MallocSizeOf,
    PartialEq,
    Serialize,
    SpecifiedValueInfo,
    ToAnimatedValue,
    ToAnimatedZero,
    ToComputedValue,
    ToResolvedValue,
    ToShmem,
)]
#[allow(missing_docs)]
#[repr(C, u8)]
pub enum GenericShapeCommand<Angle, LengthPercentage> {
    /// The move command.
    Move {
        by_to: ByTo,
        point: CoordinatePair<LengthPercentage>,
    },
    /// The line command.
    Line {
        by_to: ByTo,
        point: CoordinatePair<LengthPercentage>,
    },
    /// The hline command.
    HLine { by_to: ByTo, x: LengthPercentage },
    /// The vline command.
    VLine { by_to: ByTo, y: LengthPercentage },
    /// The cubic Bézier curve command.
    CubicCurve {
        by_to: ByTo,
        point: CoordinatePair<LengthPercentage>,
        control1: CoordinatePair<LengthPercentage>,
        control2: CoordinatePair<LengthPercentage>,
    },
    /// The quadratic Bézier curve command.
    QuadCurve {
        by_to: ByTo,
        point: CoordinatePair<LengthPercentage>,
        control1: CoordinatePair<LengthPercentage>,
    },
    /// The smooth command.
    SmoothCubic {
        by_to: ByTo,
        point: CoordinatePair<LengthPercentage>,
        control2: CoordinatePair<LengthPercentage>,
    },
    /// The smooth quadratic Bézier curve command.
    SmoothQuad {
        by_to: ByTo,
        point: CoordinatePair<LengthPercentage>,
    },
    /// The arc command.
    Arc {
        by_to: ByTo,
        point: CoordinatePair<LengthPercentage>,
        radii: CoordinatePair<LengthPercentage>,
        arc_sweep: ArcSweep,
        arc_size: ArcSize,
        rotate: Angle,
    },
    /// The closepath command.
    Close,
}

pub use self::GenericShapeCommand as ShapeCommand;

impl<Angle, LengthPercentage> ToCss for ShapeCommand<Angle, LengthPercentage>
where
    Angle: ToCss + Zero,
    LengthPercentage: PartialEq + ToCss,
{
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: fmt::Write,
    {
        use self::ShapeCommand::*;
        match *self {
            Move { by_to, ref point } => {
                dest.write_str("move ")?;
                by_to.to_css(dest)?;
                dest.write_char(' ')?;
                point.to_css(dest)
            },
            Line { by_to, ref point } => {
                dest.write_str("line ")?;
                by_to.to_css(dest)?;
                dest.write_char(' ')?;
                point.to_css(dest)
            },
            HLine { by_to, ref x } => {
                dest.write_str("hline ")?;
                by_to.to_css(dest)?;
                dest.write_char(' ')?;
                x.to_css(dest)
            },
            VLine { by_to, ref y } => {
                dest.write_str("vline ")?;
                by_to.to_css(dest)?;
                dest.write_char(' ')?;
                y.to_css(dest)
            },
            CubicCurve {
                by_to,
                ref point,
                ref control1,
                ref control2,
            } => {
                dest.write_str("curve ")?;
                by_to.to_css(dest)?;
                dest.write_char(' ')?;
                point.to_css(dest)?;
                dest.write_str(" via ")?;
                control1.to_css(dest)?;
                dest.write_char(' ')?;
                control2.to_css(dest)
            },
            QuadCurve {
                by_to,
                ref point,
                ref control1,
            } => {
                dest.write_str("curve ")?;
                by_to.to_css(dest)?;
                dest.write_char(' ')?;
                point.to_css(dest)?;
                dest.write_str(" via ")?;
                control1.to_css(dest)
            },
            SmoothCubic {
                by_to,
                ref point,
                ref control2,
            } => {
                dest.write_str("smooth ")?;
                by_to.to_css(dest)?;
                dest.write_char(' ')?;
                point.to_css(dest)?;
                dest.write_str(" via ")?;
                control2.to_css(dest)
            },
            SmoothQuad { by_to, ref point } => {
                dest.write_str("smooth ")?;
                by_to.to_css(dest)?;
                dest.write_char(' ')?;
                point.to_css(dest)
            },
            Arc {
                by_to,
                ref point,
                ref radii,
                arc_sweep,
                arc_size,
                ref rotate,
            } => {
                dest.write_str("arc ")?;
                by_to.to_css(dest)?;
                dest.write_char(' ')?;
                point.to_css(dest)?;
                dest.write_str(" of ")?;
                radii.x.to_css(dest)?;
                if radii.x != radii.y {
                    dest.write_char(' ')?;
                    radii.y.to_css(dest)?;
                }

                if matches!(arc_sweep, ArcSweep::Cw) {
                    dest.write_str(" cw")?;
                }

                if matches!(arc_size, ArcSize::Large) {
                    dest.write_str(" large")?;
                }

                if !rotate.is_zero() {
                    dest.write_str(" rotate ")?;
                    rotate.to_css(dest)?;
                }
                Ok(())
            },
            Close => dest.write_str("close"),
        }
    }
}

/// This indicates the command is absolute or relative.
/// https://drafts.csswg.org/css-shapes-2/#typedef-shape-by-to
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
#[repr(u8)]
pub enum ByTo {
    /// This indicates that the <coordinate-pair>s are relative to the command’s starting point.
    By,
    /// This relative to the top-left corner of the reference box.
    To,
}

impl ByTo {
    /// Return true if it is absolute, i.e. it is To.
    #[inline]
    pub fn is_abs(&self) -> bool {
        matches!(self, ByTo::To)
    }

    /// Create ByTo based on the flag if it is absolute.
    #[inline]
    pub fn new(is_abs: bool) -> Self {
        if is_abs {
            Self::To
        } else {
            Self::By
        }
    }
}

/// Defines a pair of coordinates, representing a rightward and downward offset, respectively, from
/// a specified reference point. Percentages are resolved against the width or height,
/// respectively, of the reference box.
/// https://drafts.csswg.org/css-shapes-2/#typedef-shape-coordinate-pair
#[allow(missing_docs)]
#[derive(
    AddAssign,
    Animate,
    Clone,
    ComputeSquaredDistance,
    Copy,
    Debug,
    Deserialize,
    MallocSizeOf,
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
#[repr(C)]
pub struct CoordinatePair<LengthPercentage> {
    pub x: LengthPercentage,
    pub y: LengthPercentage,
}

impl<LengthPercentage> CoordinatePair<LengthPercentage> {
    /// Create a CoordinatePair.
    #[inline]
    pub fn new(x: LengthPercentage, y: LengthPercentage) -> Self {
        Self { x, y }
    }
}

/// This indicates that the arc that is traced around the ellipse clockwise or counter-clockwise
/// from the center.
/// https://drafts.csswg.org/css-shapes-2/#typedef-shape-arc-sweep
#[derive(
    Clone,
    Copy,
    Debug,
    Deserialize,
    FromPrimitive,
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
#[repr(u8)]
pub enum ArcSweep {
    /// Counter-clockwise. The default value. (This also represents 0 in the svg path.)
    Ccw = 0,
    /// Clockwise. (This also represents 1 in the svg path.)
    Cw = 1,
}

impl Animate for ArcSweep {
    fn animate(&self, other: &Self, procedure: Procedure) -> Result<Self, ()> {
        use num_traits::FromPrimitive;
        // If an arc command has different <arc-sweep> between its starting and ending list, then
        // the interpolated result uses cw for any progress value between 0 and 1.
        (*self as i32)
            .animate(&(*other as i32), procedure)
            .map(|v| ArcSweep::from_u8((v > 0) as u8).unwrap_or(ArcSweep::Ccw))
    }
}

impl ComputeSquaredDistance for ArcSweep {
    fn compute_squared_distance(&self, other: &Self) -> Result<SquaredDistance, ()> {
        (*self as i32).compute_squared_distance(&(*other as i32))
    }
}

/// This indicates that the larger or smaller, respectively, of the two possible arcs must be
/// chosen.
/// https://drafts.csswg.org/css-shapes-2/#typedef-shape-arc-size
#[derive(
    Clone,
    Copy,
    Debug,
    Deserialize,
    FromPrimitive,
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
#[repr(u8)]
pub enum ArcSize {
    /// Choose the small one. The default value. (This also represents 0 in the svg path.)
    Small = 0,
    /// Choose the large one. (This also represents 1 in the svg path.)
    Large = 1,
}

impl Animate for ArcSize {
    fn animate(&self, other: &Self, procedure: Procedure) -> Result<Self, ()> {
        use num_traits::FromPrimitive;
        // If it has different <arc-size> keywords, then the interpolated result uses large for any
        // progress value between 0 and 1.
        (*self as i32)
            .animate(&(*other as i32), procedure)
            .map(|v| ArcSize::from_u8((v > 0) as u8).unwrap_or(ArcSize::Small))
    }
}

impl ComputeSquaredDistance for ArcSize {
    fn compute_squared_distance(&self, other: &Self) -> Result<SquaredDistance, ()> {
        (*self as i32).compute_squared_distance(&(*other as i32))
    }
}
