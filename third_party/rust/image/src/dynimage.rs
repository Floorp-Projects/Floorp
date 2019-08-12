use num_iter;
use std::fs::File;
use std::io;
use std::io::{BufRead, BufReader, BufWriter, Seek, Write};
use std::path::Path;
use std::u32;

#[cfg(feature = "bmp")]
use bmp;
#[cfg(feature = "gif_codec")]
use gif;
#[cfg(feature = "hdr")]
use hdr;
#[cfg(feature = "ico")]
use ico;
#[cfg(feature = "jpeg")]
use jpeg;
#[cfg(feature = "png_codec")]
use png;
#[cfg(feature = "pnm")]
use pnm;
#[cfg(feature = "tga")]
use tga;
#[cfg(feature = "tiff")]
use tiff;
#[cfg(feature = "webp")]
use webp;

use buffer::{ConvertBuffer, GrayAlphaImage, GrayImage, ImageBuffer, Pixel, RgbImage, RgbaImage, BgrImage, BgraImage};
use flat::FlatSamples;
use color;
use image;
use image::{GenericImage, GenericImageView, ImageDecoder, ImageFormat, ImageOutputFormat,
            ImageResult};
use imageops;

/// A Dynamic Image
#[derive(Clone)]
pub enum DynamicImage {
    /// Each pixel in this image is 8-bit Luma
    ImageLuma8(GrayImage),

    /// Each pixel in this image is 8-bit Luma with alpha
    ImageLumaA8(GrayAlphaImage),

    /// Each pixel in this image is 8-bit Rgb
    ImageRgb8(RgbImage),

    /// Each pixel in this image is 8-bit Rgb with alpha
    ImageRgba8(RgbaImage),

    /// Each pixel in this image is 8-bit Bgr
    ImageBgr8(BgrImage),

    /// Each pixel in this image is 8-bit Bgr with alpha
    ImageBgra8(BgraImage),
}

macro_rules! dynamic_map(
        ($dynimage: expr, ref $image: ident => $action: expr) => (
                match $dynimage {
                        DynamicImage::ImageLuma8(ref $image) => DynamicImage::ImageLuma8($action),
                        DynamicImage::ImageLumaA8(ref $image) => DynamicImage::ImageLumaA8($action),
                        DynamicImage::ImageRgb8(ref $image) => DynamicImage::ImageRgb8($action),
                        DynamicImage::ImageRgba8(ref $image) => DynamicImage::ImageRgba8($action),
                        DynamicImage::ImageBgr8(ref $image) => DynamicImage::ImageBgr8($action),
                        DynamicImage::ImageBgra8(ref $image) => DynamicImage::ImageBgra8($action),
                }
        );

        ($dynimage: expr, ref mut $image: ident => $action: expr) => (
                match $dynimage {
                        DynamicImage::ImageLuma8(ref mut $image) => DynamicImage::ImageLuma8($action),
                        DynamicImage::ImageLumaA8(ref mut $image) => DynamicImage::ImageLumaA8($action),
                        DynamicImage::ImageRgb8(ref mut $image) => DynamicImage::ImageRgb8($action),
                        DynamicImage::ImageRgba8(ref mut $image) => DynamicImage::ImageRgba8($action),
                        DynamicImage::ImageBgr8(ref mut $image) => DynamicImage::ImageBgr8($action),
                        DynamicImage::ImageBgra8(ref mut $image) => DynamicImage::ImageBgra8($action),
                }
        );

        ($dynimage: expr, ref $image: ident -> $action: expr) => (
                match $dynimage {
                        DynamicImage::ImageLuma8(ref $image) => $action,
                        DynamicImage::ImageLumaA8(ref $image) => $action,
                        DynamicImage::ImageRgb8(ref $image) => $action,
                        DynamicImage::ImageRgba8(ref $image) => $action,
                        DynamicImage::ImageBgr8(ref $image) => $action,
                        DynamicImage::ImageBgra8(ref $image) => $action,
                }
        );

        ($dynimage: expr, ref mut $image: ident -> $action: expr) => (
                match $dynimage {
                        DynamicImage::ImageLuma8(ref mut $image) => $action,
                        DynamicImage::ImageLumaA8(ref mut $image) => $action,
                        DynamicImage::ImageRgb8(ref mut $image) => $action,
                        DynamicImage::ImageRgba8(ref mut $image) => $action,
                        DynamicImage::ImageBgr8(ref mut $image) => $action,
                        DynamicImage::ImageBgra8(ref mut $image) => $action,
                }
        );
);

impl DynamicImage {
    /// Creates a dynamic image backed by a buffer of grey pixels.
    pub fn new_luma8(w: u32, h: u32) -> DynamicImage {
        DynamicImage::ImageLuma8(ImageBuffer::new(w, h))
    }

    /// Creates a dynamic image backed by a buffer of grey
    /// pixels with transparency.
    pub fn new_luma_a8(w: u32, h: u32) -> DynamicImage {
        DynamicImage::ImageLumaA8(ImageBuffer::new(w, h))
    }

    /// Creates a dynamic image backed by a buffer of RGB pixels.
    pub fn new_rgb8(w: u32, h: u32) -> DynamicImage {
        DynamicImage::ImageRgb8(ImageBuffer::new(w, h))
    }

    /// Creates a dynamic image backed by a buffer of RGBA pixels.
    pub fn new_rgba8(w: u32, h: u32) -> DynamicImage {
        DynamicImage::ImageRgba8(ImageBuffer::new(w, h))
    }

    /// Creates a dynamic image backed by a buffer of BGRA pixels.
    pub fn new_bgra8(w: u32, h: u32) -> DynamicImage {
        DynamicImage::ImageBgra8(ImageBuffer::new(w, h))
    }

