//! Decoding and Encoding of TIFF Images
//!
//! TIFF (Tagged Image File Format) is a versatile image format that supports
//! lossless and lossy compression.
//!
//! # Related Links
//! * <http://partners.adobe.com/public/developer/tiff/index.html> - The TIFF specification

extern crate tiff;

use std::io::{self, Cursor, Read, Write, Seek};
use std::marker::PhantomData;
use std::mem;

use color::ColorType;
use image::{ImageDecoder, ImageResult, ImageError};
use utils::vec_u16_into_u8;

/// Decoder for TIFF images.
pub struct TIFFDecoder<R>
    where R: Read + Seek
{
    dimensions: (u32, u32),
    colortype: ColorType,
    inner: tiff::decoder::Decoder<R>,
}

impl<R> TIFFDecoder<R>
    where R: Read + Seek
{
    /// Create a new TIFFDecoder.
    pub fn new(r: R) -> Result<TIFFDecoder<R>, ImageError> {
        let mut inner = tiff::decoder::Decoder::new(r)?;
        let dimensions = inner.dimensions()?;
        let colortype = inner.colortype()?.into();

        Ok(TIFFDecoder {
            dimensions,
            colortype,
            inner,
        })
    }
}

impl From<tiff::TiffError> for ImageError {
    fn from(err: tiff::TiffError) -> ImageError {
        match err {
            tiff::TiffError::IoError(err) => ImageError::IoError(err),
            tiff::TiffError::FormatError(desc) => ImageError::FormatError(desc.to_string()),
            tiff::TiffError::UnsupportedError(desc) => ImageError::UnsupportedError(desc.to_string()),
            tiff::TiffError::LimitsExceeded => ImageError::InsufficientMemory,
        }
    }
}

impl From<tiff::ColorType> for ColorType {
    fn from(ct: tiff::ColorType) -> ColorType {
        match ct {
            tiff::ColorType::Gray(depth) => ColorType::Gray(depth),
            tiff::ColorType::RGB(depth) => ColorType::RGB(depth),
            tiff::ColorType::Palette(depth) => ColorType::Palette(depth),
            tiff::ColorType::GrayA(depth) => ColorType::GrayA(depth),
            tiff::ColorType::RGBA(depth) => ColorType::RGBA(depth),
            tiff::ColorType::CMYK(_) => unimplemented!()
        }
    }
}

/// Wrapper struct around a `Cursor<Vec<u8>>`
pub struct TiffReader<R>(Cursor<Vec<u8>>, PhantomData<R>);
impl<R> Read for TiffReader<R> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        self.0.read(buf)
    }
    fn read_to_end(&mut self, buf: &mut Vec<u8>) -> io::Result<usize> {
        if self.0.position() == 0 && buf.is_empty() {
            mem::swap(buf, self.0.get_mut());
            Ok(buf.len())
        } else {
            self.0.read_to_end(buf)
        }
    }
}

impl<'a, R: 'a + Read + Seek> ImageDecoder<'a> for TIFFDecoder<R> {
    type Reader = TiffReader<R>;

    fn dimensions(&self) -> (u64, u64) {
        (self.dimensions.0 as u64, self.dimensions.1 as u64)
    }

    fn colortype(&self) -> ColorType {
        self.colortype
    }

    fn into_reader(self) -> ImageResult<Self::Reader> {
        Ok(TiffReader(Cursor::new(self.read_image()?), PhantomData))
    }

    fn read_image(mut self) -> ImageResult<Vec<u8>> {
        match self.inner.read_image()? {
            tiff::decoder::DecodingResult::U8(v) => Ok(v),
            tiff::decoder::DecodingResult::U16(v) => Ok(vec_u16_into_u8(v)),
        }
    }
}

/// Encoder for tiff images
pub struct TiffEncoder<W> {
    w: W,
}

impl<W: Write + Seek> TiffEncoder<W> {
    /// Create a new encoder that writes its output to `w`
    pub fn new(w: W) -> TiffEncoder<W> {
        TiffEncoder { w }
    }

    /// Encodes the image `image`
    /// that has dimensions `width` and `height`
    /// and `ColorType` `c`.
    ///
    /// 16-bit colortypes are not yet supported.
    pub fn encode(self, data: &[u8], width: u32, height: u32, color: ColorType) -> ImageResult<()> {
        // TODO: 16bit support
        let mut encoder = tiff::encoder::TiffEncoder::new(self.w)?;
        match color {
            ColorType::Gray(8) => encoder.write_image::<tiff::encoder::colortype::Gray8>(width, height, data)?,
            ColorType::RGB(8) => encoder.write_image::<tiff::encoder::colortype::RGB8>(width, height, data)?,
            ColorType::RGBA(8) => encoder.write_image::<tiff::encoder::colortype::RGBA8>(width, height, data)?,
            _ => return Err(ImageError::UnsupportedColor(color))
        }

        Ok(())
    }
}
