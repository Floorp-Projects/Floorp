use byteorder::{LittleEndian, ReadBytesExt};
use std::default::Default;
use std::io;
use std::io::{Cursor, Read};

use image;
use image::ImageDecoder;
use image::ImageResult;

use color;

use super::vp8::Frame;
use super::vp8::VP8Decoder;

/// A Representation of a Webp Image format decoder.
pub struct WebpDecoder<R> {
    r: R,
    frame: Frame,
    have_frame: bool,
}

impl<R: Read> WebpDecoder<R> {
    /// Create a new WebpDecoder from the Reader ```r```.
    /// This function takes ownership of the Reader.
    pub fn new(r: R) -> ImageResult<WebpDecoder<R>> {
        let f: Frame = Default::default();

        let mut decoder = WebpDecoder {
            r,
            have_frame: false,
            frame: f,
        };
        decoder.read_metadata()?;
        Ok(decoder)
    }

    fn read_riff_header(&mut self) -> ImageResult<u32> {
        let mut riff = Vec::with_capacity(4);
        try!(self.r.by_ref().take(4).read_to_end(&mut riff));
        let size = try!(self.r.read_u32::<LittleEndian>());
        let mut webp = Vec::with_capacity(4);
        try!(self.r.by_ref().take(4).read_to_end(&mut webp));

        if &*riff != b"RIFF" {
            return Err(image::ImageError::FormatError(
                "Invalid RIFF signature.".to_string(),
            ));
        }

        if &*webp != b"WEBP" {
            return Err(image::ImageError::FormatError(
                "Invalid WEBP signature.".to_string(),
            ));
        }

        Ok(size)
    }

    fn read_vp8_header(&mut self) -> ImageResult<()> {
        let mut vp8 = Vec::with_capacity(4);
        try!(self.r.by_ref().take(4).read_to_end(&mut vp8));

        if &*vp8 != b"VP8 " {
            return Err(image::ImageError::FormatError(
                "Invalid VP8 signature.".to_string(),
            ));
        }

        let _len = try!(self.r.read_u32::<LittleEndian>());

        Ok(())
    }

    fn read_frame(&mut self) -> ImageResult<()> {
        let mut framedata = Vec::new();
        try!(self.r.read_to_end(&mut framedata));
        let m = io::Cursor::new(framedata);

        let mut v = VP8Decoder::new(m);
        let frame = try!(v.decode_frame());

        self.frame = frame.clone();

        Ok(())
    }

    fn read_metadata(&mut self) -> ImageResult<()> {
        if !self.have_frame {
            try!(self.read_riff_header());
            try!(self.read_vp8_header());
            try!(self.read_frame());

            self.have_frame = true;
        }

        Ok(())
    }
}

impl<R: Read> ImageDecoder for WebpDecoder<R> {
    type Reader = Cursor<Vec<u8>>;

    fn dimensions(&self) -> (u64, u64) {
        (self.frame.width as u64, self.frame.height as u64)
    }

    fn colortype(&self) -> color::ColorType {
        color::ColorType::Gray(8)
    }

    fn into_reader(self) -> ImageResult<Self::Reader> {
        Ok(Cursor::new(self.frame.ybuf))
    }

    fn read_image(self) -> ImageResult<Vec<u8>> {
        Ok(self.frame.ybuf)
    }
}
