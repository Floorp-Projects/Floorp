use std::error::Error;
use std::fmt;
use std::io;
use std::io::Read;
use std::mem;
use std::ops::{Deref, DerefMut};

use buffer::{ImageBuffer, Pixel};
use color;
use color::ColorType;

use animation::Frames;

#[cfg(feature = "pnm")]
use pnm::PNMSubtype;

/// An enumeration of Image errors
#[derive(Debug)]
pub enum ImageError {
    /// The Image is not formatted properly
    FormatError(String),

    /// The Image's dimensions are either too small or too large
    DimensionError,

    /// The Decoder does not support this image format
    UnsupportedError(String),

    /// The Decoder does not support this color type
    UnsupportedColor(ColorType),

    /// Not enough data was provided to the Decoder
    /// to decode the image
    NotEnoughData,

    /// An I/O Error occurred while decoding the image
    IoError(io::Error),

    /// The end of the image has been reached
    ImageEnd,

    /// There is not enough memory to complete the given operation
    InsufficientMemory,
}

impl fmt::Display for ImageError {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        match *self {
            ImageError::FormatError(ref e) => write!(fmt, "Format error: {}", e),
            ImageError::DimensionError => write!(
                fmt,
                "The Image's dimensions are either too \
                 small or too large"
            ),
            ImageError::UnsupportedError(ref f) => write!(
                fmt,
                "The Decoder does not support the \
                 image format `{}`",
                f
            ),
            ImageError::UnsupportedColor(ref c) => write!(
                fmt,
                "The decoder does not support \
                 the color type `{:?}`",
                c
            ),
            ImageError::NotEnoughData => write!(
                fmt,
                "Not enough data was provided to the \
                 Decoder to decode the image"
            ),
            ImageError::IoError(ref e) => e.fmt(fmt),
            ImageError::ImageEnd => write!(fmt, "The end of the image has been reached"),
            ImageError::InsufficientMemory => write!(fmt, "Insufficient memory"),
        }
    }
}

impl Error for ImageError {
    fn description(&self) -> &str {
        match *self {
            ImageError::FormatError(..) => "Format error",
            ImageError::DimensionError => "Dimension error",
            ImageError::UnsupportedError(..) => "Unsupported error",
            ImageError::UnsupportedColor(..) => "Unsupported color",
            ImageError::NotEnoughData => "Not enough data",
            ImageError::IoError(..) => "IO error",
            ImageError::ImageEnd => "Image end",
            ImageError::InsufficientMemory => "Insufficient memory",
        }
    }

    fn cause(&self) -> Option<&Error> {
        match *self {
            ImageError::IoError(ref e) => Some(e),
            _ => None,
        }
    }
}

impl From<io::Error> for ImageError {
    fn from(err: io::Error) -> ImageError {
        ImageError::IoError(err)
    }
}

/// Result of an image decoding/encoding process
pub type ImageResult<T> = Result<T, ImageError>;

/// An enumeration of supported image formats.
/// Not all formats support both encoding and decoding.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum ImageFormat {
    /// An Image in PNG Format
    PNG,

    /// An Image in JPEG Format
    JPEG,

    /// An Image in GIF Format
    GIF,

    /// An Image in WEBP Format
    WEBP,

    /// An Image in general PNM Format
    PNM,

    /// An Image in TIFF Format
    TIFF,

    /// An Image in TGA Format
    TGA,

    /// An Image in BMP Format
    BMP,

    /// An Image in ICO Format
    ICO,

    /// An Image in Radiance HDR Format
    HDR,
}

/// An enumeration of supported image formats for encoding.
#[derive(Clone, PartialEq, Eq, Debug)]
pub enum ImageOutputFormat {
    #[cfg(feature = "png_codec")]
    /// An Image in PNG Format
    PNG,

    #[cfg(feature = "jpeg")]
    /// An Image in JPEG Format with specified quality
    JPEG(u8),

    #[cfg(feature = "pnm")]
    /// An Image in one of the PNM Formats
    PNM(PNMSubtype),

