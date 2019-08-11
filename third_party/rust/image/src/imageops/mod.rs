//! Image Processing Functions
use std::cmp;

use image::{GenericImage, GenericImageView, SubImage};

use buffer::Pixel;

pub use self::sample::FilterType;

pub use self::sample::FilterType::{CatmullRom, Gaussian, Lanczos3, Nearest, Triangle};

/// Affine transformations
pub use self::affine::{flip_horizontal, flip_vertical, rotate180, rotate270, rotate90};

/// Image sampling
pub use self::sample::{blur, filter3x3, resize, thumbnail, unsharpen};

/// Color operations
pub use self::colorops::{brighten, contrast, dither, grayscale, huerotate, index_colors, invert,
                         BiLevel, ColorMap};

mod affine;
// Public only because of Rust bug:
// https://github.com/rust-lang/rust/issues/18241
pub mod colorops;
mod sample;

/// Return a mutable view into an image
pub fn crop<I: GenericImageView>(
    image: &mut I,
    x: u32,
    y: u32,
    width: u32,
    height: u32,
) -> SubImage<&mut I> {
    let (iwidth, iheight) = image.dimensions();

    let x = cmp::min(x, iwidth);
    let y = cmp::min(y, iheight);

    let height = cmp::min(height, iheight - y);
    let width = cmp::min(width, iwidth - x);

    SubImage::new(image, x, y, width, height)
}

/// Calculate the region that can be copied from top to bottom.
///
/// Given image size of bottom and top image, and a point at which we want to place the top image
/// onto the bottom image, how large can we be? Have to wary of the following issues:
/// * Top might be larger than bottom
/// * Overflows in the computation
/// * Coordinates could be completely out of bounds
///
/// The main idea is to make use of inequalities provided by the nature of `saturing_add` and
/// `saturating_sub`. These intrinsically validate that all resulting coordinates will be in bounds
/// for both images.
///
/// We want that all these coordinate accesses are safe:
/// 1. `bottom.get_pixel(x + [0..x_range), y + [0..y_range))`
/// 2. `top.get_pixel([0..x_range), [0..y_range))`
///
/// Proof that the function provides the necessary bounds for width. Note that all unaugmented math
/// operations are to be read in standard arithmetic, not integer arithmetic. Since no direct
/// integer arithmetic occurs in the implementation, this is unambiguous.
///
/// ```text
/// Three short notes/lemmata:
/// - Iff `(a - b) <= 0` then `a.saturating_sub(b) = 0`
/// - Iff `(a - b) >= 0` then `a.saturating_sub(b) = a - b`
/// - If  `a <= c` then `a.saturating_sub(b) <= c.saturating_sub(b)`
///
/// 1.1 We show that if `bottom_width <= x`, then `x_range = 0` therefore `x + [0..x_range)` is empty.
///
/// x_range 
///  = (top_width.saturating_add(x).min(bottom_width)).saturating_sub(x) 
/// <= bottom_width.saturating_sub(x)
///
/// bottom_width <= x
/// <==> bottom_width - x <= 0
/// <==> bottom_width.saturating_sub(x) = 0
///  ==> x_range <= 0
///  ==> x_range  = 0
///
/// 1.2 If `x < bottom_width` then `x + x_range < bottom_width`
///
/// x + x_range 
/// <= x + bottom_width.saturating_sub(x) 
///  = x + (bottom_width - x) 
///  = bottom_width
///
/// 2. We show that `x_range <= top_width`
///
/// x_range 
///  = (top_width.saturating_add(x).min(bottom_width)).saturating_sub(x) 
/// <= top_width.saturating_add(x).saturating_sub(x)
/// <= (top_wdith + x).saturating_sub(x)
///  = top_width (due to `top_width >= 0` and `x >= 0`)
/// ```
///
/// Proof is the same for height.
pub fn overlay_bounds(
    (bottom_width, bottom_height): (u32, u32),
    (top_width, top_height): (u32, u32),
    x: u32,
    y: u32
)
    -> (u32, u32) 
{
    let x_range = top_width.saturating_add(x) // Calculate max coordinate
        .min(bottom_width) // Restrict to lower width
        .saturating_sub(x); // Determinate length from start `x`
    let y_range = top_height.saturating_add(y)
        .min(bottom_height)
        .saturating_sub(y);
    (x_range, y_range)
}

