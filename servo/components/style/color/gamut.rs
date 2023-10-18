/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Gamut mapping.
//! <https://drafts.csswg.org/css-color-4/#gamut-mapping>

use super::{AbsoluteColor, ColorSpace};

impl AbsoluteColor {
    /// Map the components of this color into it's gamut limits if needed.
    /// <https://drafts.csswg.org/css-color-4/#binsearch>
    pub fn map_into_gamut_limits(&self) -> Self {
        // 1. if destination has no gamut limits (XYZ-D65, XYZ-D50, Lab, LCH,
        //    Oklab, Oklch) return origin.
        if matches!(
            self.color_space,
            ColorSpace::Lab |
                ColorSpace::Lch |
                ColorSpace::Oklab |
                ColorSpace::Oklch |
                ColorSpace::XyzD50 |
                ColorSpace::XyzD65
        ) {
            return self.clone();
        }

        // If the color does have gamut limits, then we can check it here and
        // possibly skip the binary search.
        if self.in_gamut() {
            return self.clone();
        }

        // 2. let origin_Oklch be origin converted from origin color space to
        //    the Oklch color space.
        let origin_oklch = self.to_color_space(ColorSpace::Oklch);

        // 3. if the Lightness of origin_Oklch is greater than or equal to
        //    100%, return { 1 1 1 origin.alpha } in destination.
        if origin_oklch.components.1 >= 1.0 {
            return AbsoluteColor::new(self.color_space, 1.0, 1.0, 1.0, self.alpha);
        }

        // 4. if the Lightness of origin_Oklch is less than than or equal to
        //    0%, return { 0 0 0 origin.alpha } in destination.
        if origin_oklch.components.1 <= 0.0 {
            return AbsoluteColor::new(self.color_space, 0.0, 0.0, 0.0, self.alpha);
        }

        // 5. let inGamut(color) be a function which returns true if, when
        //    passed a color, that color is inside the gamut of destination.
        //    For HSL and HWB, it returns true if the color is inside the gamut
        //    of sRGB.
        //    See [`Color::in_gamut`].

        // 6. if inGamut(origin_Oklch) is true, convert origin_Oklch to
        //    destination and return it as the gamut mapped color.
        // We already checked if the color is in gamut limits above.

        // 7. otherwise, let delta(one, two) be a function which returns the
        //    deltaEOK of color one compared to color two.
        // See the [`delta_eok`] function.

        // 8. let JND be 0.02
        const JND: f32 = 0.02;

        // 9. let epsilon be 0.0001
        const EPSILON: f32 = 0.0001;

        // 10. let clip(color) be a function which converts color to
        //     destination, converts all negative components to zero, converts
        //     all components greater that one to one, and returns the result.
        // See [`Color::clip`].

        // 11. set min to zero
        let mut min = 0.0;

        // 12. set max to the Oklch chroma of origin_Oklch.
        let mut max = origin_oklch.components.1;

        // 13. let min_inGamut be a boolean that represents when min is still
        //     in gamut, and set it to true
        let mut min_in_gamut = true;

        let mut current = origin_oklch.clone();
        let mut current_in_space = self.clone();

        // If we are already within the JND threshold, return the clipped color
        // and skip the binary search.
        let clipped = current_in_space.clip();
        if delta_eok(&clipped, &current) < JND {
            return clipped;
        }

        // 14. while (max - min is greater than epsilon) repeat the following
        //     steps.
        while max - min > EPSILON {
            // 14.1. set chroma to (min + max) / 2
            let chroma = (min + max) / 2.0;

            // 14.2. set current to origin_Oklch and then set the chroma
            //       component to chroma
            current.components.1 = chroma;

            current_in_space = current.to_color_space(self.color_space);

            // 14.3. if min_inGamut is true and also if inGamut(current) is
            //       true, set min to chroma and continue to repeat these steps.
            if min_in_gamut && current_in_space.in_gamut() {
                min = chroma;
                continue;
            }

            // 14.4. otherwise, if inGamut(current) is false carry out these
            //       steps:

            // 14.4.1. set clipped to clip(current)
            let clipped = current_in_space.clip();

            // 14.4.2. set E to delta(clipped, current)
            let e = delta_eok(&clipped, &current);

            // 14.4.3. if E < JND
            if e < JND {
                // 14.4.3.1. if (JND - E < epsilon) return clipped as the gamut
                //           mapped color
                if JND - e < EPSILON {
                    return clipped;
                }

                // 14.4.3.2. otherwise

                // 14.4.3.2.1. set min_inGamut to false
                min_in_gamut = false;

                // 14.4.3.2.2. set min to chroma
                min = chroma;
            } else {
                // 14.4.4. otherwise, set max to chroma and continue to repeat
                //         these steps
                max = chroma;
            }
        }

        // 15. return current as the gamut mapped color current
        current_in_space
    }

    /// Clamp this color to within the [0..1] range.
    fn clip(&self) -> Self {
        let mut result = self.clone();
        result.components = result.components.map(|c| c.clamp(0.0, 1.0));
        result
    }

    /// Returns true if this color is within its gamut limits.
    fn in_gamut(&self) -> bool {
        macro_rules! in_range {
            ($c:expr) => {{
                $c >= 0.0 && $c <= 1.0
            }};
        }

        match self.color_space {
            ColorSpace::Hsl | ColorSpace::Hwb => self.to_color_space(ColorSpace::Srgb).in_gamut(),
            ColorSpace::Srgb |
            ColorSpace::SrgbLinear |
            ColorSpace::DisplayP3 |
            ColorSpace::A98Rgb |
            ColorSpace::ProphotoRgb |
            ColorSpace::Rec2020 => {
                in_range!(self.components.0) &&
                    in_range!(self.components.1) &&
                    in_range!(self.components.2)
            },
            ColorSpace::Lab |
            ColorSpace::Lch |
            ColorSpace::Oklab |
            ColorSpace::Oklch |
            ColorSpace::XyzD50 |
            ColorSpace::XyzD65 => true,
        }
    }
}

/// Calculate deltaE OK (simple root sum of squares).
/// <https://drafts.csswg.org/css-color-4/#color-difference-OK>
fn delta_eok(reference: &AbsoluteColor, sample: &AbsoluteColor) -> f32 {
    // Delta is calculated in the oklab color space.
    let reference = reference.to_color_space(ColorSpace::Oklab);
    let sample = sample.to_color_space(ColorSpace::Oklab);

    let diff = reference.components - sample.components;
    let diff = diff * diff;
    (diff.0 + diff.1 + diff.2).sqrt()
}