    #[cfg(feature = "gif_codec")]
    /// An Image in GIF Format
    GIF,

    #[cfg(feature = "ico")]
    /// An Image in ICO Format
    ICO,

    #[cfg(feature = "bmp")]
    /// An Image in BMP Format
    BMP,

    /// A value for signalling an error: An unsupported format was requested
    // Note: When TryFrom is stabilized, this value should not be needed, and
    // a TryInto<ImageOutputFormat> should be used instead of an Into<ImageOutputFormat>.
    Unsupported(String),
}

impl From<ImageFormat> for ImageOutputFormat {
    fn from(fmt: ImageFormat) -> Self {
        match fmt {
            #[cfg(feature = "png_codec")]
            ImageFormat::PNG => ImageOutputFormat::PNG,
            #[cfg(feature = "jpeg")]
            ImageFormat::JPEG => ImageOutputFormat::JPEG(75),
            #[cfg(feature = "pnm")]
            ImageFormat::PNM => ImageOutputFormat::PNM(PNMSubtype::ArbitraryMap),
            #[cfg(feature = "gif_codec")]
            ImageFormat::GIF => ImageOutputFormat::GIF,
            #[cfg(feature = "ico")]
            ImageFormat::ICO => ImageOutputFormat::ICO,
            #[cfg(feature = "bmp")]
            ImageFormat::BMP => ImageOutputFormat::BMP,

            f => ImageOutputFormat::Unsupported(format!(
                "Image format {:?} not supported for encoding.",
                f
            )),
        }
    }
}

// This struct manages buffering associated with implementing `Read` and `Seek` on decoders that can
// must decode ranges of bytes at a time.
pub(crate) struct ImageReadBuffer {
    scanline_bytes: usize,
    buffer: Vec<u8>,
    consumed: usize,

    total_bytes: usize,
    offset: usize,
}
impl ImageReadBuffer {
    pub fn new(scanline_bytes: usize, total_bytes: usize) -> Self {
        Self {
            scanline_bytes,
            buffer: Vec::new(),
            consumed: 0,
            total_bytes,
            offset: 0,
        }
    }
    pub fn read<F>(&mut self, buf: &mut [u8], mut read_scanline: F) -> io::Result<usize>
    where
        F: FnMut(&mut [u8]) -> io::Result<usize>,
    {
        if self.buffer.len() == self.consumed {
            if self.offset == self.total_bytes {
                return Ok(0);
            } else if buf.len() >= self.scanline_bytes {
                // If there is nothing buffered and the user requested a full scanline worth of
                // data, skip buffering.
                let bytes_read = read_scanline(&mut buf[..self.scanline_bytes])?;
                self.offset += bytes_read;
                return Ok(bytes_read);
            } else {
                // Lazily allocate buffer the first time that read is called with a buffer smaller
                // than the scanline size.
                if self.buffer.is_empty() {
                    self.buffer.resize(self.scanline_bytes, 0);
                }

                self.consumed = 0;
                let bytes_read = read_scanline(&mut self.buffer[..])?;
                self.buffer.resize(bytes_read, 0);
                self.offset += bytes_read;

                assert!(bytes_read == self.scanline_bytes || self.offset == self.total_bytes);
            }
        }

        // Finally, copy bytes into output buffer.
        let bytes_buffered = self.buffer.len() - self.consumed;
        if bytes_buffered > buf.len() {
            ::copy_memory(&self.buffer[self.consumed..][..buf.len()], &mut buf[..]);
            self.consumed += buf.len();
            Ok(buf.len())
        } else {
            ::copy_memory(&self.buffer[self.consumed..], &mut buf[..bytes_buffered]);
            self.consumed = self.buffer.len();
            Ok(bytes_buffered)
        }
    }
}

