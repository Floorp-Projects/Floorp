//! Functions and filters for the sampling of pixels.

// See http://cs.brown.edu/courses/cs123/lectures/08_Image_Processing_IV.pdf
// for some of the theory behind image scaling and convolution

use std::f32;

use num_traits::{NumCast, ToPrimitive, Zero};

use buffer::{ImageBuffer, Pixel};
use image::GenericImageView;
use math::utils::clamp;
use traits::{Enlargeable, Primitive};

/// Available Sampling Filters
#[derive(Clone, Copy, Debug)]
pub enum FilterType {
    /// Nearest Neighbor
    Nearest,

    /// Linear Filter
    Triangle,

    /// Cubic Filter
    CatmullRom,

    /// Gaussian Filter
    Gaussian,

    /// Lanczos with window 3
    Lanczos3,
}

/// A Representation of a separable filter.
pub struct Filter<'a> {
    /// The filter's filter function.
    pub kernel: Box<dyn Fn(f32) -> f32 + 'a>,

    /// The window on which this filter operates.
    pub support: f32,
}

// sinc function: the ideal sampling filter.
fn sinc(t: f32) -> f32 {
    let a = t * f32::consts::PI;

    if t == 0.0 {
        1.0
    } else {
        a.sin() / a
    }
}

// lanczos kernel function. A windowed sinc function.
fn lanczos(x: f32, t: f32) -> f32 {
    if x.abs() < t {
        sinc(x) * sinc(x / t)
    } else {
        0.0
    }
}

// Calculate a splice based on the b and c parameters.
// from authors Mitchell and Netravali.
fn bc_cubic_spline(x: f32, b: f32, c: f32) -> f32 {
    let a = x.abs();

    let k = if a < 1.0 {
        (12.0 - 9.0 * b - 6.0 * c) * a.powi(3) + (-18.0 + 12.0 * b + 6.0 * c) * a.powi(2)
            + (6.0 - 2.0 * b)
    } else if a < 2.0 {
        (-b - 6.0 * c) * a.powi(3) + (6.0 * b + 30.0 * c) * a.powi(2) + (-12.0 * b - 48.0 * c) * a
            + (8.0 * b + 24.0 * c)
    } else {
        0.0
    };

    k / 6.0
}

/// The Gaussian Function.
/// ```r``` is the standard deviation.
pub fn gaussian(x: f32, r: f32) -> f32 {
    ((2.0 * f32::consts::PI).sqrt() * r).recip() * (-x.powi(2) / (2.0 * r.powi(2))).exp()
}

/// Calculate the lanczos kernel with a window of 3
pub fn lanczos3_kernel(x: f32) -> f32 {
    lanczos(x, 3.0)
}

/// Calculate the gaussian function with a
/// standard deviation of 0.5
pub fn gaussian_kernel(x: f32) -> f32 {
    gaussian(x, 0.5)
}

/// Calculate the Catmull-Rom cubic spline.
/// Also known as a form of `BiCubic` sampling in two dimensions.
pub fn catmullrom_kernel(x: f32) -> f32 {
    bc_cubic_spline(x, 0.0, 0.5)
}

/// Calculate the triangle function.
/// Also known as `BiLinear` sampling in two dimensions.
pub fn triangle_kernel(x: f32) -> f32 {
    if x.abs() < 1.0 {
        1.0 - x.abs()
    } else {
        0.0
    }
}

/// Calculate the box kernel.
/// Only pixels inside the box should be considered, and those
/// contribute equally.  So this method simply returns 1.
pub fn box_kernel(_x: f32) -> f32 {
    1.0
}

