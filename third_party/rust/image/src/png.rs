//! Decoding and Encoding of PNG Images
//!
//! PNG (Portable Network Graphics) is an image format that supports lossless compression.
//!
//! # Related Links
//! * <http://www.w3.org/TR/PNG/> - The PNG Specification
//!

extern crate png;

use self::png::HasParameters;

use std::io::{self, Cursor, Read, Write};

use color::ColorType;
use image::{ImageDecoder, ImageError, ImageResult};

/// PNG decoder
pub struct PNGDecoder<R: Read> {
    colortype: ColorType,
    reader: png::Reader<R>,
}

impl<R: Read> PNGDecoder<R> {
    /// Creates a new decoder that decodes from the stream ```r```
    pub fn new(r: R) -> ImageResult<PNGDecoder<R>> {
        let decoder = png::Decoder::new(r);
        let (_, mut reader) = decoder.read_info()?;
        let colortype = reader.output_color_type().into();

        Ok(PNGDecoder { colortype, reader })
    }
}

impl<R: Read> ImageDecoder for PNGDecoder<R> {
    type Reader = Cursor<Vec<u8>>;

    fn dimensions(&self) -> (u64, u64) {
        let (w, h) = self.reader.info().size();
        (w as u64, h as u64)
    }

    fn colortype(&self) -> ColorType {
        self.colortype
    }

    fn into_reader(self) -> ImageResult<Self::Reader> {
        Ok(Cursor::new(self.read_image()?))
    }

    fn read_image(mut self) -> ImageResult<Vec<u8>> {
        let mut data = vec![0; self.reader.output_buffer_size()];
        self.reader.next_frame(&mut data)?;
        Ok(data)
    }
}

/// PNG encoder
pub struct PNGEncoder<W: Write> {
    w: W,
}

impl<W: Write> PNGEncoder<W> {
    /// Create a new encoder that writes its output to ```w```
    pub fn new(w: W) -> PNGEncoder<W> {
        PNGEncoder { w }
    }

    /// Encodes the image ```image```
    /// that has dimensions ```width``` and ```height```
    /// and ```ColorType``` ```c```
    pub fn encode(self, data: &[u8], width: u32, height: u32, color: ColorType) -> io::Result<()> {
        let (ct, bits) = color.into();
        let mut encoder = png::Encoder::new(self.w, width, height);
        encoder.set(ct).set(bits);
        let mut writer = try!(encoder.write_header());
        writer.write_image_data(data).map_err(|e| e.into())
    }
}

impl From<(png::ColorType, png::BitDepth)> for ColorType {
    fn from((ct, bits): (png::ColorType, png::BitDepth)) -> ColorType {
        use self::png::ColorType::*;
        let bits = bits as u8;
        match ct {
            Grayscale => ColorType::Gray(bits),
            RGB => ColorType::RGB(bits),
            Indexed => ColorType::Palette(bits),
            GrayscaleAlpha => ColorType::GrayA(bits),
            RGBA => ColorType::RGBA(bits),
        }
    }
}

impl From<ColorType> for (png::ColorType, png::BitDepth) {
    fn from(ct: ColorType) -> (png::ColorType, png::BitDepth) {
        use self::png::ColorType::*;
        let (ct, bits) = match ct {
            ColorType::Gray(bits) => (Grayscale, bits),
            ColorType::RGB(bits) => (RGB, bits),
            ColorType::Palette(bits) => (Indexed, bits),
            ColorType::GrayA(bits) => (GrayscaleAlpha, bits),
            ColorType::RGBA(bits) => (RGBA, bits),
            ColorType::BGRA(bits) => (RGBA, bits),
            ColorType::BGR(bits) => (RGB, bits),
        };
        (ct, png::BitDepth::from_u8(bits).unwrap())
    }
}

impl From<png::DecodingError> for ImageError {
    fn from(err: png::DecodingError) -> ImageError {
        use self::png::DecodingError::*;
        match err {
            IoError(err) => ImageError::IoError(err),
            Format(desc) => ImageError::FormatError(desc.into_owned()),
            InvalidSignature => ImageError::FormatError("invalid signature".into()),
            CrcMismatch { .. } => ImageError::FormatError("CRC error".into()),
            Other(desc) => ImageError::FormatError(desc.into_owned()),
            CorruptFlateStream => {
                ImageError::FormatError("compressed data stream corrupted".into())
            }
        }
    }
}
