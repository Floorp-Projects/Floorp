use num_traits::{NumCast, Zero};
use std::mem;
use std::ops::{Index, IndexMut};

use buffer::Pixel;
use traits::Primitive;

/// An enumeration over supported color types and their bit depths
#[derive(Copy, PartialEq, Eq, Debug, Clone, Hash)]
pub enum ColorType {
    /// Pixel is grayscale
    Gray(u8),

    /// Pixel contains R, G and B channels
    RGB(u8),

    /// Pixel is an index into a color palette
    Palette(u8),

    /// Pixel is grayscale with an alpha channel
    GrayA(u8),

    /// Pixel is RGB with an alpha channel
    RGBA(u8),

    /// Pixel contains B, G and R channels
    BGR(u8),

    /// Pixel is BGR with an alpha channel
    BGRA(u8),

}

/// Returns the number of bits contained in a pixel of `ColorType` ```c```
pub fn bits_per_pixel(c: ColorType) -> usize {
    match c {
        ColorType::Gray(n) => n as usize,
        ColorType::GrayA(n) => 2 * n as usize,
        ColorType::RGB(n) | ColorType::Palette(n)| ColorType::BGR(n) => 3 * n as usize,
        ColorType::RGBA(n) | ColorType::BGRA(n) => 4 * n as usize,
    }
}

/// Returns the number of color channels that make up this pixel
pub fn num_components(c: ColorType) -> usize {
    match c {
        ColorType::Gray(_) => 1,
        ColorType::GrayA(_) => 2,
        ColorType::RGB(_) | ColorType::Palette(_) | ColorType::BGR(_)=> 3,
        ColorType::RGBA(_) | ColorType::BGRA(_) => 4,

    }
}

macro_rules! define_colors {
    {$(
        $ident:ident,
        $channels: expr,
        $alphas: expr,
        $interpretation: expr,
        $color_type: ident,
        #[$doc:meta];
    )*} => {

$( // START Structure definitions

#[$doc]
#[derive(PartialEq, Eq, Clone, Debug, Copy, Hash)]
#[repr(C)]
#[allow(missing_docs)]
pub struct $ident<T: Primitive> (pub [T; $channels]);

impl<T: Primitive + 'static> Pixel for $ident<T> {

    type Subpixel = T;

    const CHANNEL_COUNT: u8 = $channels;

    const COLOR_MODEL: &'static str = $interpretation;

    const COLOR_TYPE: ColorType = ColorType::$color_type(mem::size_of::<T>() as u8 * 8); 

    #[inline(always)]
    fn channels(&self) -> &[T] {
        &self.0
    }
    #[inline(always)]
    fn channels_mut(&mut self) -> &mut [T] {
        &mut self.0
    }

    #[allow(trivial_casts)]
    fn channels4(&self) -> (T, T, T, T) {
        let mut channels = [T::max_value(); 4];
        channels[0..$channels].copy_from_slice(&self.0);
        (channels[0], channels[1], channels[2], channels[3])
    }

    fn from_channels(a: T, b: T, c: T, d: T,) -> $ident<T> {
        *<$ident<T> as Pixel>::from_slice(&[a, b, c, d][..$channels])
    }

    fn from_slice(slice: &[T]) -> &$ident<T> {
        assert_eq!(slice.len(), $channels);
        unsafe { &*(slice.as_ptr() as *const $ident<T>) }
    }
    fn from_slice_mut(slice: &mut [T]) -> &mut $ident<T> {
        assert_eq!(slice.len(), $channels);
        unsafe { &mut *(slice.as_ptr() as *mut $ident<T>) }
    }

    fn to_rgb(&self) -> Rgb<T> {
        let mut pix = Rgb([Zero::zero(), Zero::zero(), Zero::zero()]);
        pix.from_color(self);
        pix
    }

    fn to_bgr(&self) -> Bgr<T> {
        let mut pix = Bgr([Zero::zero(), Zero::zero(), Zero::zero()]);
        pix.from_color(self);
        pix
    }

    fn to_rgba(&self) -> Rgba<T> {
        let mut pix = Rgba([Zero::zero(), Zero::zero(), Zero::zero(), Zero::zero()]);
        pix.from_color(self);
        pix
    }

    fn to_bgra(&self) -> Bgra<T> {
        let mut pix = Bgra([Zero::zero(), Zero::zero(), Zero::zero(), Zero::zero()]);
        pix.from_color(self);
        pix
    }

    fn to_luma(&self) -> Luma<T> {
        let mut pix = Luma([Zero::zero()]);
        pix.from_color(self);
        pix
    }

    fn to_luma_alpha(&self) -> LumaA<T> {
        let mut pix = LumaA([Zero::zero(), Zero::zero()]);
        pix.from_color(self);
        pix
    }

    fn map<F>(& self, f: F) -> $ident<T> where F: FnMut(T) -> T {
        let mut this = (*self).clone();
        this.apply(f);
        this
    }

    fn apply<F>(&mut self, mut f: F) where F: FnMut(T) -> T {
        for v in &mut self.0 {
            *v = f(*v)
        }
    }

    fn map_with_alpha<F, G>(&self, f: F, g: G) -> $ident<T> where F: FnMut(T) -> T, G: FnMut(T) -> T {
        let mut this = (*self).clone();
        this.apply_with_alpha(f, g);
        this
    }

    #[allow(trivial_casts)]
    fn apply_with_alpha<F, G>(&mut self, mut f: F, mut g: G) where F: FnMut(T) -> T, G: FnMut(T) -> T {
        for v in self.0[..$channels as usize-$alphas as usize].iter_mut() {
            *v = f(*v)
        }
        if $alphas as usize != 0 {
            let v = &mut self.0[$channels as usize-$alphas as usize];
            *v = g(*v)
        }
    }

    fn map2<F>(&self, other: &Self, f: F) -> $ident<T> where F: FnMut(T, T) -> T {
        let mut this = (*self).clone();
        this.apply2(other, f);
        this
    }

    fn apply2<F>(&mut self, other: &$ident<T>, mut f: F) where F: FnMut(T, T) -> T {
        for (a, &b) in self.0.iter_mut().zip(other.0.iter()) {
            *a = f(*a, b)
        }
    }

    fn invert(&mut self) {
        Invert::invert(self)
    }

    fn blend(&mut self, other: &$ident<T>) {
        Blend::blend(self, other)
    }
}

impl<T: Primitive> Index<usize> for $ident<T> {
    type Output = T;
    #[inline(always)]
    fn index(&self, _index: usize) -> &T {
        &self.0[_index]
    }
}

impl<T: Primitive> IndexMut<usize> for $ident<T> {
    #[inline(always)]
    fn index_mut(&mut self, _index: usize) -> &mut T {
        &mut self.0[_index]
    }
}

)* // END Structure definitions

    }
}

