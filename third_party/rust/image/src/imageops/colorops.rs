//! Functions for altering and converting the color of pixelbufs

use buffer::{ImageBuffer, Pixel};
use color::{Luma, Rgba};
use image::{GenericImage, GenericImageView};
use math::nq;
use math::utils::clamp;
use num_traits::{Num, NumCast};
use std::f64::consts::PI;
use traits::Primitive;

type Subpixel<I> = <<I as GenericImageView>::Pixel as Pixel>::Subpixel;

/// Convert the supplied image to grayscale
pub fn grayscale<I: GenericImageView>(
    image: &I,
) -> ImageBuffer<Luma<Subpixel<I>>, Vec<Subpixel<I>>>
where
    Subpixel<I>: 'static,
    <Subpixel<I> as Num>::FromStrRadixErr: 'static,
{
    let (width, height) = image.dimensions();
    let mut out = ImageBuffer::new(width, height);

    for y in 0..height {
        for x in 0..width {
            let p = image.get_pixel(x, y).to_luma();
            out.put_pixel(x, y, p);
        }
    }

    out
}

/// Invert each pixel within the supplied image.
/// This function operates in place.
pub fn invert<I: GenericImage>(image: &mut I) {
    let (width, height) = image.dimensions();

    for y in 0..height {
        for x in 0..width {
            let mut p = image.get_pixel(x, y);
            p.invert();

            image.put_pixel(x, y, p);
        }
    }
}

/// Adjust the contrast of the supplied image.
/// ```contrast``` is the amount to adjust the contrast by.
/// Negative values decrease the contrast and positive values increase the contrast.
pub fn contrast<I, P, S>(image: &I, contrast: f32) -> ImageBuffer<P, Vec<S>>
where
    I: GenericImageView<Pixel = P>,
    P: Pixel<Subpixel = S> + 'static,
    S: Primitive + 'static,
{
    let (width, height) = image.dimensions();
    let mut out = ImageBuffer::new(width, height);

    let max = S::max_value();
    let max: f32 = NumCast::from(max).unwrap();

    let percent = ((100.0 + contrast) / 100.0).powi(2);

    for y in 0..height {
        for x in 0..width {
            let f = image.get_pixel(x, y).map(|b| {
                let c: f32 = NumCast::from(b).unwrap();

                let d = ((c / max - 0.5) * percent + 0.5) * max;
                let e = clamp(d, 0.0, max);

                NumCast::from(e).unwrap()
            });

            out.put_pixel(x, y, f);
        }
    }

    out
}

/// Brighten the supplied image.
/// ```value``` is the amount to brighten each pixel by.
/// Negative values decrease the brightness and positive values increase it.
pub fn brighten<I, P, S>(image: &I, value: i32) -> ImageBuffer<P, Vec<S>>
where
    I: GenericImageView<Pixel = P>,
    P: Pixel<Subpixel = S> + 'static,
    S: Primitive + 'static,
{
    let (width, height) = image.dimensions();
    let mut out = ImageBuffer::new(width, height);

    let max = S::max_value();
    let max: i32 = NumCast::from(max).unwrap();

    for y in 0..height {
        for x in 0..width {
            let e = image.get_pixel(x, y).map_with_alpha(
                |b| {
                    let c: i32 = NumCast::from(b).unwrap();
                    let d = clamp(c + value, 0, max);

                    NumCast::from(d).unwrap()
                },
                |alpha| alpha,
            );

            out.put_pixel(x, y, e);
        }
    }

    out
}

/// Hue rotate the supplied image.
/// `value` is the degrees to rotate each pixel by.
/// 0 and 360 do nothing, the rest rotates by the given degree value.
/// just like the css webkit filter hue-rotate(180)
pub fn huerotate<I, P, S>(image: &I, value: i32) -> ImageBuffer<P, Vec<S>>
where
    I: GenericImageView<Pixel = P>,
    P: Pixel<Subpixel = S> + 'static,
    S: Primitive + 'static,
{
    let (width, height) = image.dimensions();
    let mut out = ImageBuffer::new(width, height);

    let angle: f64 = NumCast::from(value).unwrap();

    let cosv = (angle * PI / 180.0).cos();
    let sinv = (angle * PI / 180.0).sin();
    let matrix: [f64; 9] = [
        // Reds
        0.213 + cosv * 0.787 - sinv * 0.213,
        0.715 - cosv * 0.715 - sinv * 0.715,
        0.072 - cosv * 0.072 + sinv * 0.928,
        // Greens
        0.213 - cosv * 0.213 + sinv * 0.143,
        0.715 + cosv * 0.285 + sinv * 0.140,
        0.072 - cosv * 0.072 - sinv * 0.283,
        // Blues
        0.213 - cosv * 0.213 - sinv * 0.787,
        0.715 - cosv * 0.715 + sinv * 0.715,
        0.072 + cosv * 0.928 + sinv * 0.072,
    ];
    for (x, y, pixel) in out.enumerate_pixels_mut() {
        let p = image.get_pixel(x, y);
        let (k1, k2, k3, k4) = p.channels4();
        let vec: (f64, f64, f64, f64) = (
            NumCast::from(k1).unwrap(),
            NumCast::from(k2).unwrap(),
            NumCast::from(k3).unwrap(),
            NumCast::from(k4).unwrap(),
        );

        let r = vec.0;
        let g = vec.1;
        let b = vec.2;

        let new_r = matrix[0] * r + matrix[1] * g + matrix[2] * b;
        let new_g = matrix[3] * r + matrix[4] * g + matrix[5] * b;
        let new_b = matrix[6] * r + matrix[7] * g + matrix[8] * b;
        let max = 255f64;
        let outpixel = Pixel::from_channels(
            NumCast::from(clamp(new_r, 0.0, max)).unwrap(),
            NumCast::from(clamp(new_g, 0.0, max)).unwrap(),
            NumCast::from(clamp(new_b, 0.0, max)).unwrap(),
            NumCast::from(clamp(vec.3, 0.0, max)).unwrap(),
        );
        *pixel = outpixel;
    }
    out
}