// Sample the rows of the supplied image using the provided filter.
// The height of the image remains unchanged.
// ```new_width``` is the desired width of the new image
// ```filter``` is the filter to use for sampling.
fn horizontal_sample<I, P, S>(
    image: &I,
    new_width: u32,
    filter: &mut Filter,
) -> ImageBuffer<P, Vec<S>>
where
    I: GenericImageView<Pixel = P>,
    P: Pixel<Subpixel = S> + 'static,
    S: Primitive + 'static,
{
    let (width, height) = image.dimensions();
    let mut out = ImageBuffer::new(new_width, height);
    let mut ws = Vec::new();

    let max: f32 = NumCast::from(S::max_value()).unwrap();
    let ratio = width as f32 / new_width as f32;
    let sratio = if ratio < 1.0 { 1.0 } else { ratio };
    let src_support = filter.support * sratio;

    for outx in 0..new_width {
        // Find the point in the input image corresponding to the centre
        // of the current pixel in the output image.
        let inputx = (outx as f32 + 0.5) * ratio;

        // Left and right are slice bounds for the input pixels relevant
        // to the output pixel we are calculating.  Pixel x is relevant
        // if and only if (x >= left) && (x < right).

        // Invariant: 0 <= left < right <= width

        let left = (inputx - src_support).floor() as i64;
        let left = clamp(left, 0, <i64 as From<_>>::from(width) - 1) as u32;

        let right = (inputx + src_support).ceil() as i64;
        let right = clamp(
            right,
            <i64 as From<_>>::from(left) + 1,
            <i64 as From<_>>::from(width),
        ) as u32;

        // Go back to left boundary of pixel, to properly compare with i
        // below, as the kernel treats the centre of a pixel as 0.
        let inputx = inputx - 0.5;

        ws.clear();
        let mut sum = 0.0;
        for i in left..right {
            let w = (filter.kernel)((i as f32 - inputx) / sratio);
            ws.push(w);
            sum += w;
        }

        for y in 0..height {
            let mut t = (0.0, 0.0, 0.0, 0.0);

            for (i, w) in ws.iter().enumerate() {
                let p = image.get_pixel(left + i as u32, y);

                let (k1, k2, k3, k4) = p.channels4();
                let vec: (f32, f32, f32, f32) = (
                    NumCast::from(k1).unwrap(),
                    NumCast::from(k2).unwrap(),
                    NumCast::from(k3).unwrap(),
                    NumCast::from(k4).unwrap(),
                );

                t.0 += vec.0 * w;
                t.1 += vec.1 * w;
                t.2 += vec.2 * w;
                t.3 += vec.3 * w;
            }

            let (t1, t2, t3, t4) = (t.0 / sum, t.1 / sum, t.2 / sum, t.3 / sum);
            let t = Pixel::from_channels(
                NumCast::from(clamp(t1, 0.0, max)).unwrap(),
                NumCast::from(clamp(t2, 0.0, max)).unwrap(),
                NumCast::from(clamp(t3, 0.0, max)).unwrap(),
                NumCast::from(clamp(t4, 0.0, max)).unwrap(),
            );

            out.put_pixel(outx, y, t);
        }
    }

    out
}