define_colors! {
    Rgb, 3, 0, "RGB", RGB, #[doc = "RGB colors"];
    Bgr, 3, 0, "BGR", BGR, #[doc = "BGR colors"];
    Luma, 1, 0, "Y", Gray, #[doc = "Grayscale colors"];
    Rgba, 4, 1, "RGBA", RGBA, #[doc = "RGB colors + alpha channel"];
    Bgra, 4, 1, "BGRA", BGRA, #[doc = "BGR colors + alpha channel"];
    LumaA, 2, 1, "YA", GrayA, #[doc = "Grayscale colors + alpha channel"];
}

/// Provides color conversions for the different pixel types.
pub trait FromColor<Other> {
    /// Changes `self` to represent `Other` in the color space of `Self`
    fn from_color(&mut self, &Other);
}

// Self->Self: just copy
impl<A: Copy> FromColor<A> for A {
    fn from_color(&mut self, other: &A) {
        *self = *other;
    }
}

/// `FromColor` for Luma

impl<T: Primitive + 'static> FromColor<Rgba<T>> for Luma<T> {
    fn from_color(&mut self, other: &Rgba<T>) {
        let gray = self.channels_mut();
        let rgb = other.channels();
        let l = 0.2126f32 * rgb[0].to_f32().unwrap() + 0.7152f32 * rgb[1].to_f32().unwrap()
            + 0.0722f32 * rgb[2].to_f32().unwrap();
        gray[0] = NumCast::from(l).unwrap()
    }
}

impl<T: Primitive + 'static> FromColor<Bgra<T>> for Luma<T> {
    fn from_color(&mut self, other: &Bgra<T>) {
        let gray = self.channels_mut();
        let bgra = other.channels();
        let l = 0.2126f32 * bgra[2].to_f32().unwrap() + 0.7152f32 * bgra[1].to_f32().unwrap()
            + 0.0722f32 * bgra[0].to_f32().unwrap();
        gray[0] = NumCast::from(l).unwrap()
    }
}