    /// Creates a dynamic image backed by a buffer of BGR pixels.
    pub fn new_bgr8(w: u32, h: u32) -> DynamicImage {
        DynamicImage::ImageBgr8(ImageBuffer::new(w, h))
    }


    /// Returns a copy of this image as an RGB image.
    pub fn to_rgb(&self) -> RgbImage {
        dynamic_map!(*self, ref p -> {
            p.convert()
        })
    }

    /// Returns a copy of this image as an RGBA image.
    pub fn to_rgba(&self) -> RgbaImage {
        dynamic_map!(*self, ref p -> {
            p.convert()
        })
    }


    /// Returns a copy of this image as an BGR image.
    pub fn to_bgr(&self) -> BgrImage {
        dynamic_map!(*self, ref p -> {
            p.convert()
        })
    }

    /// Returns a copy of this image as an BGRA image.
    pub fn to_bgra(&self) -> BgraImage {
        dynamic_map!(*self, ref p -> {
            p.convert()
        })
    }

    /// Returns a copy of this image as a Luma image.
    pub fn to_luma(&self) -> GrayImage {
        dynamic_map!(*self, ref p -> {
            p.convert()
        })
    }

    /// Returns a copy of this image as a LumaA image.
    pub fn to_luma_alpha(&self) -> GrayAlphaImage {
        dynamic_map!(*self, ref p -> {
            p.convert()
        })
    }

    /// Return a cut out of this image delimited by the bounding rectangle.
    pub fn crop(&mut self, x: u32, y: u32, width: u32, height: u32) -> DynamicImage {
        dynamic_map!(*self, ref mut p => imageops::crop(p, x, y, width, height).to_image())
    }

    /// Return a reference to an 8bit RGB image
    pub fn as_rgb8(&self) -> Option<&RgbImage> {
        match *self {
            DynamicImage::ImageRgb8(ref p) => Some(p),
            _ => None,
        }
    }

    /// Return a mutable reference to an 8bit RGB image
    pub fn as_mut_rgb8(&mut self) -> Option<&mut RgbImage> {
        match *self {
            DynamicImage::ImageRgb8(ref mut p) => Some(p),
            _ => None,
        }
    }


    /// Return a reference to an 8bit BGR image
    pub fn as_bgr8(&self) -> Option<&BgrImage> {
        match *self {
            DynamicImage::ImageBgr8(ref p) => Some(p),
            _ => None,
        }
    }

    /// Return a mutable reference to an 8bit BGR image
    pub fn as_mut_bgr8(&mut self) -> Option<&mut BgrImage> {
        match *self {
            DynamicImage::ImageBgr8(ref mut p) => Some(p),
            _ => None,
        }
    }

    /// Return a reference to an 8bit RGBA image
    pub fn as_rgba8(&self) -> Option<&RgbaImage> {
        match *self {
            DynamicImage::ImageRgba8(ref p) => Some(p),
            _ => None,
        }
    }

    /// Return a mutable reference to an 8bit RGBA image
    pub fn as_mut_rgba8(&mut self) -> Option<&mut RgbaImage> {
        match *self {
            DynamicImage::ImageRgba8(ref mut p) => Some(p),
            _ => None,
        }
    }

    /// Return a reference to an 8bit BGRA image
    pub fn as_bgra8(&self) -> Option<&BgraImage> {
        match *self {
            DynamicImage::ImageBgra8(ref p) => Some(p),
            _ => None,
        }
    }

    /// Return a mutable reference to an 8bit RGBA image
    pub fn as_mut_bgra8(&mut self) -> Option<&mut BgraImage> {
        match *self {
            DynamicImage::ImageBgra8(ref mut p) => Some(p),
            _ => None,
        }
    }

    /// Return a reference to an 8bit Grayscale image
    pub fn as_luma8(&self) -> Option<&GrayImage> {
        match *self {
            DynamicImage::ImageLuma8(ref p) => Some(p),
            _ => None,
        }
    }

    /// Return a mutable reference to an 8bit Grayscale image
    pub fn as_mut_luma8(&mut self) -> Option<&mut GrayImage> {
        match *self {
            DynamicImage::ImageLuma8(ref mut p) => Some(p),
            _ => None,
        }
    }

    /// Return a reference to an 8bit Grayscale image with an alpha channel
    pub fn as_luma_alpha8(&self) -> Option<&GrayAlphaImage> {
        match *self {
            DynamicImage::ImageLumaA8(ref p) => Some(p),
            _ => None,
        }
    }

    /// Return a mutable reference to an 8bit Grayscale image with an alpha channel
    pub fn as_mut_luma_alpha8(&mut self) -> Option<&mut GrayAlphaImage> {
        match *self {
            DynamicImage::ImageLumaA8(ref mut p) => Some(p),
            _ => None,
        }
    }

    /// Return this image's pixels as a byte vector.
    pub fn raw_pixels(&self) -> Vec<u8> {
        image_to_bytes(self)
    }

    /// Return a view on the raw sample buffer.
    pub fn as_flat_samples(&self) -> FlatSamples<&[u8]> {
        dynamic_map!(*self, ref p -> p.as_flat_samples())
    }

    /// Return this image's color type.
    pub fn color(&self) -> color::ColorType {
        match *self {
            DynamicImage::ImageLuma8(_) => color::ColorType::Gray(8),
            DynamicImage::ImageLumaA8(_) => color::ColorType::GrayA(8),
            DynamicImage::ImageRgb8(_) => color::ColorType::RGB(8),
            DynamicImage::ImageRgba8(_) => color::ColorType::RGBA(8),
            DynamicImage::ImageBgra8(_) => color::ColorType::BGRA(8),
            DynamicImage::ImageBgr8(_) => color::ColorType::BGR(8),
        }
    }