// Sample the columns of the supplied image using the provided filter.
// The width of the image remains unchanged.
// ```new_height``` is the desired height of the new image
// ```filter``` is the filter to use for sampling.
fn vertical_sample<I, P, S>(
    image: &I,
    new_height: u32,
    filter: &mut Filter,
) -> ImageBuffer<P, Vec<S>>
where
    I: GenericImageView<Pixel = P>,
    P: Pixel<Subpixel = S> + 'static,
    S: Primitive + 'static,
{
    let (width, height) = image.dimensions();
    let mut out = ImageBuffer::new(width, new_height);
    let mut ws = Vec::new();

    let max: f32 = NumCast::from(S::max_value()).unwrap();
    let ratio = height as f32 / new_height as f32;
    let sratio = if ratio < 1.0 { 1.0 } else { ratio };
    let src_support = filter.support * sratio;

    for outy in 0..new_height {
        // For an explanation of this algorithm, see the comments
        // in horizontal_sample.
        let inputy = (outy as f32 + 0.5) * ratio;

        let left = (inputy - src_support).floor() as i64;
        let left = clamp(left, 0, <i64 as From<_>>::from(height) - 1) as u32;

        let right = (inputy + src_support).ceil() as i64;
        let right = clamp(
            right,
            <i64 as From<_>>::from(left) + 1,
            <i64 as From<_>>::from(height),
        ) as u32;

        let inputy = inputy - 0.5;

        ws.clear();
        let mut sum = 0.0;
        for i in left..right {
            let w = (filter.kernel)((i as f32 - inputy) / sratio);
            ws.push(w);
            sum += w;
        }

        for x in 0..width {
            let mut t = (0.0, 0.0, 0.0, 0.0);

            for (i, w) in ws.iter().enumerate() {
                let p = image.get_pixel(x, left + i as u32);

                let (k1, k2, k3, k4) = p.channels4();
                let vec: (f32, f32, f32, f32) = (
                    NumCast::from(k1).unwrap(),
                    NumCast::from(k2).unwrap(),
                    NumCast::from(k3).unwrap(),
                    NumCast::from(k4).unwrap(),
                );

                t.0 += vec.0 * w;
                t.1 += vec.1 * w;
                t.2 += vec.2 * w;
                t.3 += vec.3 * w;
            }

            let (t1, t2, t3, t4) = (t.0 / sum, t.1 / sum, t.2 / sum, t.3 / sum);
            let t = Pixel::from_channels(
                NumCast::from(clamp(t1, 0.0, max)).unwrap(),
                NumCast::from(clamp(t2, 0.0, max)).unwrap(),
                NumCast::from(clamp(t3, 0.0, max)).unwrap(),
                NumCast::from(clamp(t4, 0.0, max)).unwrap(),
            );

            out.put_pixel(x, outy, t);
        }
    }

    out
}

/// Local struct for keeping track of pixel sums for fast thumbnail averaging
struct ThumbnailSum<S: Primitive + Enlargeable>(S::Larger, S::Larger, S::Larger, S::Larger);

impl<S: Primitive + Enlargeable> ThumbnailSum<S> {
    fn zeroed() -> Self {
        ThumbnailSum(S::Larger::zero(), S::Larger::zero(), S::Larger::zero(), S::Larger::zero())
    }

    fn sample_val(val: S) -> S::Larger {
        <S::Larger as NumCast>::from(val).unwrap()
    }

    fn add_pixel<P: Pixel<Subpixel=S>>(&mut self, pixel: P) {
        let pixel = pixel.channels4();
        self.0 += Self::sample_val(pixel.0);
        self.1 += Self::sample_val(pixel.1);
        self.2 += Self::sample_val(pixel.2);
        self.3 += Self::sample_val(pixel.3);
    }
}