impl<T: Primitive + 'static> FromColor<Rgb<T>> for Luma<T> {
    fn from_color(&mut self, other: &Rgb<T>) {
        let gray = self.channels_mut();
        let rgb = other.channels();
        let l = 0.2126f32 * rgb[0].to_f32().unwrap() + 0.7152f32 * rgb[1].to_f32().unwrap()
            + 0.0722f32 * rgb[2].to_f32().unwrap();
        gray[0] = NumCast::from(l).unwrap()
    }
}


impl<T: Primitive + 'static> FromColor<Bgr<T>> for Luma<T> {
    fn from_color(&mut self, other: &Bgr<T>) {
        let gray = self.channels_mut();
        let bgr = other.channels();
        let l = 0.2126f32 * bgr[2].to_f32().unwrap() + 0.7152f32 * bgr[1].to_f32().unwrap()
            + 0.0722f32 * bgr[0].to_f32().unwrap();
        gray[0] = NumCast::from(l).unwrap()
    }
}


impl<T: Primitive + 'static> FromColor<LumaA<T>> for Luma<T> {
    fn from_color(&mut self, other: &LumaA<T>) {
        self.channels_mut()[0] = other.channels()[0]
    }
}

/// `FromColor` for LumA

impl<T: Primitive + 'static> FromColor<Rgba<T>> for LumaA<T> {
    fn from_color(&mut self, other: &Rgba<T>) {
        let gray_a = self.channels_mut();
        let rgba = other.channels();
        let l = 0.2126f32 * rgba[0].to_f32().unwrap() + 0.7152f32 * rgba[1].to_f32().unwrap()
            + 0.0722f32 * rgba[2].to_f32().unwrap();
        gray_a[0] = NumCast::from(l).unwrap();
        gray_a[1] = rgba[3];
    }
}

impl<T: Primitive + 'static> FromColor<Bgra<T>> for LumaA<T> {
    fn from_color(&mut self, other: &Bgra<T>) {
        let gray_a = self.channels_mut();
        let bgra = other.channels();
        let l = 0.2126f32 * bgra[2].to_f32().unwrap() + 0.7152f32 * bgra[1].to_f32().unwrap()
            + 0.0722f32 * bgra[0].to_f32().unwrap();
        gray_a[0] = NumCast::from(l).unwrap();
        gray_a[1] = bgra[3];
    }
}

impl<T: Primitive + 'static> FromColor<Rgb<T>> for LumaA<T> {
    fn from_color(&mut self, other: &Rgb<T>) {
        let gray_a = self.channels_mut();
        let rgb = other.channels();
        let l = 0.2126f32 * rgb[0].to_f32().unwrap() + 0.7152f32 * rgb[1].to_f32().unwrap()
            + 0.0722f32 * rgb[2].to_f32().unwrap();
        gray_a[0] = NumCast::from(l).unwrap();
        gray_a[1] = T::max_value();
    }
}

impl<T: Primitive + 'static> FromColor<Bgr<T>> for LumaA<T> {
    fn from_color(&mut self, other: &Bgr<T>) {
        let gray_a = self.channels_mut();
        let bgr = other.channels();
        let l = 0.2126f32 * bgr[2].to_f32().unwrap() + 0.7152f32 * bgr[1].to_f32().unwrap()
            + 0.0722f32 * bgr[0].to_f32().unwrap();
        gray_a[0] = NumCast::from(l).unwrap();
        gray_a[1] = T::max_value();
    }
}

impl<T: Primitive + 'static> FromColor<Luma<T>> for LumaA<T> {
    fn from_color(&mut self, other: &Luma<T>) {
        let gray_a = self.channels_mut();
        gray_a[0] = other.channels()[0];
        gray_a[1] = T::max_value();
    }
}

/// `FromColor` for RGBA

impl<T: Primitive + 'static> FromColor<Rgb<T>> for Rgba<T> {
    fn from_color(&mut self, other: &Rgb<T>) {
        let rgba = self.channels_mut();
        let rgb = other.channels();
        rgba[0] = rgb[0];
        rgba[1] = rgb[1];
        rgba[2] = rgb[2];
        rgba[3] = T::max_value();
    }
}

