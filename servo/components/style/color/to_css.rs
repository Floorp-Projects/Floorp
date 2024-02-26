/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Write colors into CSS strings.

use cssparser::color::PredefinedColorSpace;
use std::fmt::{self, Write};
use style_traits::{CssWriter, ToCss};

use super::{parsing, AbsoluteColor, ColorFlags, ColorSpace};

impl ToCss for AbsoluteColor {
    fn to_css<W>(&self, dest: &mut CssWriter<W>) -> fmt::Result
    where
        W: Write,
    {
        match self.color_space {
            ColorSpace::Srgb if self.flags.contains(ColorFlags::IS_LEGACY_SRGB) => {
                // The "none" keyword is not supported in the rgb/rgba legacy syntax.
                cssparser::ToCss::to_css(
                    &parsing::RgbaLegacy::from_floats(
                        self.components.0,
                        self.components.1,
                        self.components.2,
                        self.alpha,
                    ),
                    dest,
                )
            },
            ColorSpace::Hsl | ColorSpace::Hwb => self.into_srgb_legacy().to_css(dest),
            ColorSpace::Lab => cssparser::ToCss::to_css(
                &parsing::Lab::new(self.c0(), self.c1(), self.c2(), self.alpha()),
                dest,
            ),
            ColorSpace::Lch => cssparser::ToCss::to_css(
                &parsing::Lch::new(self.c0(), self.c1(), self.c2(), self.alpha()),
                dest,
            ),
            ColorSpace::Oklab => cssparser::ToCss::to_css(
                &parsing::Oklab::new(self.c0(), self.c1(), self.c2(), self.alpha()),
                dest,
            ),
            ColorSpace::Oklch => cssparser::ToCss::to_css(
                &parsing::Oklch::new(self.c0(), self.c1(), self.c2(), self.alpha()),
                dest,
            ),
            _ => {
                let color_space = match self.color_space {
                    ColorSpace::Srgb => {
                        debug_assert!(
                            !self.flags.contains(ColorFlags::IS_LEGACY_SRGB),
                            "legacy srgb is not a color function"
                        );
                        PredefinedColorSpace::Srgb
                    },
                    ColorSpace::SrgbLinear => PredefinedColorSpace::SrgbLinear,
                    ColorSpace::DisplayP3 => PredefinedColorSpace::DisplayP3,
                    ColorSpace::A98Rgb => PredefinedColorSpace::A98Rgb,
                    ColorSpace::ProphotoRgb => PredefinedColorSpace::ProphotoRgb,
                    ColorSpace::Rec2020 => PredefinedColorSpace::Rec2020,
                    ColorSpace::XyzD50 => PredefinedColorSpace::XyzD50,
                    ColorSpace::XyzD65 => PredefinedColorSpace::XyzD65,

                    _ => {
                        unreachable!("other color spaces do not support color() syntax")
                    },
                };

                let color_function = parsing::ColorFunction {
                    color_space,
                    c1: self.c0(),
                    c2: self.c1(),
                    c3: self.c2(),
                    alpha: self.alpha(),
                };
                let color = parsing::Color::ColorFunction(color_function);
                cssparser::ToCss::to_css(&color, dest)
            },
        }
    }
}