/// Decodes a specific region of the image, represented by the rectangle
/// starting from ```x``` and ```y``` and having ```length``` and ```width```
pub(crate) fn load_rect<D, F, F1, F2>(x: u64, y: u64, width: u64, height: u64, buf: &mut [u8],
                                      progress_callback: F,
                                      decoder: &mut D,
                                      mut seek_scanline: F1,
                                      mut read_scanline: F2) -> ImageResult<()>
    where D: ImageDecoder,
          F: Fn(Progress),
          F1: FnMut(&mut D, u64) -> io::Result<()>,
          F2: FnMut(&mut D, &mut [u8]) -> io::Result<usize>
{
    let dimensions = decoder.dimensions();
    let row_bytes = decoder.row_bytes();
    let scanline_bytes = decoder.scanline_bytes();
    let bits_per_pixel = color::bits_per_pixel(decoder.colortype()) as u64;
    let total_bits = width * height * bits_per_pixel;

    let mut bits_read = 0u64;
    let mut current_scanline = 0;
    let mut tmp = Vec::new();

    {
        // Read a range of the image starting from bit number `start` and continuing until bit
        // number `end`. Updates `current_scanline` and `bits_read` appropiately.
        let mut read_image_range = |start: u64, end: u64| -> ImageResult<()> {
            let target_scanline = start / (scanline_bytes * 8);
            if target_scanline != current_scanline {
                seek_scanline(decoder, target_scanline)?;
                current_scanline = target_scanline;
            }

            let mut position = current_scanline * scanline_bytes * 8;
            while position < end {
                if position >= start && end - position >= scanline_bytes * 8 && bits_read % 8 == 0 {
                    read_scanline(decoder, &mut buf[((bits_read/8) as usize)..]
                                                   [..(scanline_bytes as usize)])?;
                    bits_read += scanline_bytes * 8;
                } else {
                    tmp.resize(scanline_bytes as usize, 0u8);
                    read_scanline(decoder, &mut tmp)?;

                    let offset = start.saturating_sub(position);
                    let len = (end - start)
                        .min(scanline_bytes * 8 - offset)
                        .min(end - position);
                    if bits_read % 8 == 0 && offset % 8 == 0 && len % 8 == 0 {
                        let o = (offset / 8) as usize;
                        let l = (len / 8) as usize;
                        buf[((bits_read/8) as usize)..][..l].copy_from_slice(&tmp[o..][..l]);
                        bits_read += len;
                    } else {
                        unimplemented!("Target rectangle not aligned on byte boundaries")
                    }
                }

                current_scanline += 1;
                position += scanline_bytes * 8;
                progress_callback(Progress {current: bits_read, total: total_bits});
            }
            Ok(())
        };

        if x + width > dimensions.0 || y + height > dimensions.0
            || width == 0 || height == 0 {
                return Err(ImageError::DimensionError);
            }
        if scanline_bytes > usize::max_value() as u64 {
            return Err(ImageError::InsufficientMemory);
        }

        progress_callback(Progress {current: 0, total: total_bits});
        if x == 0 && width == dimensions.0 {
            let start = x * bits_per_pixel + y * row_bytes * 8;
            let end = (x + width) * bits_per_pixel + (y + height - 1) * row_bytes * 8;
            read_image_range(start, end)?;
        } else {
            for row in y..(y+height) {
                let start = x * bits_per_pixel + row * row_bytes * 8;
                let end = (x + width) * bits_per_pixel + row * row_bytes * 8;
                read_image_range(start, end)?;
            }
        }
    }

    // Seek back to the start
    Ok(seek_scanline(decoder, 0)?)
}

/// Represents the progress of an image operation.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Progress {
    current: u64,
    total: u64,
}

/// The trait that all decoders implement
pub trait ImageDecoder: Sized {
    /// The type of reader produced by `into_reader`.
    type Reader: Read;

    /// Returns a tuple containing the width and height of the image
    fn dimensions(&self) -> (u64, u64);

    /// Returns the color type of the image e.g. RGB(8) (8bit RGB)
    fn colortype(&self) -> ColorType;

    /// Returns a reader that can be used to obtain the bytes of the image. For the best
    /// performance, always try to read at least `scanline_bytes` from the reader at a time. Reading
    /// fewer bytes will cause the reader to perform internal buffering.
    fn into_reader(self) -> ImageResult<Self::Reader>;