impl<T: Primitive + 'static> FromColor<Bgr<T>> for Rgba<T> {
    fn from_color(&mut self, other: &Bgr<T>) {
        let rgba = self.channels_mut();
        let bgr = other.channels();
        rgba[0] = bgr[2];
        rgba[1] = bgr[1];
        rgba[2] = bgr[0];
        rgba[3] = T::max_value();
    }
}

impl<T: Primitive + 'static> FromColor<Bgra<T>> for Rgba<T> {
    fn from_color(&mut self, other: &Bgra<T>) {
        let rgba = self.channels_mut();
        let bgra = other.channels();
        rgba[0] = bgra[2];
        rgba[1] = bgra[1];
        rgba[2] = bgra[0];
        rgba[3] = bgra[3];
    }
}


impl<T: Primitive + 'static> FromColor<LumaA<T>> for Rgba<T> {
    fn from_color(&mut self, other: &LumaA<T>) {
        let rgba = self.channels_mut();
        let gray = other.channels();
        rgba[0] = gray[0];
        rgba[1] = gray[0];
        rgba[2] = gray[0];
        rgba[3] = gray[1];
    }
}



impl<T: Primitive + 'static> FromColor<Luma<T>> for Rgba<T> {
    fn from_color(&mut self, gray: &Luma<T>) {
        let rgba = self.channels_mut();
        let gray = gray.channels()[0];
        rgba[0] = gray;
        rgba[1] = gray;
        rgba[2] = gray;
        rgba[3] = T::max_value();
    }
}


/// `FromColor` for BGRA

impl<T: Primitive + 'static> FromColor<Rgb<T>> for Bgra<T> {
    fn from_color(&mut self, other: &Rgb<T>) {
        let bgra = self.channels_mut();
        let rgb = other.channels();
        bgra[0] = rgb[2];
        bgra[1] = rgb[1];
        bgra[2] = rgb[0];
        bgra[3] = T::max_value();
    }
}


impl<T: Primitive + 'static> FromColor<Bgr<T>> for Bgra<T> {
    fn from_color(&mut self, other: &Bgr<T>) {
        let bgra = self.channels_mut();
        let bgr = other.channels();
        bgra[0] = bgr[0];
        bgra[1] = bgr[1];
        bgra[2] = bgr[2];
        bgra[3] = T::max_value();
    }
}


impl<T: Primitive + 'static> FromColor<Rgba<T>> for Bgra<T> {
    fn from_color(&mut self, other: &Rgba<T>) {
        let bgra = self.channels_mut();
        let rgba = other.channels();
        bgra[2] = rgba[0];
        bgra[1] = rgba[1];
        bgra[0] = rgba[2];
        bgra[3] = rgba[3];
    }
}

impl<T: Primitive + 'static> FromColor<LumaA<T>> for Bgra<T> {
    fn from_color(&mut self, other: &LumaA<T>) {
        let bgra = self.channels_mut();
        let gray = other.channels();
        bgra[0] = gray[0];
        bgra[1] = gray[0];
        bgra[2] = gray[0];
        bgra[3] = gray[1];
    }
}

impl<T: Primitive + 'static> FromColor<Luma<T>> for Bgra<T> {
    fn from_color(&mut self, gray: &Luma<T>) {
        let bgra = self.channels_mut();
        let gray = gray.channels()[0];
        bgra[0] = gray;
        bgra[1] = gray;
        bgra[2] = gray;
        bgra[3] = T::max_value();
    }
}



/// `FromColor` for RGB

impl<T: Primitive + 'static> FromColor<Rgba<T>> for Rgb<T> {
    fn from_color(&mut self, other: &Rgba<T>) {
        let rgb = self.channels_mut();
        let rgba = other.channels();
        rgb[0] = rgba[0];
        rgb[1] = rgba[1];
        rgb[2] = rgba[2];
    }
}


impl<T: Primitive + 'static> FromColor<Bgra<T>> for Rgb<T> {
    fn from_color(&mut self, other: &Bgra<T>) {
        let rgb = self.channels_mut();
        let bgra = other.channels();
        rgb[0] = bgra[2];
        rgb[1] = bgra[1];
        rgb[2] = bgra[0];
    }
}

impl<T: Primitive + 'static> FromColor<Bgr<T>> for Rgb<T> {
    fn from_color(&mut self, other: &Bgr<T>) {
        let rgb = self.channels_mut();
        let bgr = other.channels();
        rgb[0] = bgr[2];
        rgb[1] = bgr[1];
        rgb[2] = bgr[0];
    }
}