/// Resize the supplied image to the specific dimensions.
///
/// For downscaling, this method uses a fast integer algorithm where each source pixel contributes
/// to exactly one target pixel.  May give aliasing artifacts if new size is close to old size.
///
/// In case the current width is smaller than the new width or similar for the height, another
/// strategy is used instead.  For each pixel in the output, a rectangular region of the input is
/// determined, just as previously.  But when no input pixel is part of this region, the nearest
/// pixels are interpolated instead.
///
/// For speed reasons, all interpolation is performed linearly over the colour values.  It will not
/// take the pixel colour spaces into account.
pub fn thumbnail<I, P, S>(image: &I, new_width: u32, new_height: u32) -> ImageBuffer<P, Vec<S>>
where
    I: GenericImageView<Pixel = P>,
    P: Pixel<Subpixel = S> + 'static,
    S: Primitive + Enlargeable + 'static,
{
    let (width, height) = image.dimensions();
    let mut out = ImageBuffer::new(new_width, new_height);

    let x_ratio = width as f32 / new_width as f32;
    let y_ratio = height as f32 / new_height as f32;

    for outy in 0..new_height {
        let bottomf = outy as f32 * y_ratio;
        let topf = bottomf + y_ratio;

        let bottom = clamp(
            bottomf.ceil() as u32,
            0,
            height - 1,
        );
        let top = clamp(
            topf.ceil() as u32,
            bottom,
            height,
        );

        for outx in 0..new_width {
            let leftf = outx as f32 * x_ratio;
            let rightf = leftf + x_ratio;

            let left = clamp(
                leftf.ceil() as u32,
                0,
                width - 1,
            );
            let right = clamp(
                rightf.ceil() as u32,
                left,
                width,
            );

            let avg = if bottom != top && left != right {
                thumbnail_sample_block(image, left, right, bottom, top)
            } else if bottom != top {  // && left == right
                // In the first column we have left == 0 and right > ceil(y_scale) > 0 so this
                // assertion can never trigger.
                debug_assert!(left > 0 && right > 0,
                    "First output column must have corresponding pixels");

                let fraction_horizontal = (leftf.fract() + rightf.fract())/2.;
                thumbnail_sample_fraction_horizontal(image, right - 1, fraction_horizontal, bottom, top)
            } else if left != right {  // && bottom == top
                // In the first line we have bottom == 0 and top > ceil(x_scale) > 0 so this
                // assertion can never trigger.
                debug_assert!(bottom > 0 && top > 0,
                    "First output row must have corresponding pixels");

                let fraction_vertical = (topf.fract() + bottomf.fract())/2.;
                thumbnail_sample_fraction_vertical(image, left, right, top - 1, fraction_vertical)
            } else {  // bottom == top && left == right
                let fraction_horizontal = (topf.fract() + bottomf.fract())/2.;
                let fraction_vertical= (leftf.fract() + rightf.fract())/2.;

                thumbnail_sample_fraction_both(image, right - 1, fraction_horizontal, top - 1, fraction_vertical)
            };

            let pixel = Pixel::from_channels(avg.0, avg.1, avg.2, avg.3);
            out.put_pixel(outx, outy, pixel);
        }
    }

    out
}

/// Get a pixel for a thumbnail where the input window encloses at least a full pixel.
fn thumbnail_sample_block<I, P, S>(
    image: &I,
    left: u32,
    right: u32, 
    bottom: u32,
    top: u32,
) -> (S, S, S, S)
where
    I: GenericImageView<Pixel = P>,
    P: Pixel<Subpixel = S>,
    S: Primitive + Enlargeable,
{
    let mut sum = ThumbnailSum::zeroed();

    for y in bottom..top {
        for x in left..right {
            let k = image.get_pixel(x, y);
            sum.add_pixel(k);
        }
    }

    let n = <S::Larger as NumCast>::from(
        (right - left) * (top - bottom)).unwrap();
    let round = <S::Larger as NumCast>::from(
        n / NumCast::from(2).unwrap()).unwrap();
    (
        S::clamp_from((sum.0 + round)/n),
        S::clamp_from((sum.1 + round)/n),
        S::clamp_from((sum.2 + round)/n),
        S::clamp_from((sum.3 + round)/n),
    )
}

/// Get a thumbnail pixel where the input window encloses at least a vertical pixel.
fn thumbnail_sample_fraction_horizontal<I, P, S>(
    image: &I,
    left: u32,
    fraction_horizontal: f32, 
    bottom: u32,
    top: u32,
) -> (S, S, S, S)
where
    I: GenericImageView<Pixel = P>,
    P: Pixel<Subpixel = S>,
    S: Primitive + Enlargeable,
{
    let fract = fraction_horizontal;

    let mut sum_left = ThumbnailSum::zeroed();
    let mut sum_right = ThumbnailSum::zeroed();
    for x in bottom..top {
        let k_left = image.get_pixel(left, x);
        sum_left.add_pixel(k_left);

        let k_right = image.get_pixel(left + 1, x);
        sum_right.add_pixel(k_right);
    }

    // Now we approximate: left/n*(1-fract) + right/n*fract
    let fact_right =       fract /((top - bottom) as f32);
    let fact_left  = (1. - fract)/((top - bottom) as f32);
    
    let mix_left_and_right = |leftv: S::Larger, rightv: S::Larger| 
        <S as NumCast>::from(
            fact_left * leftv.to_f32().unwrap() +
            fact_right * rightv.to_f32().unwrap()
        ).expect("Average sample value should fit into sample type");

    (
        mix_left_and_right(sum_left.0, sum_right.0),
        mix_left_and_right(sum_left.1, sum_right.1),
        mix_left_and_right(sum_left.2, sum_right.2),
        mix_left_and_right(sum_left.3, sum_right.3),
    )
}