    /// Return a grayscale version of this image.
    pub fn grayscale(&self) -> DynamicImage {
        match *self {
            DynamicImage::ImageLuma8(ref p) => DynamicImage::ImageLuma8(p.clone()),
            DynamicImage::ImageLumaA8(ref p) => DynamicImage::ImageLuma8(imageops::grayscale(p)),
            DynamicImage::ImageRgb8(ref p) => DynamicImage::ImageLuma8(imageops::grayscale(p)),
            DynamicImage::ImageRgba8(ref p) => DynamicImage::ImageLuma8(imageops::grayscale(p)),
            DynamicImage::ImageBgr8(ref p) => DynamicImage::ImageLuma8(imageops::grayscale(p)),
            DynamicImage::ImageBgra8(ref p) => DynamicImage::ImageLuma8(imageops::grayscale(p)),
        }
    }

    /// Invert the colors of this image.
    /// This method operates inplace.
    pub fn invert(&mut self) {
        dynamic_map!(*self, ref mut p -> imageops::invert(p))
    }

    /// Resize this image using the specified filter algorithm.
    /// Returns a new image. The image's aspect ratio is preserved.
    /// The image is scaled to the maximum possible size that fits
    /// within the bounds specified by ```nwidth``` and ```nheight```.
    pub fn resize(&self, nwidth: u32, nheight: u32, filter: imageops::FilterType) -> DynamicImage {
        let (width2, height2) =
            resize_dimensions(self.width(), self.height(), nwidth, nheight, false);

        self.resize_exact(width2, height2, filter)
    }

    /// Resize this image using the specified filter algorithm.
    /// Returns a new image. Does not preserve aspect ratio.
    /// ```nwidth``` and ```nheight``` are the new image's dimensions
    pub fn resize_exact(
        &self,
        nwidth: u32,
        nheight: u32,
        filter: imageops::FilterType,
    ) -> DynamicImage {
        dynamic_map!(*self, ref p => imageops::resize(p, nwidth, nheight, filter))
    }

    /// Scale this image down to fit within a specific size.
    /// Returns a new image. The image's aspect ratio is preserved.
    /// The image is scaled to the maximum possible size that fits
    /// within the bounds specified by ```nwidth``` and ```nheight```.
    ///
    /// This method uses a fast integer algorithm where each source
    /// pixel contributes to exactly one target pixel.
    /// May give aliasing artifacts if new size is close to old size.
    pub fn thumbnail(&self, nwidth: u32, nheight: u32) -> DynamicImage {
        let (width2, height2) =
            resize_dimensions(self.width(), self.height(), nwidth, nheight, false);
        self.thumbnail_exact(width2, height2)
    }

    /// Scale this image down to a specific size.
    /// Returns a new image. Does not preserve aspect ratio.
    /// ```nwidth``` and ```nheight``` are the new image's dimensions.
    /// This method uses a fast integer algorithm where each source
    /// pixel contributes to exactly one target pixel.
    /// May give aliasing artifacts if new size is close to old size.
    pub fn thumbnail_exact(&self, nwidth: u32, nheight: u32) -> DynamicImage {
        dynamic_map!(*self, ref p => imageops::thumbnail(p, nwidth, nheight))
    }

    /// Resize this image using the specified filter algorithm.
    /// Returns a new image. The image's aspect ratio is preserved.
    /// The image is scaled to the maximum possible size that fits
    /// within the larger (relative to aspect ratio) of the bounds
    /// specified by ```nwidth``` and ```nheight```, then cropped to
    /// fit within the other bound.
    pub fn resize_to_fill(
        &self,
        nwidth: u32,
        nheight: u32,
        filter: imageops::FilterType,
    ) -> DynamicImage {
        let (width2, height2) =
            resize_dimensions(self.width(), self.height(), nwidth, nheight, true);

        let mut intermediate = self.resize_exact(width2, height2, filter);
        let (iwidth, iheight) = intermediate.dimensions();
        let ratio = u64::from(iwidth) * u64::from(nheight);
        let nratio = u64::from(nwidth) * u64::from(iheight);

        if nratio > ratio {
            intermediate.crop(0, (iheight - nheight) / 2, nwidth, nheight)
        } else {
            intermediate.crop((iwidth - nwidth) / 2, 0, nwidth, nheight)
        }
    }

    /// Performs a Gaussian blur on this image.
    /// ```sigma``` is a measure of how much to blur by.
    pub fn blur(&self, sigma: f32) -> DynamicImage {
        dynamic_map!(*self, ref p => imageops::blur(p, sigma))
    }

    /// Performs an unsharpen mask on this image.
    /// ```sigma``` is the amount to blur the image by.
    /// ```threshold``` is a control of how much to sharpen.
    ///
    /// See <https://en.wikipedia.org/wiki/Unsharp_masking#Digital_unsharp_masking>
    pub fn unsharpen(&self, sigma: f32, threshold: i32) -> DynamicImage {
        dynamic_map!(*self, ref p => imageops::unsharpen(p, sigma, threshold))
    }

    /// Filters this image with the specified 3x3 kernel.
    pub fn filter3x3(&self, kernel: &[f32]) -> DynamicImage {
        if kernel.len() != 9 {
            panic!("filter must be 3 x 3")
        }

        dynamic_map!(*self, ref p => imageops::filter3x3(p, kernel))
    }

    /// Adjust the contrast of this image.
    /// ```contrast``` is the amount to adjust the contrast by.
    /// Negative values decrease the contrast and positive values increase the contrast.
    pub fn adjust_contrast(&self, c: f32) -> DynamicImage {
        dynamic_map!(*self, ref p => imageops::contrast(p, c))
    }

    /// Brighten the pixels of this image.
    /// ```value``` is the amount to brighten each pixel by.
    /// Negative values decrease the brightness and positive values increase it.
    pub fn brighten(&self, value: i32) -> DynamicImage {
        dynamic_map!(*self, ref p => imageops::brighten(p, value))
    }