    /// Returns the number of bytes in a single row of the image. All decoders will pad image rows
    /// to a byte boundary.
    fn row_bytes(&self) -> u64 {
        (self.dimensions().0 * color::bits_per_pixel(self.colortype()) as u64 + 7) / 8
    }

    /// Returns the total number of bytes in the image.
    fn total_bytes(&self) -> u64 {
        self.dimensions().1 * self.row_bytes()
    }

    /// Returns the minimum number of bytes that can be efficiently read from this decoder. This may
    /// be as few as 1 or as many as `total_bytes()`.
    fn scanline_bytes(&self) -> u64 {
        self.total_bytes()
    }

    /// Returns all the bytes in the image.
    fn read_image(self) -> ImageResult<Vec<u8>> {
        self.read_image_with_progress(|_| {})
    }

    /// Same as `read_image` but periodically calls the provided callback to give updates on loading
    /// progress.
    fn read_image_with_progress<F: Fn(Progress)>(
        self,
        progress_callback: F,
    ) -> ImageResult<Vec<u8>> {
        let total_bytes = self.total_bytes();
        if total_bytes > usize::max_value() as u64 {
            return Err(ImageError::InsufficientMemory);
        }

        let total_bytes = total_bytes as usize;
        let scanline_bytes = self.scanline_bytes() as usize;
        let target_read_size = if scanline_bytes < 4096 {
            (4096 / scanline_bytes) * scanline_bytes
        } else {
            scanline_bytes
        };

        let mut reader = self.into_reader()?;

        let mut bytes_read = 0;
        let mut contents = vec![0; total_bytes];
        while bytes_read < total_bytes {
            let read_size = target_read_size.min(total_bytes - bytes_read);
            reader.read_exact(&mut contents[bytes_read..][..read_size])?;
            bytes_read += read_size;

            progress_callback(Progress {
                current: bytes_read as u64,
                total: total_bytes as u64,
            });
        }

        Ok(contents)
    }
}

/// ImageDecoderExt trait
pub trait ImageDecoderExt: ImageDecoder + Sized {
    /// Read a rectangular section of the image.
    fn read_rect(
        &mut self,
        x: u64,
        y: u64,
        width: u64,
        height: u64,
        buf: &mut [u8],
    ) -> ImageResult<()> {
        self.read_rect_with_progress(x, y, width, height, buf, |_|{})
    }

    /// Read a rectangular section of the image, periodically reporting progress.
    fn read_rect_with_progress<F: Fn(Progress)>(
        &mut self,
        x: u64,
        y: u64,
        width: u64,
        height: u64,
        buf: &mut [u8],
        progress_callback: F,
    ) -> ImageResult<()>;
}

/// AnimationDecoder trait
pub trait AnimationDecoder<'a> {
    /// Consume the decoder producing a series of frames.
    fn into_frames(self) -> Frames<'a>;
}

/// Immutable pixel iterator
pub struct Pixels<'a, I: ?Sized + 'a> {
    image: &'a I,
    x: u32,
    y: u32,
    width: u32,
    height: u32,
}

impl<'a, I: GenericImageView> Iterator for Pixels<'a, I> {
    type Item = (u32, u32, I::Pixel);

    fn next(&mut self) -> Option<(u32, u32, I::Pixel)> {
        if self.x >= self.width {
            self.x = 0;
            self.y += 1;
        }

        if self.y >= self.height {
            None
        } else {
            let pixel = self.image.get_pixel(self.x, self.y);
            let p = (self.x, self.y, pixel);

            self.x += 1;

            Some(p)
        }
    }
}

/// Mutable pixel iterator
///
/// DEPRECATED: It is currently not possible to create a safe iterator for this in Rust. You have to use an iterator over the image buffer instead.
pub struct MutPixels<'a, I: ?Sized + 'a> {
    image: &'a mut I,
    x: u32,
    y: u32,
    width: u32,
    height: u32,
}