/// Get a thumbnail pixel where the input window encloses at least a horizontal pixel.
fn thumbnail_sample_fraction_vertical<I, P, S>(
    image: &I,
    left: u32,
    right: u32, 
    bottom: u32,
    fraction_vertical: f32,
) -> (S, S, S, S)
where
    I: GenericImageView<Pixel = P>,
    P: Pixel<Subpixel = S>,
    S: Primitive + Enlargeable,
{
    let fract = fraction_vertical;

    let mut sum_bot = ThumbnailSum::zeroed();
    let mut sum_top = ThumbnailSum::zeroed();
    for x in left..right {
        let k_bot = image.get_pixel(x, bottom);
        sum_bot.add_pixel(k_bot);

        let k_top = image.get_pixel(x, bottom + 1);
        sum_top.add_pixel(k_top);
    }

    // Now we approximate: bot/n*fract + top/n*(1-fract)
    let fact_top =       fract /((right - left) as f32);
    let fact_bot = (1. - fract)/((right - left) as f32);
    
    let mix_bot_and_top = |botv: S::Larger, topv: S::Larger| 
        <S as NumCast>::from(
            fact_bot * botv.to_f32().unwrap() +
            fact_top * topv.to_f32().unwrap()
        ).expect("Average sample value should fit into sample type");

    (
        mix_bot_and_top(sum_bot.0, sum_top.0),
        mix_bot_and_top(sum_bot.1, sum_top.1),
        mix_bot_and_top(sum_bot.2, sum_top.2),
        mix_bot_and_top(sum_bot.3, sum_top.3),
    )
}

/// Get a single pixel for a thumbnail where the input window does not enclose any full pixel.
fn thumbnail_sample_fraction_both<I, P, S>(
    image: &I,
    left: u32,
    fraction_vertical: f32,
    bottom: u32,
    fraction_horizontal: f32,
) -> (S, S, S, S)
where
    I: GenericImageView<Pixel = P>,
    P: Pixel<Subpixel = S>,
    S: Primitive + Enlargeable,
{
    let k_bl = image.get_pixel(left,     bottom    ).channels4();
    let k_tl = image.get_pixel(left,     bottom + 1).channels4();
    let k_br = image.get_pixel(left + 1, bottom    ).channels4();
    let k_tr = image.get_pixel(left + 1, bottom + 1).channels4();
    
    let frac_v = fraction_vertical;
    let frac_h = fraction_horizontal;

    let fact_tr = frac_v        * frac_h;
    let fact_tl = frac_v        * (1. - frac_h);
    let fact_br = (1. - frac_v) * frac_h;
    let fact_bl = (1. - frac_v) * (1. - frac_h);

    let mix = |br: S, tr: S, bl: S, tl: S|
        <S as NumCast>::from(
            fact_br * br.to_f32().unwrap() +
            fact_tr * tr.to_f32().unwrap() +
            fact_bl * bl.to_f32().unwrap() +
            fact_tl * tl.to_f32().unwrap()
        ).expect("Average sample value should fit into sample type");
    
    (
        mix(k_br.0, k_tr.0, k_bl.0, k_tl.0),
        mix(k_br.1, k_tr.1, k_bl.1, k_tl.1),
        mix(k_br.2, k_tr.2, k_bl.2, k_tl.2),
        mix(k_br.3, k_tr.3, k_bl.3, k_tl.3),
    )
}