    /// Hue rotate the supplied image.
    /// `value` is the degrees to rotate each pixel by.
    /// 0 and 360 do nothing, the rest rotates by the given degree value.
    /// just like the css webkit filter hue-rotate(180)
    pub fn huerotate(&self, value: i32) -> DynamicImage {
        dynamic_map!(*self, ref p => imageops::huerotate(p, value))
    }

    /// Flip this image vertically
    pub fn flipv(&self) -> DynamicImage {
        dynamic_map!(*self, ref p => imageops::flip_vertical(p))
    }

    /// Flip this image horizontally
    pub fn fliph(&self) -> DynamicImage {
        dynamic_map!(*self, ref p => imageops::flip_horizontal(p))
    }

    /// Rotate this image 90 degrees clockwise.
    pub fn rotate90(&self) -> DynamicImage {
        dynamic_map!(*self, ref p => imageops::rotate90(p))
    }

    /// Rotate this image 180 degrees clockwise.
    pub fn rotate180(&self) -> DynamicImage {
        dynamic_map!(*self, ref p => imageops::rotate180(p))
    }

    /// Rotate this image 270 degrees clockwise.
    pub fn rotate270(&self) -> DynamicImage {
        dynamic_map!(*self, ref p => imageops::rotate270(p))
    }

    /// Encode this image and write it to ```w```
    pub fn write_to<W: Write, F: Into<ImageOutputFormat>>(
        &self,
        w: &mut W,
        format: F,
    ) -> ImageResult<()> {
        let mut bytes = self.raw_pixels();
        let (width, height) = self.dimensions();
        let mut color = self.color();
        let format = format.into();

        #[allow(deprecated)]
        match format {
            #[cfg(feature = "png_codec")]
            image::ImageOutputFormat::PNG => {
                let p = png::PNGEncoder::new(w);
                match *self {
                    DynamicImage::ImageBgra8(_) => {
                        bytes=self.to_rgba().iter().cloned().collect();
                        color=color::ColorType::RGBA(8);
                    },
                    DynamicImage::ImageBgr8(_) => {
                        bytes=self.to_rgb().iter().cloned().collect();
                        color=color::ColorType::RGB(8);
                    },
                    _ => {},
                }
                p.encode(&bytes, width, height, color)?;
                Ok(())
            }
            #[cfg(feature = "pnm")]
            image::ImageOutputFormat::PNM(subtype) => {
                let mut p = pnm::PNMEncoder::new(w).with_subtype(subtype);
                match *self {
                    DynamicImage::ImageBgra8(_) => {
                        bytes=self.to_rgba().iter().cloned().collect();
                        color=color::ColorType::RGBA(8);
                    },
                    DynamicImage::ImageBgr8(_) => {
                        bytes=self.to_rgb().iter().cloned().collect();
                        color=color::ColorType::RGB(8);
                    },
                    _ => {},
                }
                p.encode(&bytes[..], width, height, color)?;
                Ok(())
            }
            #[cfg(feature = "jpeg")]
            image::ImageOutputFormat::JPEG(quality) => {
                let mut j = jpeg::JPEGEncoder::new_with_quality(w, quality);

                j.encode(&bytes, width, height, color)?;
                Ok(())
            }

            #[cfg(feature = "gif_codec")]
            image::ImageOutputFormat::GIF => {
                let mut g = gif::Encoder::new(w);

                try!(g.encode(&gif::Frame::from_rgba(
                    width as u16,
                    height as u16,
                    &mut *self.to_rgba().iter().cloned().collect::<Vec<u8>>()
                )));
                Ok(())
            }

            #[cfg(feature = "ico")]
            image::ImageOutputFormat::ICO => {
                let i = ico::ICOEncoder::new(w);

                i.encode(&bytes, width, height, color)?;
                Ok(())
            }

            #[cfg(feature = "bmp")]
            image::ImageOutputFormat::BMP => {
                let mut b = bmp::BMPEncoder::new(w);
                b.encode(&bytes, width, height, color)?;
                Ok(())
            }

            image::ImageOutputFormat::Unsupported(msg) => {
                Err(image::ImageError::UnsupportedError(msg))
            }
        }
    }

    /// Saves the buffer to a file at the path specified.
    ///
    /// The image format is derived from the file extension.
    pub fn save<Q>(&self, path: Q) -> io::Result<()>
    where
        Q: AsRef<Path>,
    {
        dynamic_map!(*self, ref p -> {
            p.save(path)
        })
    }

    /// Saves the buffer to a file at the specified path in
    /// the specified format.
    ///
    /// See [`save_buffer_with_format`](fn.save_buffer_with_format.html) for
    /// supported types.
    pub fn save_with_format<Q>(&self, path: Q, format: ImageFormat) -> io::Result<()>
    where
        Q: AsRef<Path>,
    {
        dynamic_map!(*self, ref p -> {
            p.save_with_format(path, format)
        })
    }
}

#[allow(deprecated)]
impl GenericImageView for DynamicImage {
    type Pixel = color::Rgba<u8>;
    type InnerImageView = Self;

    fn dimensions(&self) -> (u32, u32) {
        dynamic_map!(*self, ref p -> p.dimensions())
    }

    fn bounds(&self) -> (u32, u32, u32, u32) {
        dynamic_map!(*self, ref p -> p.bounds())
    }

    fn get_pixel(&self, x: u32, y: u32) -> color::Rgba<u8> {
        dynamic_map!(*self, ref p -> p.get_pixel(x, y).to_rgba())
    }

    fn inner(&self) -> &Self::InnerImageView {
        self
    }
}

#[allow(deprecated)]
impl GenericImage for DynamicImage {
    type InnerImage = DynamicImage;

