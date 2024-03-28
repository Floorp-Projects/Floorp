/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! CSS handling for the computed value of
//! [`basic-shape`][basic-shape]s
//!
//! [basic-shape]: https://drafts.csswg.org/css-shapes/#typedef-basic-shape

use crate::values::animated::{Animate, Procedure};
use crate::values::computed::angle::Angle;
use crate::values::computed::url::ComputedUrl;
use crate::values::computed::{Image, LengthPercentage, NonNegativeLengthPercentage, Position};
use crate::values::generics::basic_shape as generic;
use crate::values::specified::svg_path::{CoordPair, PathCommand};

/// A computed alias for FillRule.
pub use crate::values::generics::basic_shape::FillRule;

/// A computed `clip-path` value.
pub type ClipPath = generic::GenericClipPath<BasicShape, ComputedUrl>;

/// A computed `shape-outside` value.
pub type ShapeOutside = generic::GenericShapeOutside<BasicShape, Image>;

/// A computed basic shape.
pub type BasicShape = generic::GenericBasicShape<
    Angle,
    Position,
    LengthPercentage,
    NonNegativeLengthPercentage,
    InsetRect,
>;

/// The computed value of `inset()`.
pub type InsetRect = generic::GenericInsetRect<LengthPercentage, NonNegativeLengthPercentage>;

/// A computed circle.
pub type Circle = generic::Circle<Position, NonNegativeLengthPercentage>;

/// A computed ellipse.
pub type Ellipse = generic::Ellipse<Position, NonNegativeLengthPercentage>;

/// The computed value of `ShapeRadius`.
pub type ShapeRadius = generic::GenericShapeRadius<NonNegativeLengthPercentage>;

/// The computed value of `shape()`.
pub type Shape = generic::Shape<Angle, LengthPercentage>;

/// The computed value of `ShapeCommand`.
pub type ShapeCommand = generic::GenericShapeCommand<Angle, LengthPercentage>;

/// The computed value of `PathOrShapeFunction`.
pub type PathOrShapeFunction = generic::GenericPathOrShapeFunction<Angle, LengthPercentage>;

/// The computed value of `CoordinatePair`.
pub type CoordinatePair = generic::CoordinatePair<LengthPercentage>;

/// Animate from `Shape` to `Path`, and vice versa.
macro_rules! animate_shape {
    (
        $from:ident,
        $to:ident,
        $procedure:ident,
        $from_as_shape:tt,
        $to_as_shape:tt
    ) => {{
        // Check fill-rule.
        if $from.fill != $to.fill {
            return Err(());
        }

        // Check the list of commands. (This is a specialized lists::by_computed_value::animate().)
        let from_cmds = $from.commands();
        let to_cmds = $to.commands();
        if from_cmds.len() != to_cmds.len() {
            return Err(());
        }
        let commands = from_cmds
            .iter()
            .zip(to_cmds.iter())
            .map(|(from_cmd, to_cmd)| {
                $from_as_shape(from_cmd).animate(&$to_as_shape(to_cmd), $procedure)
            })
            .collect::<Result<Vec<ShapeCommand>, ()>>()?;

        Ok(Shape {
            fill: $from.fill,
            commands: commands.into(),
        })
    }};
}

impl Animate for PathOrShapeFunction {
    #[inline]
    fn animate(&self, other: &Self, procedure: Procedure) -> Result<Self, ()> {
        // Per spec, commands are "the same" if they use the same command keyword, and use the same
        // <by-to> keyword. For curve and smooth, they also must have the same number of control
        // points. Therefore, we don't have to do normalization here. (Note that we do
        // normalization if we animate from path() to path(). See svg_path.rs for more details.)
        //
        // https://drafts.csswg.org/css-shapes-2/#interpolating-shape
        match (self, other) {
            (Self::Path(ref from), Self::Path(ref to)) => {
                from.animate(to, procedure).map(Self::Path)
            },
            (Self::Shape(ref from), Self::Shape(ref to)) => {
                from.animate(to, procedure).map(Self::Shape)
            },
            (Self::Shape(ref from), Self::Path(ref to)) => {
                // Animate from shape() to path(). We convert each PathCommand into ShapeCommand,
                // and return shape().
                animate_shape!(
                    from,
                    to,
                    procedure,
                    (|shape_cmd| shape_cmd),
                    (|path_cmd| ShapeCommand::from(path_cmd))
                )
                .map(Self::Shape)
            },
            (Self::Path(ref from), Self::Shape(ref to)) => {
                // Animate from path() to shape(). We convert each PathCommand into ShapeCommand,
                // and return shape().
                animate_shape!(
                    from,
                    to,
                    procedure,
                    (|path_cmd| ShapeCommand::from(path_cmd)),
                    (|shape_cmd| shape_cmd)
                )
                .map(Self::Shape)
            },
        }
    }
}

impl From<&PathCommand> for ShapeCommand {
    #[inline]
    fn from(path: &PathCommand) -> Self {
        use crate::values::computed::CSSPixelLength;
        match path {
            &PathCommand::Close => Self::Close,
            &PathCommand::Move { by_to, ref point } => Self::Move {
                by_to,
                point: point.into(),
            },
            &PathCommand::Line { by_to, ref point } => Self::Move {
                by_to,
                point: point.into(),
            },
            &PathCommand::HLine { by_to, x } => Self::HLine {
                by_to,
                x: LengthPercentage::new_length(CSSPixelLength::new(x)),
            },
            &PathCommand::VLine { by_to, y } => Self::VLine {
                by_to,
                y: LengthPercentage::new_length(CSSPixelLength::new(y)),
            },
            &PathCommand::CubicCurve {
                by_to,
                ref point,
                ref control1,
                ref control2,
            } => Self::CubicCurve {
                by_to,
                point: point.into(),
                control1: control1.into(),
                control2: control2.into(),
            },
            &PathCommand::QuadCurve {
                by_to,
                ref point,
                ref control1,
            } => Self::QuadCurve {
                by_to,
                point: point.into(),
                control1: control1.into(),
            },
            &PathCommand::SmoothCubic {
                by_to,
                ref point,
                ref control2,
            } => Self::SmoothCubic {
                by_to,
                point: point.into(),
                control2: control2.into(),
            },
            &PathCommand::SmoothQuad { by_to, ref point } => Self::SmoothQuad {
                by_to,
                point: point.into(),
            },
            &PathCommand::Arc {
                by_to,
                ref point,
                ref radii,
                arc_sweep,
                arc_size,
                rotate,
            } => Self::Arc {
                by_to,
                point: point.into(),
                radii: radii.into(),
                arc_sweep,
                arc_size,
                rotate: Angle::from_degrees(rotate),
            },
        }
    }
}

impl From<&CoordPair> for CoordinatePair {
    #[inline]
    fn from(p: &CoordPair) -> Self {
        use crate::values::computed::CSSPixelLength;
        Self::new(
            LengthPercentage::new_length(CSSPixelLength::new(p.x)),
            LengthPercentage::new_length(CSSPixelLength::new(p.y)),
        )
    }
}