/// Perform a 3x3 box filter on the supplied image.
/// ```kernel``` is an array of the filter weights of length 9.
pub fn filter3x3<I, P, S>(image: &I, kernel: &[f32]) -> ImageBuffer<P, Vec<S>>
where
    I: GenericImageView<Pixel = P>,
    P: Pixel<Subpixel = S> + 'static,
    S: Primitive + 'static,
{
    // The kernel's input positions relative to the current pixel.
    let taps: &[(isize, isize)] = &[
        (-1, -1),
        (0, -1),
        (1, -1),
        (-1, 0),
        (0, 0),
        (1, 0),
        (-1, 1),
        (0, 1),
        (1, 1),
    ];

    let (width, height) = image.dimensions();

    let mut out = ImageBuffer::new(width, height);

    let max = S::max_value();
    let max: f32 = NumCast::from(max).unwrap();

    let sum = match kernel.iter().fold(0.0, |s, &item| s + item) {
        x if x == 0.0 => 1.0,
        sum => sum,
    };
    let sum = (sum, sum, sum, sum);

    for y in 1..height - 1 {
        for x in 1..width - 1 {
            let mut t = (0.0, 0.0, 0.0, 0.0);

            // TODO: There is no need to recalculate the kernel for each pixel.
            // Only a subtract and addition is needed for pixels after the first
            // in each row.
            for (&k, &(a, b)) in kernel.iter().zip(taps.iter()) {
                let k = (k, k, k, k);
                let x0 = x as isize + a;
                let y0 = y as isize + b;

                let p = image.get_pixel(x0 as u32, y0 as u32);

                let (k1, k2, k3, k4) = p.channels4();

                let vec: (f32, f32, f32, f32) = (
                    NumCast::from(k1).unwrap(),
                    NumCast::from(k2).unwrap(),
                    NumCast::from(k3).unwrap(),
                    NumCast::from(k4).unwrap(),
                );

                t.0 += vec.0 * k.0;
                t.1 += vec.1 * k.1;
                t.2 += vec.2 * k.2;
                t.3 += vec.3 * k.3;
            }

            let (t1, t2, t3, t4) = (t.0 / sum.0, t.1 / sum.1, t.2 / sum.2, t.3 / sum.3);

            let t = Pixel::from_channels(
                NumCast::from(clamp(t1, 0.0, max)).unwrap(),
                NumCast::from(clamp(t2, 0.0, max)).unwrap(),
                NumCast::from(clamp(t3, 0.0, max)).unwrap(),
                NumCast::from(clamp(t4, 0.0, max)).unwrap(),
            );

            out.put_pixel(x, y, t);
        }
    }

    out
}

/// Resize the supplied image to the specified dimensions.
/// ```nwidth``` and ```nheight``` are the new dimensions.
/// ```filter``` is the sampling filter to use.
pub fn resize<I: GenericImageView>(
    image: &I,
    nwidth: u32,
    nheight: u32,
    filter: FilterType,
) -> ImageBuffer<I::Pixel, Vec<<I::Pixel as Pixel>::Subpixel>>
where
    I::Pixel: 'static,
    <I::Pixel as Pixel>::Subpixel: 'static,
{
    let mut method = match filter {
        FilterType::Nearest => Filter {
            kernel: Box::new(box_kernel),
            support: 0.0,
        },
        FilterType::Triangle => Filter {
            kernel: Box::new(triangle_kernel),
            support: 1.0,
        },
        FilterType::CatmullRom => Filter {
            kernel: Box::new(catmullrom_kernel),
            support: 2.0,
        },
        FilterType::Gaussian => Filter {
            kernel: Box::new(gaussian_kernel),
            support: 3.0,
        },
        FilterType::Lanczos3 => Filter {
            kernel: Box::new(lanczos3_kernel),
            support: 3.0,
        },
    };

    let tmp = vertical_sample(image, nheight, &mut method);
    horizontal_sample(&tmp, nwidth, &mut method)
}

/// Performs a Gaussian blur on the supplied image.
/// ```sigma``` is a measure of how much to blur by.
pub fn blur<I: GenericImageView>(
    image: &I,
    sigma: f32,
) -> ImageBuffer<I::Pixel, Vec<<I::Pixel as Pixel>::Subpixel>>
where
    I::Pixel: 'static,
{
    let sigma = if sigma < 0.0 { 1.0 } else { sigma };

    let mut method = Filter {
        kernel: Box::new(|x| gaussian(x, sigma)),
        support: 2.0 * sigma,
    };

    let (width, height) = image.dimensions();

    // Keep width and height the same for horizontal and
    // vertical sampling.
    let tmp = vertical_sample(image, height, &mut method);
    horizontal_sample(&tmp, width, &mut method)
}