    fn put_pixel(&mut self, x: u32, y: u32, pixel: color::Rgba<u8>) {
        match *self {
            DynamicImage::ImageLuma8(ref mut p) => p.put_pixel(x, y, pixel.to_luma()),
            DynamicImage::ImageLumaA8(ref mut p) => p.put_pixel(x, y, pixel.to_luma_alpha()),
            DynamicImage::ImageRgb8(ref mut p) => p.put_pixel(x, y, pixel.to_rgb()),
            DynamicImage::ImageRgba8(ref mut p) => p.put_pixel(x, y, pixel),
            DynamicImage::ImageBgr8(ref mut p) => p.put_pixel(x, y, pixel.to_bgr()),
            DynamicImage::ImageBgra8(ref mut p) => p.put_pixel(x, y, pixel.to_bgra()),
        }
    }
    /// DEPRECATED: Use iterator `pixels_mut` to blend the pixels directly.
    fn blend_pixel(&mut self, x: u32, y: u32, pixel: color::Rgba<u8>) {
        match *self {
            DynamicImage::ImageLuma8(ref mut p) => p.blend_pixel(x, y, pixel.to_luma()),
            DynamicImage::ImageLumaA8(ref mut p) => p.blend_pixel(x, y, pixel.to_luma_alpha()),
            DynamicImage::ImageRgb8(ref mut p) => p.blend_pixel(x, y, pixel.to_rgb()),
            DynamicImage::ImageRgba8(ref mut p) => p.blend_pixel(x, y, pixel),

            DynamicImage::ImageBgr8(ref mut p) => p.blend_pixel(x, y, pixel.to_bgr()),
            DynamicImage::ImageBgra8(ref mut p) => p.blend_pixel(x, y, pixel.to_bgra()),
        }
    }

    /// DEPRECATED: Do not use is function: It is unimplemented!
    fn get_pixel_mut(&mut self, _: u32, _: u32) -> &mut color::Rgba<u8> {
        unimplemented!()
    }

    fn inner_mut(&mut self) -> &mut Self::InnerImage {
        self
    }
}

/// Decodes an image and stores it into a dynamic image
pub fn decoder_to_image<'a, I: ImageDecoder<'a>>(codec: I) -> ImageResult<DynamicImage> {
    let color = codec.colortype();
    let (w, h) = codec.dimensions();
    let buf = codec.read_image()?;

    // TODO: Avoid this cast by having ImageBuffer use u64's
    assert!(w <= u32::max_value() as u64);
    assert!(h <= u32::max_value() as u64);
    let (w, h) = (w as u32, h as u32);

    let image = match color {
        color::ColorType::RGB(8) => {
            ImageBuffer::from_raw(w, h, buf).map(DynamicImage::ImageRgb8)
        }

        color::ColorType::RGBA(8) => {
            ImageBuffer::from_raw(w, h, buf).map(DynamicImage::ImageRgba8)
        }

        color::ColorType::BGR(8) => {
            ImageBuffer::from_raw(w, h, buf).map(DynamicImage::ImageBgr8)
        }

        color::ColorType::BGRA(8) => {
            ImageBuffer::from_raw(w, h, buf).map(DynamicImage::ImageBgra8)
        }

        color::ColorType::Gray(8) => {
            ImageBuffer::from_raw(w, h, buf).map(DynamicImage::ImageLuma8)
        }

        color::ColorType::GrayA(8) => {
            ImageBuffer::from_raw(w, h, buf).map(DynamicImage::ImageLumaA8)
        }
        color::ColorType::Gray(bit_depth)
            if bit_depth == 1 || bit_depth == 2 || bit_depth == 4 =>
        {
            gray_to_luma8(bit_depth, w, h, &buf).map(DynamicImage::ImageLuma8)
        }
        _ => return Err(image::ImageError::UnsupportedColor(color)),
    };
    match image {
        Some(image) => Ok(image),
        None => Err(image::ImageError::DimensionError),
    }
}

fn gray_to_luma8(bit_depth: u8, w: u32, h: u32, buf: &[u8]) -> Option<GrayImage> {
    // Note: this conversion assumes that the scanlines begin on byte boundaries
    let mask = (1u8 << bit_depth as usize) - 1;
    let scaling_factor = 255 / ((1 << bit_depth as usize) - 1);
    let bit_width = w * u32::from(bit_depth);
    let skip = if bit_width % 8 == 0 {
        0
    } else {
        (8 - bit_width % 8) / u32::from(bit_depth)
    };
    let row_len = w + skip;
    let mut p = Vec::new();
    let mut i = 0;
    for v in buf {
        for shift in num_iter::range_step_inclusive(8i8-(bit_depth as i8), 0, -(bit_depth as i8)) {
            // skip the pixels that can be neglected because scanlines should
            // start at byte boundaries
            if i % (row_len as usize) < (w as usize) {
                let pixel = (v & mask << shift as usize) >> shift as usize;
                p.push(pixel * scaling_factor);
            }
            i += 1;
        }
    }
    ImageBuffer::from_raw(w, h, p)
}

#[allow(deprecated)]
fn image_to_bytes(image: &DynamicImage) -> Vec<u8> {
    match *image {
        // TODO: consider transmuting
        DynamicImage::ImageLuma8(ref a) => a.iter().cloned().collect(),

        DynamicImage::ImageLumaA8(ref a) => a.iter().cloned().collect(),

        DynamicImage::ImageRgb8(ref a) => a.iter().cloned().collect(),

        DynamicImage::ImageRgba8(ref a) => a.iter().cloned().collect(),

        DynamicImage::ImageBgr8(ref a) => a.iter().cloned().collect(),

        DynamicImage::ImageBgra8(ref a) => a.iter().cloned().collect(),
    }
}

/// Open the image located at the path specified.
/// The image's format is determined from the path's file extension.
pub fn open<P>(path: P) -> ImageResult<DynamicImage>
where
    P: AsRef<Path>,
{
    // thin wrapper function to strip generics before calling open_impl
    open_impl(path.as_ref())
}