impl<T: Primitive + 'static> FromColor<LumaA<T>> for Rgb<T> {
    fn from_color(&mut self, other: &LumaA<T>) {
        let rgb = self.channels_mut();
        let gray = other.channels()[0];
        rgb[0] = gray;
        rgb[1] = gray;
        rgb[2] = gray;
    }
}

impl<T: Primitive + 'static> FromColor<Luma<T>> for Rgb<T> {
    fn from_color(&mut self, gray: &Luma<T>) {
        let rgb = self.channels_mut();
        let gray = gray.channels()[0];
        rgb[0] = gray;
        rgb[1] = gray;
        rgb[2] = gray;
    }
}

/// `FromColor` for BGR

impl<T: Primitive + 'static> FromColor<Rgba<T>> for Bgr<T> {
    fn from_color(&mut self, other: &Rgba<T>) {
        let bgr = self.channels_mut();
        let rgba = other.channels();
        bgr[0] = rgba[2];
        bgr[1] = rgba[1];
        bgr[2] = rgba[0];
    }
}

impl<T: Primitive + 'static> FromColor<Rgb<T>> for Bgr<T> {
    fn from_color(&mut self, other: &Rgb<T>) {
        let bgr = self.channels_mut();
        let rgb = other.channels();
        bgr[0] = rgb[2];
        bgr[1] = rgb[1];
        bgr[2] = rgb[0];
    }
}


impl<T: Primitive + 'static> FromColor<Bgra<T>> for Bgr<T> {
    fn from_color(&mut self, other: &Bgra<T>) {
        let bgr = self.channels_mut();
        let bgra = other.channels();
        bgr[0] = bgra[0];
        bgr[1] = bgra[1];
        bgr[2] = bgra[2];
    }
}

impl<T: Primitive + 'static> FromColor<LumaA<T>> for Bgr<T> {
    fn from_color(&mut self, other: &LumaA<T>) {
        let bgr = self.channels_mut();
        let gray = other.channels()[0];
        bgr[0] = gray;
        bgr[1] = gray;
        bgr[2] = gray;
    }
}

impl<T: Primitive + 'static> FromColor<Luma<T>> for Bgr<T> {
    fn from_color(&mut self, gray: &Luma<T>) {
        let bgr = self.channels_mut();
        let gray = gray.channels()[0];
        bgr[0] = gray;
        bgr[1] = gray;
        bgr[2] = gray;
    }
}


/// Blends a color inter another one
pub trait Blend {
    /// Blends a color in-place.
    fn blend(&mut self, other: &Self);
}

impl<T: Primitive> Blend for LumaA<T> {
    fn blend(&mut self, other: &LumaA<T>) {
        let max_t = T::max_value();
        let max_t = max_t.to_f32().unwrap();
        let (bg_luma, bg_a) = (self.0[0], self.0[1]);
        let (fg_luma, fg_a) = (other.0[0], other.0[1]);

        let (bg_luma, bg_a) = (
            bg_luma.to_f32().unwrap() / max_t,
            bg_a.to_f32().unwrap() / max_t,
        );
        let (fg_luma, fg_a) = (
            fg_luma.to_f32().unwrap() / max_t,
            fg_a.to_f32().unwrap() / max_t,
        );

        let alpha_final = bg_a + fg_a - bg_a * fg_a;
        if alpha_final == 0.0 {
            return;
        };
        let bg_luma_a = bg_luma * bg_a;
        let fg_luma_a = fg_luma * fg_a;

        let out_luma_a = fg_luma_a + bg_luma_a * (1.0 - fg_a);
        let out_luma = out_luma_a / alpha_final;

        *self = LumaA([
            NumCast::from(max_t * out_luma).unwrap(),
            NumCast::from(max_t * alpha_final).unwrap(),
        ])
    }
}

impl<T: Primitive> Blend for Luma<T> {
    fn blend(&mut self, other: &Luma<T>) {
        *self = *other
    }
}