impl<'a, I: GenericImage + 'a> Iterator for MutPixels<'a, I>
where
    I::Pixel: 'a,
    <I::Pixel as Pixel>::Subpixel: 'a,
{
    type Item = (u32, u32, &'a mut I::Pixel);

    fn next(&mut self) -> Option<(u32, u32, &'a mut I::Pixel)> {
        if self.x >= self.width {
            self.x = 0;
            self.y += 1;
        }

        if self.y >= self.height {
            None
        } else {
            let tmp = self.image.get_pixel_mut(self.x, self.y);

            // NOTE: This is potentially dangerous. It would require the signature fn next(&'a mut self) to be safe.
            // error: lifetime of `self` is too short to guarantee its contents can be safely reborrowed...
            let ptr = unsafe { mem::transmute(tmp) };

            let p = (self.x, self.y, ptr);

            self.x += 1;

            Some(p)
        }
    }
}

/// Trait to inspect an image.
pub trait GenericImageView {
    /// The type of pixel.
    type Pixel: Pixel;

    /// Underlying image type. This is mainly used by SubImages in order to
    /// always have a reference to the original image. This allows for less
    /// indirections and it eases the use of nested SubImages.
    type InnerImageView: GenericImageView<Pixel = Self::Pixel>;

    /// The width and height of this image.
    fn dimensions(&self) -> (u32, u32);

    /// The width of this image.
    fn width(&self) -> u32 {
        let (w, _) = self.dimensions();
        w
    }

    /// The height of this image.
    fn height(&self) -> u32 {
        let (_, h) = self.dimensions();
        h
    }

    /// The bounding rectangle of this image.
    fn bounds(&self) -> (u32, u32, u32, u32);

    /// Returns true if this x, y coordinate is contained inside the image.
    fn in_bounds(&self, x: u32, y: u32) -> bool {
        let (ix, iy, iw, ih) = self.bounds();
        x >= ix && x < ix + iw && y >= iy && y < iy + ih
    }

    /// Returns the pixel located at (x, y)
    ///
    /// # Panics
    ///
    /// Panics if `(x, y)` is out of bounds.
    ///
    /// TODO: change this signature to &P
    fn get_pixel(&self, x: u32, y: u32) -> Self::Pixel;

    /// Returns the pixel located at (x, y)
    ///
    /// This function can be implemented in a way that ignores bounds checking.
    unsafe fn unsafe_get_pixel(&self, x: u32, y: u32) -> Self::Pixel {
        self.get_pixel(x, y)
    }

    /// Returns an Iterator over the pixels of this image.
    /// The iterator yields the coordinates of each pixel
    /// along with their value
    fn pixels(&self) -> Pixels<Self> {
        let (width, height) = self.dimensions();

        Pixels {
            image: self,
            x: 0,
            y: 0,
            width,
            height,
        }
    }

    /// Returns a reference to the underlying image.
    fn inner(&self) -> &Self::InnerImageView;

    /// Returns an subimage that is an immutable view into this image.
    fn view(&self, x: u32, y: u32, width: u32, height: u32) -> SubImage<&Self::InnerImageView> {
        SubImage::new(self.inner(), x, y, width, height)
    }
}

/// A trait for manipulating images.
pub trait GenericImage: GenericImageView {
    /// Underlying image type. This is mainly used by SubImages in order to
    /// always have a reference to the original image. This allows for less
    /// indirections and it eases the use of nested SubImages.
    type InnerImage: GenericImage<Pixel = Self::Pixel>;

    /// Gets a reference to the mutable pixel at location `(x, y)`
    ///
    /// # Panics
    ///
    /// Panics if `(x, y)` is out of bounds.
    fn get_pixel_mut(&mut self, x: u32, y: u32) -> &mut Self::Pixel;

    /// Put a pixel at location (x, y)
    ///
    /// # Panics
    ///
    /// Panics if `(x, y)` is out of bounds.
    fn put_pixel(&mut self, x: u32, y: u32, pixel: Self::Pixel);

    /// Puts a pixel at location (x, y)
    ///
    /// This function can be implemented in a way that ignores bounds checking.
    unsafe fn unsafe_put_pixel(&mut self, x: u32, y: u32, pixel: Self::Pixel) {
        self.put_pixel(x, y, pixel);
    }

    /// Put a pixel at location (x, y), taking into account alpha channels
    ///
    /// DEPRECATED: This method will be removed. Blend the pixel directly instead.
    fn blend_pixel(&mut self, x: u32, y: u32, pixel: Self::Pixel);

    /// Returns an Iterator over mutable pixels of this image.
    /// The iterator yields the coordinates of each pixel
    /// along with a mutable reference to them.
    #[deprecated(
        note = "This cannot be implemented safely in Rust. Please use the image buffer directly."
    )]
    fn pixels_mut(&mut self) -> MutPixels<Self> {
        let (width, height) = self.dimensions();

        MutPixels {
            image: self,
            x: 0,
            y: 0,
            width,
            height,
        }
    }

    /// Copies all of the pixels from another image into this image.
    ///
    /// The other image is copied with the top-left corner of the
    /// other image placed at (x, y).
    ///
    /// In order to copy only a piece of the other image, use `sub_image`.
    ///
    /// # Returns
    /// `true` if the copy was successful, `false` if the image could not
    /// be copied due to size constraints.
    fn copy_from<O>(&mut self, other: &O, x: u32, y: u32) -> bool
    where
        O: GenericImageView<Pixel = Self::Pixel>,
    {
        // Do bounds checking here so we can use the non-bounds-checking
        // functions to copy pixels.
        if self.width() < other.width() + x || self.height() < other.height() + y {
            return false;
        }

        for i in 0..other.width() {
            for k in 0..other.height() {
                unsafe {
                    let p = other.unsafe_get_pixel(i, k);
                    self.unsafe_put_pixel(i + x, k + y, p);
                }
            }
        }
        true
    }

    /// Returns a mutable reference to the underlying image.
    fn inner_mut(&mut self) -> &mut Self::InnerImage;

    /// Returns a subimage that is a view into this image.
    fn sub_image(
        &mut self,
        x: u32,
        y: u32,
        width: u32,
        height: u32,
    ) -> SubImage<&mut Self::InnerImage> {
        SubImage::new(self.inner_mut(), x, y, width, height)
    }
}

