use std::convert::TryFrom;
use std::io::{self, Cursor, Read};
use std::marker::PhantomData;
use std::mem;

use crate::color::ColorType;
use crate::image::ImageDecoder;
use crate::error::{ImageError, ImageResult};

/// JPEG decoder
pub struct JpegDecoder<R> {
    decoder: jpeg::Decoder<R>,
    metadata: jpeg::ImageInfo,
}

impl<R: Read> JpegDecoder<R> {
    /// Create a new decoder that decodes from the stream ```r```
    pub fn new(r: R) -> ImageResult<JpegDecoder<R>> {
        let mut decoder = jpeg::Decoder::new(r);

        decoder.read_info().map_err(ImageError::from_jpeg)?;
        let mut metadata = decoder.info().unwrap();

        // We convert CMYK data to RGB before returning it to the user.
        if metadata.pixel_format == jpeg::PixelFormat::CMYK32 {
            metadata.pixel_format = jpeg::PixelFormat::RGB24;
        }

        Ok(JpegDecoder {
            decoder,
            metadata,
        })
    }
}

/// Wrapper struct around a `Cursor<Vec<u8>>`
pub struct JpegReader<R>(Cursor<Vec<u8>>, PhantomData<R>);
impl<R> Read for JpegReader<R> {
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

impl<'a, R: 'a + Read> ImageDecoder<'a> for JpegDecoder<R> {
    type Reader = JpegReader<R>;

    fn dimensions(&self) -> (u32, u32) {
        (u32::from(self.metadata.width), u32::from(self.metadata.height))
    }

    fn color_type(&self) -> ColorType {
        ColorType::from_jpeg(self.metadata.pixel_format)
    }

    fn into_reader(mut self) -> ImageResult<Self::Reader> {
        let mut data = self.decoder.decode().map_err(ImageError::from_jpeg)?;
        data = match self.decoder.info().unwrap().pixel_format {
            jpeg::PixelFormat::CMYK32 => cmyk_to_rgb(&data),
            _ => data,
        };

        Ok(JpegReader(Cursor::new(data), PhantomData))
    }

    fn read_image(mut self, buf: &mut [u8]) -> ImageResult<()> {
        assert_eq!(u64::try_from(buf.len()), Ok(self.total_bytes()));

        let mut data = self.decoder.decode().map_err(ImageError::from_jpeg)?;
        data = match self.decoder.info().unwrap().pixel_format {
            jpeg::PixelFormat::CMYK32 => cmyk_to_rgb(&data),
            _ => data,
        };

        buf.copy_from_slice(&data);
        Ok(())
    }
}

fn cmyk_to_rgb(input: &[u8]) -> Vec<u8> {
    let size = input.len() - input.len() / 4;
    let mut output = Vec::with_capacity(size);

    for pixel in input.chunks(4) {
        let c = f32::from(pixel[0]) / 255.0;
        let m = f32::from(pixel[1]) / 255.0;
        let y = f32::from(pixel[2]) / 255.0;
        let k = f32::from(pixel[3]) / 255.0;

        // CMYK -> CMY
        let c = c * (1.0 - k) + k;
        let m = m * (1.0 - k) + k;
        let y = y * (1.0 - k) + k;

        // CMY -> RGB
        let r = (1.0 - c) * 255.0;
        let g = (1.0 - m) * 255.0;
        let b = (1.0 - y) * 255.0;

        output.push(r as u8);
        output.push(g as u8);
        output.push(b as u8);
    }

    output
}

impl ColorType {
    fn from_jpeg(pixel_format: jpeg::PixelFormat) -> ColorType {
        use jpeg::PixelFormat::*;
        match pixel_format {
            L8 => ColorType::L8,
            RGB24 => ColorType::Rgb8,
            CMYK32 => panic!(),
        }
    }
}

impl ImageError {
    fn from_jpeg(err: jpeg::Error) -> ImageError {
        use jpeg::Error::*;
        match err {
            Format(desc) => ImageError::FormatError(desc),
            Unsupported(desc) => ImageError::UnsupportedError(format!("{:?}", desc)),
            Io(err) => ImageError::IoError(err),
            Internal(err) => ImageError::FormatError(err.to_string()),
        }
    }
}
