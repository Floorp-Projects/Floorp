/*
 * Copyright 2006 The Android Open Source Project
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

extern crate lazy_static;

pub trait ColorSpaceLuminance {
	fn to_luma(&self, gamma: f32, luminance: f32) -> f32;
	fn from_luma(&self, gamma: f32, luma: f32) -> f32;
}

pub struct SRGBColorSpaceLuminance
{
}

pub struct LinearColorSpaceLuminance
{
}

pub struct GammaColorSpaceLuminance
{}

impl ColorSpaceLuminance for LinearColorSpaceLuminance {
	fn to_luma(&self, gamma: f32, luminance: f32) -> f32 {
        assert!(gamma == 1.);
		luminance
	}
	fn from_luma(&self, gamma: f32, luma: f32) -> f32 {
        assert!(gamma == 1.);
		luma
	}
}

impl ColorSpaceLuminance for GammaColorSpaceLuminance {
	fn to_luma(&self, gamma: f32, luminance: f32) -> f32 {
		luminance.powf(gamma)
	}
	fn from_luma(&self, gamma: f32, luma: f32) -> f32 {
		luma.powf(1./gamma)
	}
}

impl ColorSpaceLuminance for SRGBColorSpaceLuminance {
	fn to_luma(&self, gamma: f32, luminance: f32) -> f32 {
        assert!(gamma == 0.);
        //The magic numbers are derived from the sRGB specification.
        //See http://www.color.org/chardata/rgb/srgb.xalter .
		if luminance <= 0.04045 {
			return luminance / 12.92;
		}
		return ((luminance + 0.055) / 1.055).powf(2.4);
	}
	fn from_luma(&self, gamma: f32, luma: f32) -> f32 {
        assert!(gamma == 0.);
        //The magic numbers are derived from the sRGB specification.
        //See http://www.color.org/chardata/rgb/srgb.xalter .
        if luma <= 0.0031308 {
            return luma * 12.92;
        }
        return 1.055 * luma.powf(1./2.4)
               - 0.055;
	}
}

fn round_to_u8(x : f32) -> u8 {
    assert!((x + 0.5).floor() < 256.0);
    (x + 0.5).floor() as u8
}

/*
 * Scales base <= 2^N-1 to 2^8-1
 * @param N [1, 8] the number of bits used by base.
 * @param base the number to be scaled to [0, 255].
 */
fn scale255(n: u8, mut base : u8) -> u8 {
    base <<= 8 - n;
    let mut lum = base;
    let mut i = n;

    while i < 8 {
        lum |= base >> i;
        i += n;
    }

    return lum;
}

#[derive(Copy, Clone)]
#[allow(dead_code)]
pub struct Color {
    r: u8,
    g: u8,
    b: u8,
    a: u8,
}

impl Color {
    pub fn new(r: u8, g: u8, b: u8, a: u8) -> Color {
        Color {
            r: r,
            g: g,
            b: b,
            a: a,
        }
    }
}

// This will invert the gamma applied by CoreGraphics,
// so we can get linear values.
// CoreGraphics obscurely defaults to 2.0 as the smoothing gamma value.
// The color space used does not appear to affect this choice.
fn get_inverse_gamma_table_coregraphics_smoothing() -> [u8; 256] {
	let mut table : [u8; 256] = [0; 256];

	for i in 0..256 {
		let x = i as f32 / 255.0;
		let value = round_to_u8(x * x * 255.0);
		table[i] = value;
	}

	table
}

// A value of 0.5 for SK_GAMMA_CONTRAST appears to be a good compromise.
// With lower values small text appears washed out (though correctly so).
// With higher values lcd fringing is worse and the smoothing effect of
// partial coverage is diminished.
fn apply_contrast(srca: f32, contrast: f32) -> f32 {
    srca + ((1.0 - srca) * contrast * srca)
}