/// A View into another image
pub struct SubImage<I> {
    image: I,
    xoffset: u32,
    yoffset: u32,
    xstride: u32,
    ystride: u32,
}

/// Alias to access Pixel behind a reference
type DerefPixel<I> = <<I as Deref>::Target as GenericImageView>::Pixel;

/// Alias to access Subpixel behind a reference
type DerefSubpixel<I> = <DerefPixel<I> as Pixel>::Subpixel;

impl<I> SubImage<I> {
    /// Construct a new subimage
    pub fn new(image: I, x: u32, y: u32, width: u32, height: u32) -> SubImage<I> {
        SubImage {
            image,
            xoffset: x,
            yoffset: y,
            xstride: width,
            ystride: height,
        }
    }

    /// Change the coordinates of this subimage.
    pub fn change_bounds(&mut self, x: u32, y: u32, width: u32, height: u32) {
        self.xoffset = x;
        self.yoffset = y;
        self.xstride = width;
        self.ystride = height;
    }

    /// Convert this subimage to an ImageBuffer
    pub fn to_image(&self) -> ImageBuffer<DerefPixel<I>, Vec<DerefSubpixel<I>>>
    where
        I: Deref,
        I::Target: GenericImage + 'static,
    {
        let mut out = ImageBuffer::new(self.xstride, self.ystride);
        let borrowed = self.image.deref();

        for y in 0..self.ystride {
            for x in 0..self.xstride {
                let p = borrowed.get_pixel(x + self.xoffset, y + self.yoffset);
                out.put_pixel(x, y, p);
            }
        }

        out
    }
}