/// Performs an unsharpen mask on the supplied image.
/// ```sigma``` is the amount to blur the image by.
/// ```threshold``` is the threshold for the difference between
///
/// See <https://en.wikipedia.org/wiki/Unsharp_masking#Digital_unsharp_masking>
pub fn unsharpen<I, P, S>(image: &I, sigma: f32, threshold: i32) -> ImageBuffer<P, Vec<S>>
where
    I: GenericImageView<Pixel = P>,
    P: Pixel<Subpixel = S> + 'static,
    S: Primitive + 'static,
{
    let mut tmp = blur(image, sigma);

    let max = S::max_value();
    let max: i32 = NumCast::from(max).unwrap();
    let (width, height) = image.dimensions();

    for y in 0..height {
        for x in 0..width {
            let a = image.get_pixel(x, y);
            let b = tmp.get_pixel_mut(x, y);

            let p = a.map2(b, |c, d| {
                let ic: i32 = NumCast::from(c).unwrap();
                let id: i32 = NumCast::from(d).unwrap();

                let diff = (ic - id).abs();

                if diff > threshold {
                    let e = clamp(ic + diff, 0, max);

                    NumCast::from(e).unwrap()
                } else {
                    c
                }
            });

            *b = p;
        }
    }

    tmp
}

#[cfg(test)]
mod tests {
    use super::{resize, FilterType};
    use buffer::{ImageBuffer, RgbImage};
    #[cfg(feature = "benchmarks")]
    use test;

    #[bench]
    #[cfg(all(feature = "benchmarks", feature = "png_codec"))]
    fn bench_resize(b: &mut test::Bencher) {
        use std::path::Path;
        let img = ::open(&Path::new("./examples/fractal.png")).unwrap();
        b.iter(|| {
            test::black_box(resize(&img, 200, 200, ::Nearest));
        });
        b.bytes = 800 * 800 * 3 + 200 * 200 * 3;
    }

    #[test]
    fn test_issue_186() {
        let img: RgbImage = ImageBuffer::new(100, 100);
        let _ = resize(&img, 50, 50, FilterType::Lanczos3);
    }

    #[bench]
    #[cfg(all(feature = "benchmarks", feature = "tiff"))]
    fn bench_thumbnail(b: &mut test::Bencher) {
        let path = concat!(env!("CARGO_MANIFEST_DIR"), "/tests/images/tiff/testsuite/lenna.tiff");
        let image = ::open(path).unwrap();
        b.iter(|| {
            test::black_box(image.thumbnail(256, 256));
        });
        b.bytes = 512 * 512 * 4 + 256 * 256 * 4;
    }

    #[bench]
    #[cfg(all(feature = "benchmarks", feature = "tiff"))]
    fn bench_thumbnail_upsize(b: &mut test::Bencher) {
        let path = concat!(env!("CARGO_MANIFEST_DIR"), "/tests/images/tiff/testsuite/lenna.tiff");
        let image = ::open(path).unwrap().thumbnail(256, 256);
        b.iter(|| {
            test::black_box(image.thumbnail(512, 512));
        });
        b.bytes = 512 * 512 * 4 + 256 * 256 * 4;
    }

    #[bench]
    #[cfg(all(feature = "benchmarks", feature = "tiff"))]
    fn bench_thumbnail_upsize_irregular(b: &mut test::Bencher) {
        let path = concat!(env!("CARGO_MANIFEST_DIR"), "/tests/images/tiff/testsuite/lenna.tiff");
        let image = ::open(path).unwrap().thumbnail(193, 193);
        b.iter(|| {
            test::black_box(image.thumbnail(256, 256));
        });
        b.bytes = 193 * 193 * 4 + 256 * 256 * 4;
    }
}