/// A color map
pub trait ColorMap {
    /// The color type on which the map operates on
    type Color;
    /// Returns the index of the closed match of `color`
    /// in the color map.
    fn index_of(&self, color: &Self::Color) -> usize;
    /// Maps `color` to the closest color in the color map.
    fn map_color(&self, color: &mut Self::Color);
}

/// A bi-level color map
#[derive(Clone, Copy)]
pub struct BiLevel;

impl ColorMap for BiLevel {
    type Color = Luma<u8>;

    #[inline(always)]
    fn index_of(&self, color: &Luma<u8>) -> usize {
        let luma = color.data;
        if luma[0] > 127 {
            1
        } else {
            0
        }
    }

    #[inline(always)]
    fn map_color(&self, color: &mut Luma<u8>) {
        let new_color = 0xFF * self.index_of(color) as u8;
        let luma = &mut color.data;
        luma[0] = new_color;
    }
}

impl ColorMap for nq::NeuQuant {
    type Color = Rgba<u8>;

    #[inline(always)]
    fn index_of(&self, color: &Rgba<u8>) -> usize {
        self.index_of(color.channels())
    }

    #[inline(always)]
    fn map_color(&self, color: &mut Rgba<u8>) {
        self.map_pixel(color.channels_mut())
    }
}

/// Floyd-Steinberg error diffusion
fn diffuse_err<P: Pixel<Subpixel = u8>>(pixel: &mut P, error: [i16; 3], factor: i16) {
    for (e, c) in error.iter().zip(pixel.channels_mut().iter_mut()) {
        *c = match <i16 as From<_>>::from(*c) + e * factor / 16 {
            val if val < 0 => 0,
            val if val > 0xFF => 0xFF,
            val => val as u8,
        }
    }
}

macro_rules! do_dithering(
    ($map:expr, $image:expr, $err:expr, $x:expr, $y:expr) => (
        {
            let old_pixel = $image[($x, $y)];
            let new_pixel = $image.get_pixel_mut($x, $y);
            $map.map_color(new_pixel);
            for ((e, &old), &new) in $err.iter_mut()
                                        .zip(old_pixel.channels().iter())
                                        .zip(new_pixel.channels().iter())
            {
                *e = <i16 as From<_>>::from(old) - <i16 as From<_>>::from(new)
            }
        }
    )
);

/// Reduces the colors of the image using the supplied `color_map` while applying
/// Floyd-Steinberg dithering to improve the visual conception
pub fn dither<Pix, Map>(image: &mut ImageBuffer<Pix, Vec<u8>>, color_map: &Map)
where
    Map: ColorMap<Color = Pix>,
    Pix: Pixel<Subpixel = u8> + 'static,
{
    let (width, height) = image.dimensions();
    let mut err: [i16; 3] = [0; 3];
    for y in 0..height - 1 {
        let x = 0;
        do_dithering!(color_map, image, err, x, y);
        diffuse_err(image.get_pixel_mut(x + 1, y), err, 7);
        diffuse_err(image.get_pixel_mut(x, y + 1), err, 5);
        diffuse_err(image.get_pixel_mut(x + 1, y + 1), err, 1);
        for x in 1..width - 1 {
            do_dithering!(color_map, image, err, x, y);
            diffuse_err(image.get_pixel_mut(x + 1, y), err, 7);
            diffuse_err(image.get_pixel_mut(x - 1, y + 1), err, 3);
            diffuse_err(image.get_pixel_mut(x, y + 1), err, 5);
            diffuse_err(image.get_pixel_mut(x + 1, y + 1), err, 1);
        }
        let x = width - 1;
        do_dithering!(color_map, image, err, x, y);
        diffuse_err(image.get_pixel_mut(x - 1, y + 1), err, 3);
        diffuse_err(image.get_pixel_mut(x, y + 1), err, 5);
    }
    let y = height - 1;
    let x = 0;
    do_dithering!(color_map, image, err, x, y);
    diffuse_err(image.get_pixel_mut(x + 1, y), err, 7);
    for x in 1..width - 1 {
        do_dithering!(color_map, image, err, x, y);
        diffuse_err(image.get_pixel_mut(x + 1, y), err, 7);
    }
    let x = width - 1;
    do_dithering!(color_map, image, err, x, y);
}

/// Reduces the colors using the supplied `color_map` and returns an image of the indices
pub fn index_colors<Pix, Map>(
    image: &ImageBuffer<Pix, Vec<u8>>,
    color_map: &Map,
) -> ImageBuffer<Luma<u8>, Vec<u8>>
where
    Map: ColorMap<Color = Pix>,
    Pix: Pixel<Subpixel = u8> + 'static,
{
    let mut indices = ImageBuffer::new(image.width(), image.height());
    for (pixel, idx) in image.pixels().zip(indices.pixels_mut()) {
        *idx = Luma([color_map.index_of(pixel) as u8])
    }
    indices
}

#[cfg(test)]
mod test {

    use super::*;
    use ImageBuffer;

    #[test]
    fn test_dither() {
        let mut image = ImageBuffer::from_raw(2, 2, vec![127, 127, 127, 127]).unwrap();
        let cmap = BiLevel;
        dither(&mut image, &cmap);
        assert_eq!(&*image, &[0, 0xFF, 0xFF, 0]);
        assert_eq!(index_colors(&image, &cmap).into_raw(), vec![0, 1, 1, 0])
    }
}