#[allow(deprecated)]
impl<I> GenericImageView for SubImage<I>
where
    I: Deref,
    I::Target: GenericImageView + Sized,
{
    type Pixel = DerefPixel<I>;
    type InnerImageView = I::Target;

    fn dimensions(&self) -> (u32, u32) {
        (self.xstride, self.ystride)
    }

    fn bounds(&self) -> (u32, u32, u32, u32) {
        (self.xoffset, self.yoffset, self.xstride, self.ystride)
    }

    fn get_pixel(&self, x: u32, y: u32) -> Self::Pixel {
        self.image.get_pixel(x + self.xoffset, y + self.yoffset)
    }

    fn view(&self, x: u32, y: u32, width: u32, height: u32) -> SubImage<&Self::InnerImageView> {
        let x = self.xoffset + x;
        let y = self.yoffset + y;
        SubImage::new(self.inner(), x, y, width, height)
    }

    fn inner(&self) -> &Self::InnerImageView {
        &self.image
    }
}

#[allow(deprecated)]
impl<I> GenericImage for SubImage<I>
where
    I: DerefMut,
    I::Target: GenericImage + Sized,
{
    type InnerImage = I::Target;

    fn get_pixel_mut(&mut self, x: u32, y: u32) -> &mut Self::Pixel {
        self.image.get_pixel_mut(x + self.xoffset, y + self.yoffset)
    }

    fn put_pixel(&mut self, x: u32, y: u32, pixel: Self::Pixel) {
        self.image
            .put_pixel(x + self.xoffset, y + self.yoffset, pixel)
    }

    /// DEPRECATED: This method will be removed. Blend the pixel directly instead.
    fn blend_pixel(&mut self, x: u32, y: u32, pixel: Self::Pixel) {
        self.image
            .blend_pixel(x + self.xoffset, y + self.yoffset, pixel)
    }

    fn sub_image(
        &mut self,
        x: u32,
        y: u32,
        width: u32,
        height: u32,
    ) -> SubImage<&mut Self::InnerImage> {
        let x = self.xoffset + x;
        let y = self.yoffset + y;
        SubImage::new(self.inner_mut(), x, y, width, height)
    }

    fn inner_mut(&mut self) -> &mut Self::InnerImage {
        &mut self.image
    }
}

#[cfg(test)]
mod tests {

    use super::{GenericImage, GenericImageView};
    use buffer::ImageBuffer;
    use color::Rgba;

    #[test]
    /// Test that alpha blending works as expected
    fn test_image_alpha_blending() {
        let mut target = ImageBuffer::new(1, 1);
        target.put_pixel(0, 0, Rgba([255u8, 0, 0, 255]));
        assert!(*target.get_pixel(0, 0) == Rgba([255, 0, 0, 255]));
        target.blend_pixel(0, 0, Rgba([0, 255, 0, 255]));
        assert!(*target.get_pixel(0, 0) == Rgba([0, 255, 0, 255]));

        // Blending an alpha channel onto a solid background
        target.blend_pixel(0, 0, Rgba([255, 0, 0, 127]));
        assert!(*target.get_pixel(0, 0) == Rgba([127, 127, 0, 255]));

        // Blending two alpha channels
        target.put_pixel(0, 0, Rgba([0, 255, 0, 127]));
        target.blend_pixel(0, 0, Rgba([255, 0, 0, 127]));
        assert!(*target.get_pixel(0, 0) == Rgba([169, 85, 0, 190]));
    }

    #[test]
    fn test_in_bounds() {
        let mut target = ImageBuffer::new(2, 2);
        target.put_pixel(0, 0, Rgba([255u8, 0, 0, 255]));

        assert!(target.in_bounds(0, 0));
        assert!(target.in_bounds(1, 0));
        assert!(target.in_bounds(0, 1));
        assert!(target.in_bounds(1, 1));

        assert!(!target.in_bounds(2, 0));
        assert!(!target.in_bounds(0, 2));
        assert!(!target.in_bounds(2, 2));
    }