impl<T: Primitive> Blend for Rgba<T> {
    fn blend(&mut self, other: &Rgba<T>) {
        // http://stackoverflow.com/questions/7438263/alpha-compositing-algorithm-blend-modes#answer-11163848

        // First, as we don't know what type our pixel is, we have to convert to floats between 0.0 and 1.0
        let max_t = T::max_value();
        let max_t = max_t.to_f32().unwrap();
        let (bg_r, bg_g, bg_b, bg_a) = (self.0[0], self.0[1], self.0[2], self.0[3]);
        let (fg_r, fg_g, fg_b, fg_a) = (other.0[0], other.0[1], other.0[2], other.0[3]);
        let (bg_r, bg_g, bg_b, bg_a) = (
            bg_r.to_f32().unwrap() / max_t,
            bg_g.to_f32().unwrap() / max_t,
            bg_b.to_f32().unwrap() / max_t,
            bg_a.to_f32().unwrap() / max_t,
        );
        let (fg_r, fg_g, fg_b, fg_a) = (
            fg_r.to_f32().unwrap() / max_t,
            fg_g.to_f32().unwrap() / max_t,
            fg_b.to_f32().unwrap() / max_t,
            fg_a.to_f32().unwrap() / max_t,
        );

        // Work out what the final alpha level will be
        let alpha_final = bg_a + fg_a - bg_a * fg_a;
        if alpha_final == 0.0 {
            return;
        };

        // We premultiply our channels by their alpha, as this makes it easier to calculate
        let (bg_r_a, bg_g_a, bg_b_a) = (bg_r * bg_a, bg_g * bg_a, bg_b * bg_a);
        let (fg_r_a, fg_g_a, fg_b_a) = (fg_r * fg_a, fg_g * fg_a, fg_b * fg_a);

        // Standard formula for src-over alpha compositing
        let (out_r_a, out_g_a, out_b_a) = (
            fg_r_a + bg_r_a * (1.0 - fg_a),
            fg_g_a + bg_g_a * (1.0 - fg_a),
            fg_b_a + bg_b_a * (1.0 - fg_a),
        );

        // Unmultiply the channels by our resultant alpha channel
        let (out_r, out_g, out_b) = (
            out_r_a / alpha_final,
            out_g_a / alpha_final,
            out_b_a / alpha_final,
        );

        // Cast back to our initial type on return
        *self = Rgba([
            NumCast::from(max_t * out_r).unwrap(),
            NumCast::from(max_t * out_g).unwrap(),
            NumCast::from(max_t * out_b).unwrap(),
            NumCast::from(max_t * alpha_final).unwrap(),
        ])
    }
}



impl<T: Primitive> Blend for Bgra<T> {
    fn blend(&mut self, other: &Bgra<T>) {
        // http://stackoverflow.com/questions/7438263/alpha-compositing-algorithm-blend-modes#answer-11163848

        // First, as we don't know what type our pixel is, we have to convert to floats between 0.0 and 1.0
        let max_t = T::max_value();
        let max_t = max_t.to_f32().unwrap();
        let (bg_r, bg_g, bg_b, bg_a) = (self.0[2], self.0[1], self.0[0], self.0[3]);
        let (fg_r, fg_g, fg_b, fg_a) = (other.0[2], other.0[1], other.0[0], other.0[3]);
        let (bg_r, bg_g, bg_b, bg_a) = (
            bg_r.to_f32().unwrap() / max_t,
            bg_g.to_f32().unwrap() / max_t,
            bg_b.to_f32().unwrap() / max_t,
            bg_a.to_f32().unwrap() / max_t,
        );
        let (fg_r, fg_g, fg_b, fg_a) = (
            fg_r.to_f32().unwrap() / max_t,
            fg_g.to_f32().unwrap() / max_t,
            fg_b.to_f32().unwrap() / max_t,
            fg_a.to_f32().unwrap() / max_t,
        );

        // Work out what the final alpha level will be
        let alpha_final = bg_a + fg_a - bg_a * fg_a;
        if alpha_final == 0.0 {
            return;
        };

        // We premultiply our channels by their alpha, as this makes it easier to calculate
        let (bg_r_a, bg_g_a, bg_b_a) = (bg_r * bg_a, bg_g * bg_a, bg_b * bg_a);
        let (fg_r_a, fg_g_a, fg_b_a) = (fg_r * fg_a, fg_g * fg_a, fg_b * fg_a);

        // Standard formula for src-over alpha compositing
        let (out_r_a, out_g_a, out_b_a) = (
            fg_r_a + bg_r_a * (1.0 - fg_a),
            fg_g_a + bg_g_a * (1.0 - fg_a),
            fg_b_a + bg_b_a * (1.0 - fg_a),
        );

        // Unmultiply the channels by our resultant alpha channel
        let (out_r, out_g, out_b) = (
            out_r_a / alpha_final,
            out_g_a / alpha_final,
            out_b_a / alpha_final,
        );

        // Cast back to our initial type on return
        *self = Bgra([
            NumCast::from(max_t * out_b).unwrap(),
            NumCast::from(max_t * out_g).unwrap(),
            NumCast::from(max_t * out_r).unwrap(),
            NumCast::from(max_t * alpha_final).unwrap(),
        ])
    }
}