/// Overlay an image at a given coordinate (x, y)
pub fn overlay<I: GenericImage>(bottom: &mut I, top: &I, x: u32, y: u32) {
    let bottom_dims = bottom.dimensions();
    let top_dims = top.dimensions();

    // Crop our top image if we're going out of bounds
    let (range_width, range_height) = overlay_bounds(bottom_dims, top_dims, x, y);

    for top_y in 0..range_height {
        for top_x in 0..range_width {
            let p = top.get_pixel(top_x, top_y);
            let mut bottom_pixel = bottom.get_pixel(x + top_x, y + top_y);
            bottom_pixel.blend(&p);

            bottom.put_pixel(x + top_x, y + top_y, bottom_pixel);
        }
    }
}

/// Replace the contents of an image at a given coordinate (x, y)
pub fn replace<I: GenericImage>(bottom: &mut I, top: &I, x: u32, y: u32) {
    let bottom_dims = bottom.dimensions();
    let top_dims = top.dimensions();

    // Crop our top image if we're going out of bounds
    let (range_width, range_height) = overlay_bounds(bottom_dims, top_dims, x, y);

    for top_y in 0..range_height {
        for top_x in 0..range_width {
            let p = top.get_pixel(top_x, top_y);
            bottom.put_pixel(x + top_x, y + top_y, p);
        }
    }
}

#[cfg(test)]
mod tests {

    use super::overlay;
    use buffer::ImageBuffer;
    use color::Rgb;

    #[test]
    /// Test that images written into other images works
    fn test_image_in_image() {
        let mut target = ImageBuffer::new(32, 32);
        let source = ImageBuffer::from_pixel(16, 16, Rgb([255u8, 0, 0]));
        overlay(&mut target, &source, 0, 0);
        assert!(*target.get_pixel(0, 0) == Rgb([255u8, 0, 0]));
        assert!(*target.get_pixel(15, 0) == Rgb([255u8, 0, 0]));
        assert!(*target.get_pixel(16, 0) == Rgb([0u8, 0, 0]));
        assert!(*target.get_pixel(0, 15) == Rgb([255u8, 0, 0]));
        assert!(*target.get_pixel(0, 16) == Rgb([0u8, 0, 0]));
    }

    #[test]
    /// Test that images written outside of a frame doesn't blow up
    fn test_image_in_image_outside_of_bounds() {
        let mut target = ImageBuffer::new(32, 32);
        let source = ImageBuffer::from_pixel(32, 32, Rgb([255u8, 0, 0]));
        overlay(&mut target, &source, 1, 1);
        assert!(*target.get_pixel(0, 0) == Rgb([0, 0, 0]));
        assert!(*target.get_pixel(1, 1) == Rgb([255u8, 0, 0]));
        assert!(*target.get_pixel(31, 31) == Rgb([255u8, 0, 0]));
    }

    #[test]
    /// Test that images written to coordinates out of the frame doesn't blow up
    /// (issue came up in #848)
    fn test_image_outside_image_no_wrap_around() {
        let mut target = ImageBuffer::new(32, 32);
        let source = ImageBuffer::from_pixel(32, 32, Rgb([255u8, 0, 0]));
        overlay(&mut target, &source, 33, 33);
        assert!(*target.get_pixel(0, 0) == Rgb([0, 0, 0]));
        assert!(*target.get_pixel(1, 1) == Rgb([0, 0, 0]));
        assert!(*target.get_pixel(31, 31) == Rgb([0, 0, 0]));
    }

    #[test]
    /// Test that images written to coordinates with overflow works
    fn test_image_coordinate_overflow() {
        let mut target = ImageBuffer::new(16, 16);
        let source = ImageBuffer::from_pixel(32, 32, Rgb([255u8, 0, 0]));
        // Overflows to 'sane' coordinates but top is larger than bot.
        overlay(&mut target, &source, u32::max_value() - 31, u32::max_value() - 31);
        assert!(*target.get_pixel(0, 0) == Rgb([0, 0, 0]));
        assert!(*target.get_pixel(1, 1) == Rgb([0, 0, 0]));
        assert!(*target.get_pixel(15, 15) == Rgb([0, 0, 0]));
    }
}
