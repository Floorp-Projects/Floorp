use byteorder::{LittleEndian, ReadBytesExt};
use std::default::Default;
use std::io::{self, Cursor, Read};
use std::marker::PhantomData;
use std::mem;

use image;
use image::ImageDecoder;
use image::ImageResult;

use color;

use super::vp8::Frame;
use super::vp8::VP8Decoder;

/// Webp Image format decoder. Currently only supportes the luma channel (meaning that decoded
/// images will be grayscale).
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
        self.r.by_ref().take(4).read_to_end(&mut riff)?;
        let size = self.r.read_u32::<LittleEndian>()?;
        let mut webp = Vec::with_capacity(4);
        self.r.by_ref().take(4).read_to_end(&mut webp)?;

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
        self.r.by_ref().take(4).read_to_end(&mut vp8)?;

        if &*vp8 != b"VP8 " {
            return Err(image::ImageError::FormatError(
                "Invalid VP8 signature.".to_string(),
            ));
        }

        let _len = self.r.read_u32::<LittleEndian>()?;

        Ok(())
    }

    fn read_frame(&mut self) -> ImageResult<()> {
        let mut framedata = Vec::new();
        self.r.read_to_end(&mut framedata)?;
        let m = io::Cursor::new(framedata);

        let mut v = VP8Decoder::new(m);
        let frame = v.decode_frame()?;

        self.frame = frame.clone();

        Ok(())
    }

    fn read_metadata(&mut self) -> ImageResult<()> {
        if !self.have_frame {
            self.read_riff_header()?;
            self.read_vp8_header()?;
            self.read_frame()?;

            self.have_frame = true;
        }

        Ok(())
    }
}

/// Wrapper struct around a `Cursor<Vec<u8>>`
pub struct WebpReader<R>(Cursor<Vec<u8>>, PhantomData<R>);
impl<R> Read for WebpReader<R> {
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

impl<'a, R: 'a + Read> ImageDecoder<'a> for WebpDecoder<R> {
    type Reader = WebpReader<R>;

    fn dimensions(&self) -> (u64, u64) {
        (self.frame.width as u64, self.frame.height as u64)
    }

    fn colortype(&self) -> color::ColorType {
        color::ColorType::Gray(8)
    }

    fn into_reader(self) -> ImageResult<Self::Reader> {
        Ok(WebpReader(Cursor::new(self.frame.ybuf), PhantomData))
    }

    fn read_image(self) -> ImageResult<Vec<u8>> {
        Ok(self.frame.ybuf)
    }
}