impl<T: Primitive> Blend for Rgb<T> {
    fn blend(&mut self, other: &Rgb<T>) {
        *self = *other
    }
}

impl<T: Primitive> Blend for Bgr<T> {
    fn blend(&mut self, other: &Bgr<T>) {
        *self = *other
    }
}


/// Invert a color
pub trait Invert {
    /// Inverts a color in-place.
    fn invert(&mut self);
}

impl<T: Primitive> Invert for LumaA<T> {
    fn invert(&mut self) {
        let l = self.0;
        let max = T::max_value();

        *self = LumaA([max - l[0], l[1]])
    }
}

impl<T: Primitive> Invert for Luma<T> {
    fn invert(&mut self) {
        let l = self.0;

        let max = T::max_value();
        let l1 = max - l[0];

        *self = Luma([l1])
    }
}

impl<T: Primitive> Invert for Rgba<T> {
    fn invert(&mut self) {
        let rgba = self.0;

        let max = T::max_value();

        *self = Rgba([max - rgba[0], max - rgba[1], max - rgba[2], rgba[3]])
    }
}


impl<T: Primitive> Invert for Bgra<T> {
    fn invert(&mut self) {
        let bgra = self.0;

        let max = T::max_value();

        *self = Bgra([max - bgra[2], max - bgra[1], max - bgra[0], bgra[3]])
    }
}


impl<T: Primitive> Invert for Rgb<T> {
    fn invert(&mut self) {
        let rgb = self.0;

        let max = T::max_value();

        let r1 = max - rgb[0];
        let g1 = max - rgb[1];
        let b1 = max - rgb[2];

        *self = Rgb([r1, g1, b1])
    }
}

impl<T: Primitive> Invert for Bgr<T> {
    fn invert(&mut self) {
        let bgr = self.0;

        let max = T::max_value();

        let r1 = max - bgr[2];
        let g1 = max - bgr[1];
        let b1 = max - bgr[0];

        *self = Bgr([b1, g1, r1])
    }
}

#[cfg(test)]
mod tests {
    use super::{LumaA, Pixel, Rgb, Rgba, Bgr, Bgra};

    #[test]
    fn test_apply_with_alpha_rgba() {
        let mut rgba = Rgba([0, 0, 0, 0]);
        rgba.apply_with_alpha(|s| s, |_| 0xFF);
        assert_eq!(rgba, Rgba([0, 0, 0, 0xFF]));
    }

    #[test]
    fn test_apply_with_alpha_bgra() {
        let mut bgra = Bgra([0, 0, 0, 0]);
        bgra.apply_with_alpha(|s| s, |_| 0xFF);
        assert_eq!(bgra, Bgra([0, 0, 0, 0xFF]));
    }

    #[test]
    fn test_apply_with_alpha_rgb() {
        let mut rgb = Rgb([0, 0, 0]);
        rgb.apply_with_alpha(|s| s, |_| panic!("bug"));
        assert_eq!(rgb, Rgb([0, 0, 0]));
    }

    #[test]
    fn test_apply_with_alpha_bgr() {
        let mut bgr = Bgr([0, 0, 0]);
        bgr.apply_with_alpha(|s| s, |_| panic!("bug"));
        assert_eq!(bgr, Bgr([0, 0, 0]));
    }


    #[test]
    fn test_map_with_alpha_rgba() {
        let rgba = Rgba([0, 0, 0, 0]).map_with_alpha(|s| s, |_| 0xFF);
        assert_eq!(rgba, Rgba([0, 0, 0, 0xFF]));
    }

    #[test]
    fn test_map_with_alpha_rgb() {
        let rgb = Rgb([0, 0, 0]).map_with_alpha(|s| s, |_| panic!("bug"));
        assert_eq!(rgb, Rgb([0, 0, 0]));
    }