// The approach here is not necessarily the one with the lowest error
// See https://bel.fi/alankila/lcd/alpcor.html for a similar kind of thing
// that just search for the adjusted alpha value
pub fn build_gamma_correcting_lut(table: &mut [u8; 256], src: u8, contrast: f32,
                                  src_space: &ColorSpaceLuminance, src_gamma: f32,
                                  dst_convert: &ColorSpaceLuminance, dst_gamma: f32) {

    let src = src as f32 / 255.0;
    let lin_src = src_space.to_luma(src_gamma, src);
    // Guess at the dst. The perceptual inverse provides smaller visual
    // discontinuities when slight changes to desaturated colors cause a channel
    // to map to a different correcting lut with neighboring srcI.
    // See https://code.google.com/p/chromium/issues/detail?id=141425#c59 .
    let dst = 1.0 - src;
    let lin_dst = dst_convert.to_luma(dst_gamma, dst);

    // Contrast value tapers off to 0 as the src luminance becomes white
    let adjusted_contrast = contrast * lin_dst;

    // Remove discontinuity and instability when src is close to dst.
    // The value 1/256 is arbitrary and appears to contain the instability.
    if (src - dst).abs() < (1.0 / 256.0) {
        let mut ii : f32 = 0.0;
        for i in 0..256 {
            let raw_srca = ii / 255.0;
            let srca = apply_contrast(raw_srca, adjusted_contrast);

            table[i] = round_to_u8(255.0 * srca);
            ii += 1.0;
        }
    } else {
        // Avoid slow int to float conversion.
        let mut ii : f32 = 0.0;
        for i in 0..256 {
            // 'raw_srca += 1.0f / 255.0f' and even
            // 'raw_srca = i * (1.0f / 255.0f)' can add up to more than 1.0f.
            // When this happens the table[255] == 0x0 instead of 0xff.
            // See http://code.google.com/p/chromium/issues/detail?id=146466
            let raw_srca = ii / 255.0;
            let srca = apply_contrast(raw_srca, adjusted_contrast);
            assert!(srca <= 1.0);
            let dsta = 1.0 - srca;

            // Calculate the output we want.
            let lin_out = lin_src * srca + dsta * lin_dst;
            assert!(lin_out <= 1.0);
            let out = dst_convert.from_luma(dst_gamma, lin_out);

            // Undo what the blit blend will do.
            // i.e. given the formula for OVER: out = src * result + (1 - result) * dst
            // solving for result gives:
            let result = (out - dst) / (src - dst);

            //println!("Setting {:?} to {:?}", i, round_to_u8(255.0 * result));
            table[i] = round_to_u8(255.0 * result);

            ii += 1.0;
        }
    }
}

fn fetch_color_space(gamma: f32) -> Box<ColorSpaceLuminance> {
    if 0.0 == gamma {
        return Box::new( SRGBColorSpaceLuminance{} );
    } else if 1.0 == gamma {
        return Box::new( LinearColorSpaceLuminance{} );
    } else {
        return Box::new( GammaColorSpaceLuminance{} );
    }
}

// Computes the luminance from the given r, g, and b in accordance with
// SK_LUM_COEFF_X. For correct results, r, g, and b should be in linear space.
fn compute_luminance(r: u8, g: u8, b: u8) -> u8 {
	// The following is
	// r * SK_LUM_COEFF_R + g * SK_LUM_COEFF_G + b * SK_LUM_COEFF_B
	// with SK_LUM_COEFF_X in 1.8 fixed point (rounding adjusted to sum to 256).
	let val : u32 = r as u32 * 54 + g as u32 * 183 + b as u32 * 19;
	return (val >> 8) as u8;
}

// Skia uses 3 bits per channel for luminance.
pub const LUM_BITS :u8 = 3;
#[allow(dead_code)]
pub struct GammaLut {
    tables: [[u8; 256 ]; 1 << LUM_BITS],
    cg_inverse_gamma: [u8; 256],
}

impl GammaLut {
    // Skia actually makes 9 gamma tables, then based on the luminance color,
    // fetches the RGB gamma table for that color.
    fn generate_tables(&mut self, contrast: f32, paint_gamma: f32, device_gamma: f32) {
        let paint_color_space = fetch_color_space(paint_gamma);
        let device_color_space = fetch_color_space(device_gamma);

        for i in 0..(1 << LUM_BITS) {
            let luminance = scale255(LUM_BITS, i);
            build_gamma_correcting_lut(&mut self.tables[i as usize],
                                       luminance,
                                       contrast,
                                       &*paint_color_space,
                                       paint_gamma,
                                       &*device_color_space,
                                       device_gamma);
        }
    }

    #[allow(dead_code)]
    fn table_count() -> usize {
        return 1 << LUM_BITS;
    }

    pub fn print_values(&self, table: usize) {
        for x in 0..256 {
            println!("[{:?}] = {:?}", x, self.tables[table][x])
        }
    }

    pub fn get_table(&self, color: u8) -> [u8; 256] {
        return self.tables[(color >> (8 - LUM_BITS)) as usize];
    }

    pub fn new(contrast: f32, paint_gamma: f32, device_gamma: f32) -> GammaLut {
        let mut table = GammaLut {
            tables: [[0; 256]; 1 << LUM_BITS],
            cg_inverse_gamma: get_inverse_gamma_table_coregraphics_smoothing(),
        };

        table.generate_tables(contrast, paint_gamma, device_gamma);

        table
    }

    // Skia normally preblends based on what the text color is.
    // If we can't do that, use Skia default colors.
    pub fn preblend_default_colors_bgra(&self, pixels: &mut Vec<u8>, width: usize, height: usize) {
        let preblend_color = Color {
            r: 0x7f,
            g: 0x80,
            b: 0x7f,
            a: 0xff,
        };
        self.preblend_bgra(pixels, width, height, preblend_color);
    }