fn open_impl(path: &Path) -> ImageResult<DynamicImage> {
    let fin = match File::open(path) {
        Ok(f) => f,
        Err(err) => return Err(image::ImageError::IoError(err)),
    };
    let fin = BufReader::new(fin);

    let ext = path.extension()
        .and_then(|s| s.to_str())
        .map_or("".to_string(), |s| s.to_ascii_lowercase());

    let format = match &ext[..] {
        "jpg" | "jpeg" => image::ImageFormat::JPEG,
        "png" => image::ImageFormat::PNG,
        "gif" => image::ImageFormat::GIF,
        "webp" => image::ImageFormat::WEBP,
        "tif" | "tiff" => image::ImageFormat::TIFF,
        "tga" => image::ImageFormat::TGA,
        "bmp" => image::ImageFormat::BMP,
        "ico" => image::ImageFormat::ICO,
        "hdr" => image::ImageFormat::HDR,
        "pbm" | "pam" | "ppm" | "pgm" => image::ImageFormat::PNM,
        format => {
            return Err(image::ImageError::UnsupportedError(format!(
                "Image format image/{:?} is not supported.",
                format
            )))
        }
    };

    load(fin, format)
}

/// Read the dimensions of the image located at the specified path.
/// This is faster than fully loading the image and then getting its dimensions.
pub fn image_dimensions<P>(path: P) -> ImageResult<(u32, u32)>
where
    P: AsRef<Path>
{
    // thin wrapper function to strip generics before calling open_impl
    image_dimensions_impl(path.as_ref())
}

fn image_dimensions_impl(path: &Path) -> ImageResult<(u32, u32)> {
    let fin = File::open(path)?;
    let fin = BufReader::new(fin);

    let ext = path
        .extension()
        .and_then(|s| s.to_str())
        .map_or("".to_string(), |s| s.to_ascii_lowercase());

    let (w, h): (u64, u64) = match &ext[..] {
        #[cfg(feature = "jpeg")]
        "jpg" | "jpeg" => jpeg::JPEGDecoder::new(fin)?.dimensions(),
        #[cfg(feature = "png_codec")]
        "png" => png::PNGDecoder::new(fin)?.dimensions(),
        #[cfg(feature = "gif_codec")]
        "gif" => gif::Decoder::new(fin)?.dimensions(),
        #[cfg(feature = "webp")]
        "webp" => webp::WebpDecoder::new(fin)?.dimensions(),
        #[cfg(feature = "tiff")]
        "tif" | "tiff" => tiff::TIFFDecoder::new(fin)?.dimensions(),
        #[cfg(feature = "tga")]
        "tga" => tga::TGADecoder::new(fin)?.dimensions(),
        #[cfg(feature = "bmp")]
        "bmp" => bmp::BMPDecoder::new(fin)?.dimensions(),
        #[cfg(feature = "ico")]
        "ico" => ico::ICODecoder::new(fin)?.dimensions(),
        #[cfg(feature = "hdr")]
        "hdr" => hdr::HDRAdapter::new(fin)?.dimensions(),
        #[cfg(feature = "pnm")]
        "pbm" | "pam" | "ppm" | "pgm" => {
            pnm::PNMDecoder::new(fin)?.dimensions()
        }
        format => return Err(image::ImageError::UnsupportedError(format!(
            "Image format image/{:?} is not supported.",
            format
        ))),
    };
    if w >= u32::MAX as u64 || h >= u32::MAX as u64 {
        return Err(image::ImageError::DimensionError);
    }
    Ok((w as u32, h as u32))
}


/// Saves the supplied buffer to a file at the path specified.
///
/// The image format is derived from the file extension. The buffer is assumed to have
/// the correct format according to the specified color type.

/// This will lead to corrupted files if the buffer contains malformed data. Currently only
/// jpeg, png, ico, pnm, bmp and tiff files are supported.
pub fn save_buffer<P>(
    path: P,
    buf: &[u8],
    width: u32,
    height: u32,
    color: color::ColorType,
) -> io::Result<()>
where
    P: AsRef<Path>,
{
    // thin wrapper function to strip generics before calling save_buffer_impl
    save_buffer_impl(path.as_ref(), buf, width, height, color)
}

fn save_buffer_impl(
    path: &Path,
    buf: &[u8],
    width: u32,
    height: u32,
    color: color::ColorType,
) -> io::Result<()> {
    let fout = &mut BufWriter::new(File::create(path)?);
    let ext = path.extension()
        .and_then(|s| s.to_str())
        .map_or("".to_string(), |s| s.to_ascii_lowercase());

    match &*ext {
        #[cfg(feature = "ico")]
        "ico" => ico::ICOEncoder::new(fout).encode(buf, width, height, color),
        #[cfg(feature = "jpeg")]
        "jpg" | "jpeg" => jpeg::JPEGEncoder::new(fout).encode(buf, width, height, color),
        #[cfg(feature = "png_codec")]
        "png" => png::PNGEncoder::new(fout).encode(buf, width, height, color),
        #[cfg(feature = "pnm")]
        "pbm" => pnm::PNMEncoder::new(fout)
            .with_subtype(pnm::PNMSubtype::Bitmap(pnm::SampleEncoding::Binary))
            .encode(buf, width, height, color),
        #[cfg(feature = "pnm")]
        "pgm" => pnm::PNMEncoder::new(fout)
            .with_subtype(pnm::PNMSubtype::Graymap(pnm::SampleEncoding::Binary))
            .encode(buf, width, height, color),
        #[cfg(feature = "pnm")]
        "ppm" => pnm::PNMEncoder::new(fout)
            .with_subtype(pnm::PNMSubtype::Pixmap(pnm::SampleEncoding::Binary))
            .encode(buf, width, height, color),
        #[cfg(feature = "pnm")]
        "pam" => pnm::PNMEncoder::new(fout).encode(buf, width, height, color),
        #[cfg(feature = "bmp")]
        "bmp" => bmp::BMPEncoder::new(fout).encode(buf, width, height, color),
        #[cfg(feature = "tiff")]
        "tif" | "tiff" => tiff::TiffEncoder::new(fout).encode(buf, width, height, color)
            .map_err(|e| io::Error::new(io::ErrorKind::Other, Box::new(e))), // FIXME: see https://github.com/image-rs/image/issues/921
        format => Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            &format!("Unsupported image format image/{:?}", format)[..],
        )),
    }
}