    #[test]
    fn test_map_with_alpha_bgr() {
        let bgr = Bgr([0, 0, 0]).map_with_alpha(|s| s, |_| panic!("bug"));
        assert_eq!(bgr, Bgr([0, 0, 0]));
    }


    #[test]
    fn test_map_with_alpha_bgra() {
        let bgra = Bgra([0, 0, 0, 0]).map_with_alpha(|s| s, |_| 0xFF);
        assert_eq!(bgra, Bgra([0, 0, 0, 0xFF]));
    }

    #[test]
    fn test_blend_luma_alpha() {
        let ref mut a = LumaA([255 as u8, 255]);
        let b = LumaA([255 as u8, 255]);
        a.blend(&b);
        assert_eq!(a.0[0], 255);
        assert_eq!(a.0[1], 255);

        let ref mut a = LumaA([255 as u8, 0]);
        let b = LumaA([255 as u8, 255]);
        a.blend(&b);
        assert_eq!(a.0[0], 255);
        assert_eq!(a.0[1], 255);

        let ref mut a = LumaA([255 as u8, 255]);
        let b = LumaA([255 as u8, 0]);
        a.blend(&b);
        assert_eq!(a.0[0], 255);
        assert_eq!(a.0[1], 255);

        let ref mut a = LumaA([255 as u8, 0]);
        let b = LumaA([255 as u8, 0]);
        a.blend(&b);
        assert_eq!(a.0[0], 255);
        assert_eq!(a.0[1], 0);
    }

    #[test]
    fn test_blend_rgba() {
        let ref mut a = Rgba([255 as u8, 255, 255, 255]);
        let b = Rgba([255 as u8, 255, 255, 255]);
        a.blend(&b);
        assert_eq!(a.0, [255, 255, 255, 255]);

        let ref mut a = Rgba([255 as u8, 255, 255, 0]);
        let b = Rgba([255 as u8, 255, 255, 255]);
        a.blend(&b);
        assert_eq!(a.0, [255, 255, 255, 255]);

        let ref mut a = Rgba([255 as u8, 255, 255, 255]);
        let b = Rgba([255 as u8, 255, 255, 0]);
        a.blend(&b);
        assert_eq!(a.0, [255, 255, 255, 255]);

        let ref mut a = Rgba([255 as u8, 255, 255, 0]);
        let b = Rgba([255 as u8, 255, 255, 0]);
        a.blend(&b);
        assert_eq!(a.0, [255, 255, 255, 0]);
    }    

    #[test]
    fn test_apply_without_alpha_rgba() {
        let mut rgba = Rgba([0, 0, 0, 0]);
        rgba.apply_without_alpha(|s| s + 1);
        assert_eq!(rgba, Rgba([1, 1, 1, 0]));
    }

    #[test]
    fn test_apply_without_alpha_bgra() {
        let mut bgra = Bgra([0, 0, 0, 0]);
        bgra.apply_without_alpha(|s| s + 1);
        assert_eq!(bgra, Bgra([1, 1, 1, 0]));
    }

    #[test]
    fn test_apply_without_alpha_rgb() {
        let mut rgb = Rgb([0, 0, 0]);
        rgb.apply_without_alpha(|s| s + 1);
        assert_eq!(rgb, Rgb([1, 1, 1]));
    }

    #[test]
    fn test_apply_without_alpha_bgr() {
        let mut bgr = Bgr([0, 0, 0]);
        bgr.apply_without_alpha(|s| s + 1);
        assert_eq!(bgr, Bgr([1, 1, 1]));
    }

    #[test]
    fn test_map_without_alpha_rgba() {
        let rgba = Rgba([0, 0, 0, 0]).map_without_alpha(|s| s + 1);
        assert_eq!(rgba, Rgba([1, 1, 1, 0]));
    }

    #[test]
    fn test_map_without_alpha_rgb() {
        let rgb = Rgb([0, 0, 0]).map_without_alpha(|s| s + 1);
        assert_eq!(rgb, Rgb([1, 1, 1]));
    }

    #[test]
    fn test_map_without_alpha_bgr() {
        let bgr = Bgr([0, 0, 0]).map_without_alpha(|s| s + 1);
        assert_eq!(bgr, Bgr([1, 1, 1]));
    }

    #[test]
    fn test_map_without_alpha_bgra() {
        let bgra = Bgra([0, 0, 0, 0]).map_without_alpha(|s| s + 1);
        assert_eq!(bgra, Bgra([1, 1, 1, 0]));
    }
}