    #[test]
    fn test_can_subimage_clone_nonmut() {
        let mut source = ImageBuffer::new(3, 3);
        source.put_pixel(1, 1, Rgba([255u8, 0, 0, 255]));

        // A non-mutable copy of the source image
        let source = source.clone();

        // Clone a view into non-mutable to a separate buffer
        let cloned = source.view(1, 1, 1, 1).to_image();

        assert!(cloned.get_pixel(0, 0) == source.get_pixel(1, 1));
    }

    #[test]
    fn test_can_nest_views() {
        let mut source = ImageBuffer::from_pixel(3, 3, Rgba([255u8, 0, 0, 255]));

        {
            let mut sub1 = source.sub_image(0, 0, 2, 2);
            let mut sub2 = sub1.sub_image(1, 1, 1, 1);
            sub2.put_pixel(0, 0, Rgba([0, 0, 0, 0]));
        }

        assert_eq!(*source.get_pixel(1, 1), Rgba([0, 0, 0, 0]));

        let view1 = source.view(0, 0, 2, 2);
        assert_eq!(*source.get_pixel(1, 1), view1.get_pixel(1, 1));

        let view2 = view1.view(1, 1, 1, 1);
        assert_eq!(*source.get_pixel(1, 1), view2.get_pixel(0, 0));
    }

    #[test]
    fn test_load_rect() {
        use super::*;

        struct MockDecoder {scanline_number: u64, scanline_bytes: u64}
        impl ImageDecoder for MockDecoder {
            type Reader = Box<::std::io::Read>;
            fn dimensions(&self) -> (u64, u64) {(5, 5)}
            fn colortype(&self) -> ColorType {  ColorType::Gray(8) }
            fn into_reader(self) -> ImageResult<Self::Reader> {unimplemented!()}
            fn scanline_bytes(&self) -> u64 { self.scanline_bytes }
        }

        const DATA: [u8; 25] = [0,  1,  2,  3,  4,
                                5,  6,  7,  8,  9,
                                10, 11, 12, 13, 14,
                                15, 16, 17, 18, 19,
                                20, 21, 22, 23, 24];

        fn seek_scanline(m: &mut MockDecoder, n: u64) -> io::Result<()> {
            m.scanline_number = n;
            Ok(())
        }
        fn read_scanline(m: &mut MockDecoder, buf: &mut [u8]) -> io::Result<usize> {
            let bytes_read = m.scanline_number * m.scanline_bytes;
            if bytes_read >= 25 { return Ok(0); }

            let len = m.scanline_bytes.min(25 - bytes_read);
            buf[..(len as usize)].copy_from_slice(&DATA[(bytes_read as usize)..][..(len as usize)]);
            m.scanline_number += 1;
            Ok(len as usize)
        }

        for scanline_bytes in 1..30 {
            let mut output = [0u8; 26];

            load_rect(0, 0, 5, 5, &mut output, |_|{},
                      &mut MockDecoder{scanline_number:0, scanline_bytes},
                      seek_scanline, read_scanline).unwrap();
            assert_eq!(output[0..25], DATA);
            assert_eq!(output[25], 0);

            output = [0u8; 26];
            load_rect(3, 2, 1, 1, &mut output, |_|{},
                      &mut MockDecoder{scanline_number:0, scanline_bytes},
                      seek_scanline, read_scanline).unwrap();
            assert_eq!(output[0..2], [13, 0]);

            output = [0u8; 26];
            load_rect(3, 2, 2, 2, &mut output, |_|{},
                      &mut MockDecoder{scanline_number:0, scanline_bytes},
                      seek_scanline, read_scanline).unwrap();
            assert_eq!(output[0..5], [13, 14, 18, 19, 0]);


            output = [0u8; 26];
            load_rect(1, 1, 2, 4, &mut output, |_|{},
                      &mut MockDecoder{scanline_number:0, scanline_bytes},
                      seek_scanline, read_scanline).unwrap();
            assert_eq!(output[0..9], [6, 7, 11, 12, 16, 17, 21, 22, 0]);

        }
    }
}