    fn replace_pixels_bgra(&self, pixels: &mut Vec<u8>, width: usize, height: usize,
                           table_r: [u8; 256], table_g: [u8; 256], table_b: [u8; 256]) {
         for y in 0..height {
            let current_height = y * width * 4;

            for pixel in pixels[current_height..current_height + (width * 4)].chunks_mut(4) {
                pixel[0] = table_b[pixel[0] as usize];
                pixel[1] = table_g[pixel[1] as usize];
                pixel[2] = table_r[pixel[2] as usize];
                // Don't touch alpha
            }
        }
    }

    // Mostly used by windows and GlyphRunAnalysis::GetAlphaTexture
    fn replace_pixels_rgb(&self, pixels: &mut Vec<u8>, width: usize, height: usize,
                          table_r: [u8; 256], table_g: [u8; 256], table_b: [u8; 256]) {
         for y in 0..height {
            let current_height = y * width * 3;

            for pixel in pixels[current_height..current_height + (width * 3)].chunks_mut(3) {
                pixel[0] = table_r[pixel[0] as usize];
                pixel[1] = table_g[pixel[1] as usize];
                pixel[2] = table_b[pixel[2] as usize];
            }
        }
    }

    // Assumes pixels are in BGRA format. Assumes pixel values are in linear space already.
    pub fn preblend_bgra(&self, pixels: &mut Vec<u8>, width: usize, height: usize, color: Color) {
        let table_r = self.get_table(color.r);
        let table_g = self.get_table(color.g);
        let table_b = self.get_table(color.b);

        self.replace_pixels_bgra(pixels, width, height, table_r, table_g, table_b);
    }

    // Assumes pixels are in RGB format. Assumes pixel values are in linear space already. NOTE:
    // there is no alpha here.
    pub fn preblend_rgb(&self, pixels: &mut Vec<u8>, width: usize, height: usize, color: Color) {
        let table_r = self.get_table(color.r);
        let table_g = self.get_table(color.g);
        let table_b = self.get_table(color.b);

        self.replace_pixels_rgb(pixels, width, height, table_r, table_g, table_b);
    }

    #[cfg(target_os="macos")]
    pub fn coregraphics_convert_to_linear_bgra(&self, pixels: &mut Vec<u8>, width: usize, height: usize) {
        self.replace_pixels_bgra(pixels, width, height,
                                 self.cg_inverse_gamma,
                                 self.cg_inverse_gamma,
                                 self.cg_inverse_gamma);
    }

    // Assumes pixels are in BGRA format. Assumes pixel values are in linear space already.
    pub fn preblend_grayscale_bgra(&self, pixels: &mut Vec<u8>, width: usize, height: usize, color: Color) {
        let table_g = self.get_table(color.g);

         for y in 0..height {
            let current_height = y * width * 4;

            for pixel in pixels[current_height..current_height + (width * 4)].chunks_mut(4) {
                let luminance = compute_luminance(pixel[0], pixel[1], pixel[2]);
                pixel[0] = table_g[luminance as usize];
                pixel[1] = table_g[luminance as usize];
                pixel[2] = table_g[luminance as usize];
                // Don't touch alpha
            }
		}
    }

} // end impl GammaLut

#[cfg(test)]
mod tests {
    use std::cmp;
    use std::mem;

    fn over(dst: u32, src: u32, alpha: u32) -> u32 {
        (src * alpha + dst * (255 - alpha))/255
    }

    fn overf(dst: f32, src: f32, alpha: f32) -> f32 {
        ((src * alpha + dst * (255. - alpha))/255.) as f32
    }


    fn absdiff(a: u32, b: u32) -> u32 {
        if a < b  { b - a } else { a - b }
    }

    #[test]
    fn gamma() {
        let mut table: [u8; 256] = unsafe{ mem::uninitialized() } ;
        let space = ::GammaColorSpaceLuminance{};
        let g : f32 = 2.;
        let mut src : u32 = 131;
        while src < 256 {
            ::build_gamma_correcting_lut(&mut table, src as u8, 0., &space, g, &space, g);
            let mut max_diff = 0;
            let mut dst = 0;
            while dst < 256 {
                for alpha in 0u32..256 {
                    let preblend = table[alpha as usize];
                    let lin_dst = (dst as f32 / 255.).powf(g) * 255.;
                    let lin_src = (src as f32 / 255.).powf(g) * 255.;

                    let preblend_result = over(dst, src, preblend as u32);
                    let true_result = ((overf(lin_dst, lin_src, alpha as f32) / 255.).powf(1. / g) * 255.) as u32;
                    let diff = absdiff(preblend_result, true_result);
                    //println!("{} -- {} {} = {}", alpha, preblend_result, true_result, diff);
                    max_diff = cmp::max(max_diff, diff);
                }

                //println!("{} {} max {}", src, dst, max_diff);
                assert!(max_diff <= 33);
                dst += 1;

            }
            src += 1;
        }
    }
} // end mod