/// Saves the supplied buffer to a file at the path specified
/// in the specified format.
///
/// The buffer is assumed to have the correct format according
/// to the specified color type.
/// This will lead to corrupted files if the buffer contains
/// malformed data. Currently only jpeg, png, ico, bmp and
/// tiff files are supported.
pub fn save_buffer_with_format<P>(
    path: P,
    buf: &[u8],
    width: u32,
    height: u32,
    color: color::ColorType,
    format: ImageFormat,
) -> io::Result<()>
where
    P: AsRef<Path>,
{
    // thin wrapper function to strip generics
    save_buffer_with_format_impl(path.as_ref(), buf, width, height, color, format)
}

fn save_buffer_with_format_impl(
    path: &Path,
    buf: &[u8],
    width: u32,
    height: u32,
    color: color::ColorType,
    format: ImageFormat,
) -> io::Result<()> {
    let fout = &mut BufWriter::new(File::create(path)?);

    match format {
        #[cfg(feature = "ico")]
        image::ImageFormat::ICO => ico::ICOEncoder::new(fout).encode(buf, width, height, color),
        #[cfg(feature = "jpeg")]
        image::ImageFormat::JPEG => jpeg::JPEGEncoder::new(fout).encode(buf, width, height, color),
        #[cfg(feature = "png_codec")]
        image::ImageFormat::PNG => png::PNGEncoder::new(fout).encode(buf, width, height, color),
        #[cfg(feature = "bmp")]
        image::ImageFormat::BMP => bmp::BMPEncoder::new(fout).encode(buf, width, height, color),
        #[cfg(feature = "tiff")]
        image::ImageFormat::TIFF => tiff::TiffEncoder::new(fout)
            .encode(buf, width, height, color)
            .map_err(|e| io::Error::new(io::ErrorKind::Other, Box::new(e))),
        _ => Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            &format!("Unsupported image format image/{:?}", format)[..],
        )),
    }
}

/// Create a new image from a Reader
pub fn load<R: BufRead + Seek>(r: R, format: ImageFormat) -> ImageResult<DynamicImage> {
    #[allow(deprecated, unreachable_patterns)]
    // Default is unreachable if all features are supported.
    match format {
        #[cfg(feature = "png_codec")]
        image::ImageFormat::PNG => decoder_to_image(png::PNGDecoder::new(r)?),
        #[cfg(feature = "gif_codec")]
        image::ImageFormat::GIF => decoder_to_image(gif::Decoder::new(r)?),
        #[cfg(feature = "jpeg")]
        image::ImageFormat::JPEG => decoder_to_image(jpeg::JPEGDecoder::new(r)?),
        #[cfg(feature = "webp")]
        image::ImageFormat::WEBP => decoder_to_image(webp::WebpDecoder::new(r)?),
        #[cfg(feature = "tiff")]
        image::ImageFormat::TIFF => decoder_to_image(try!(tiff::TIFFDecoder::new(r))),
        #[cfg(feature = "tga")]
        image::ImageFormat::TGA => decoder_to_image(tga::TGADecoder::new(r)?),
        #[cfg(feature = "bmp")]
        image::ImageFormat::BMP => decoder_to_image(bmp::BMPDecoder::new(r)?),
        #[cfg(feature = "ico")]
        image::ImageFormat::ICO => decoder_to_image(try!(ico::ICODecoder::new(r))),
        #[cfg(feature = "hdr")]
        image::ImageFormat::HDR => decoder_to_image(try!(hdr::HDRAdapter::new(BufReader::new(r)))),
        #[cfg(feature = "pnm")]
        image::ImageFormat::PNM => decoder_to_image(try!(pnm::PNMDecoder::new(BufReader::new(r)))),
        _ => Err(image::ImageError::UnsupportedError(format!(
            "A decoder for {:?} is not available.",
            format
        ))),
    }
}

