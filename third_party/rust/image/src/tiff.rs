//! Decoding and Encoding of TIFF Images
//!
//! TIFF (Tagged Image File Format) is a versatile image format that supports
//! lossless and lossy compression.
//!
//! # Related Links
//! * <http://partners.adobe.com/public/developer/tiff/index.html> - The TIFF specification

extern crate tiff;

use std::io::{Cursor, Read, Seek};

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

impl<R: Read + Seek> ImageDecoder for TIFFDecoder<R> {
    type Reader = Cursor<Vec<u8>>;

    fn dimensions(&self) -> (u64, u64) {
        (self.dimensions.0 as u64, self.dimensions.1 as u64)
    }

    fn colortype(&self) -> ColorType {
        self.colortype
    }

    fn into_reader(self) -> ImageResult<Self::Reader> {
        Ok(Cursor::new(self.read_image()?))
    }

    fn read_image(mut self) -> ImageResult<Vec<u8>> {
        match self.inner.read_image()? {
            tiff::decoder::DecodingResult::U8(v) => Ok(v),
            tiff::decoder::DecodingResult::U16(v) => Ok(vec_u16_into_u8(v)),
        }
    }
}