static MAGIC_BYTES: [(&'static [u8], ImageFormat); 17] = [
    (b"\x89PNG\r\n\x1a\n", ImageFormat::PNG),
    (&[0xff, 0xd8, 0xff], ImageFormat::JPEG),
    (b"GIF89a", ImageFormat::GIF),
    (b"GIF87a", ImageFormat::GIF),
    (b"RIFF", ImageFormat::WEBP), // TODO: better magic byte detection, see https://github.com/image-rs/image/issues/660
    (b"MM.*", ImageFormat::TIFF),
    (b"II*.", ImageFormat::TIFF),
    (b"BM", ImageFormat::BMP),
    (&[0, 0, 1, 0], ImageFormat::ICO),
    (b"#?RADIANCE", ImageFormat::HDR),
    (b"P1", ImageFormat::PNM),
    (b"P2", ImageFormat::PNM),
    (b"P3", ImageFormat::PNM),
    (b"P4", ImageFormat::PNM),
    (b"P5", ImageFormat::PNM),
    (b"P6", ImageFormat::PNM),
    (b"P7", ImageFormat::PNM),
];

/// Create a new image from a byte slice
///
/// Makes an educated guess about the image format.
/// TGA is not supported by this function.
pub fn load_from_memory(buffer: &[u8]) -> ImageResult<DynamicImage> {
    load_from_memory_with_format(buffer, try!(guess_format(buffer)))
}

/// Create a new image from a byte slice
#[inline(always)]
pub fn load_from_memory_with_format(buf: &[u8], format: ImageFormat) -> ImageResult<DynamicImage> {
    let b = io::Cursor::new(buf);
    load(b, format)
}

/// Guess image format from memory block
///
/// Makes an educated guess about the image format based on the Magic Bytes at the beginning.
/// TGA is not supported by this function.
/// This is not to be trusted on the validity of the whole memory block
pub fn guess_format(buffer: &[u8]) -> ImageResult<ImageFormat> {
    for &(signature, format) in &MAGIC_BYTES {
        if buffer.starts_with(signature) {
            return Ok(format);
        }
    }
    Err(image::ImageError::UnsupportedError(
        "Unsupported image format".to_string(),
    ))
}

/// Calculates the width and height an image should be resized to.
/// This preserves aspect ratio, and based on the `fill` parameter
/// will either fill the dimensions to fit inside the smaller constraint
/// (will overflow the specified bounds on one axis to preserve
/// aspect ratio), or will shrink so that both dimensions are
/// completely contained with in the given `width` and `height`,
/// with empty space on one axis.
fn resize_dimensions(width: u32, height: u32, nwidth: u32, nheight: u32, fill: bool) -> (u32, u32) {
    let ratio = u64::from(width) * u64::from(nheight);
    let nratio = u64::from(nwidth) * u64::from(height);

    let use_width = if fill {
        nratio > ratio
    } else {
        nratio <= ratio
    };
    let intermediate = if use_width {
        u64::from(height) * u64::from(nwidth) / u64::from(width)
    } else {
        u64::from(width) * u64::from(nheight) / u64::from(height)
    };
    if use_width {
        if intermediate <= u64::from(::std::u32::MAX) {
            (nwidth, intermediate as u32)
        } else {
            (
                (u64::from(nwidth) * u64::from(::std::u32::MAX) / intermediate) as u32,
                ::std::u32::MAX,
            )
        }
    } else if intermediate <= u64::from(::std::u32::MAX) {
        (intermediate as u32, nheight)
    } else {
        (
            ::std::u32::MAX,
            (u64::from(nheight) * u64::from(::std::u32::MAX) / intermediate) as u32,
        )
    }
}

#[cfg(test)]
mod bench {
    #[cfg(feature = "benchmarks")]
    use test;

    #[bench]
    #[cfg(feature = "benchmarks")]
    fn bench_conversion(b: &mut test::Bencher) {
        let a = super::DynamicImage::ImageRgb8(::ImageBuffer::new(1000, 1000));
        b.iter(|| a.to_luma());
        b.bytes = 1000 * 1000 * 3
    }
}

#[cfg(test)]
mod test {
    #[test]
    fn test_empty_file() {
        assert!(super::load_from_memory(b"").is_err());
    }

    quickcheck! {
        fn resize_bounds_correctly_width(old_w: u32, new_w: u32) -> bool {
            if old_w == 0 || new_w == 0 { return true; }
            let result = super::resize_dimensions(old_w, 400, new_w, ::std::u32::MAX, false);
            result.0 == new_w && result.1 == (400 as f64 * new_w as f64 / old_w as f64) as u32
        }
    }

    quickcheck! {
        fn resize_bounds_correctly_height(old_h: u32, new_h: u32) -> bool {
            if old_h == 0 || new_h == 0 { return true; }
            let result = super::resize_dimensions(400, old_h, ::std::u32::MAX, new_h, false);
            result.1 == new_h && result.0 == (400 as f64 * new_h as f64 / old_h as f64) as u32
        }
    }

    #[test]
    fn resize_handles_fill() {
        let result = super::resize_dimensions(100, 200, 200, 500, true);
        assert!(result.0 == 250);
        assert!(result.1 == 500);

        let result = super::resize_dimensions(200, 100, 500, 200, true);
        assert!(result.0 == 500);
        assert!(result.1 == 250);
    }

    #[test]
    fn resize_handles_overflow() {
        let result = super::resize_dimensions(100, ::std::u32::MAX, 200, ::std::u32::MAX, true);
        assert!(result.0 == 100);
        assert!(result.1 == ::std::u32::MAX);

        let result = super::resize_dimensions(::std::u32::MAX, 100, ::std::u32::MAX, 200, true);
        assert!(result.0 == ::std::u32::MAX);
        assert!(result.1 == 100);
    }

    #[test]
    fn gray_to_luma8_skip() {
        let check = |bit_depth, w, h, from, to| {
            assert_eq!(
                super::gray_to_luma8(bit_depth, w, h, from).map(super::GrayImage::into_raw),
                Some(to));
        };
        // Bit depth 1, skip is more than half a byte
        check(
            1, 10, 2,
            &[0b11110000, 0b11000000, 0b00001111, 0b11000000],
            vec![255, 255, 255, 255, 0, 0, 0, 0, 255, 255, 0, 0, 0, 0, 255, 255, 255, 255, 255, 255]);
        // Bit depth 2, skip is more than half a byte
        check(
            2, 5, 2,
            &[0b11110000, 0b11000000, 0b00001111, 0b11000000],
            vec![255, 255, 0, 0, 255, 0, 0, 255, 255, 255]);
        // Bit depth 2, skip is 0
        check(
            2, 4, 2,
            &[0b11110000, 0b00001111],
            vec![255, 255, 0, 0, 0, 0, 255, 255]);
        // Bit depth 4, skip is half a byte
        check(
            4, 1, 2,
            &[0b11110011, 0b00001100],
            vec![255, 0]);
    }

    #[cfg(feature = "jpeg")]
    #[test]
    fn image_dimensions() {
        let im_path = "./tests/images/jpg/progressive/cat.jpg";
        let dims = super::image_dimensions(im_path).unwrap();
        assert_eq!(dims, (320, 240));
    }
}
