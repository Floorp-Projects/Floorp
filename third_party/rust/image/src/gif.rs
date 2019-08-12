//!  Decoding of GIF Images
//!
//!  GIF (Graphics Interchange Format) is an image format that supports lossless compression.
//!
//!  # Related Links
//!  * <http://www.w3.org/Graphics/GIF/spec-gif89a.txt> - The GIF Specification
//!
//! # Examples
//! ```rust,no_run
//! use image::gif::{Decoder, Encoder};
//! use image::{ImageDecoder, AnimationDecoder};
//! use std::fs::File;
//! # fn main() -> std::io::Result<()> {
//! // Decode a gif into frames
//! let file_in = File::open("foo.gif")?;
//! let mut decoder = Decoder::new(file_in).unwrap();
//! let frames = decoder.into_frames();
//! let frames = frames.collect_frames().expect("error decoding gif");
//!
//! // Encode frames into a gif and save to a file
//! let mut file_out = File::open("out.gif")?;
//! let mut encoder = Encoder::new(file_out);
//! encoder.encode_frames(frames.into_iter());
//! # Ok(())
//! # }
//! ```
#![cfg_attr(feature = "cargo-clippy", allow(while_let_loop))]

extern crate gif;
extern crate num_rational;

use std::clone::Clone;
use std::io::{self, Cursor, Read, Write};
use std::marker::PhantomData;
use std::mem;

use self::gif::{ColorOutput, SetParameter};
pub use self::gif::{DisposalMethod, Frame};

use animation;
use buffer::{ImageBuffer, Pixel};
use color;
use color::Rgba;
use image::{AnimationDecoder, ImageDecoder, ImageError, ImageResult};
use num_rational::Ratio;

/// GIF decoder
pub struct Decoder<R: Read> {
    reader: gif::Reader<R>,
}

impl<R: Read> Decoder<R> {
    /// Creates a new decoder that decodes the input steam ```r```
    pub fn new(r: R) -> ImageResult<Decoder<R>> {
        let mut decoder = gif::Decoder::new(r);
        decoder.set(ColorOutput::RGBA);

        Ok(Decoder {
            reader: decoder.read_info()?,
        })
    }
}

/// Wrapper struct around a `Cursor<Vec<u8>>`
pub struct GifReader<R>(Cursor<Vec<u8>>, PhantomData<R>);
impl<R> Read for GifReader<R> {
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

impl<'a, R: 'a + Read> ImageDecoder<'a> for Decoder<R> {
    type Reader = GifReader<R>;

    fn dimensions(&self) -> (u64, u64) {
        (self.reader.width() as u64, self.reader.height() as u64)
    }

    fn colortype(&self) -> color::ColorType {
        color::ColorType::RGBA(8)
    }

    fn into_reader(self) -> ImageResult<Self::Reader> {
        Ok(GifReader(Cursor::new(self.read_image()?), PhantomData))
    }

    fn read_image(mut self) -> ImageResult<Vec<u8>> {
        if self.reader.next_frame_info()?.is_some() {
            let mut buf = vec![0; self.reader.buffer_size()];
            self.reader.read_into_buffer(&mut buf)?;
            Ok(buf)
        } else {
            Err(ImageError::ImageEnd)
        }
    }
}

struct GifFrameIterator<R: Read> {
    reader: gif::Reader<R>,

    width: u32,
    height: u32,

    background_img: ImageBuffer<Rgba<u8>, Vec<u8>>,
    non_disposed_frame: ImageBuffer<Rgba<u8>, Vec<u8>>,

    left: u32,
    top: u32,
    delay: Ratio<u16>,
    dispose: DisposalMethod,
}


impl<R: Read> GifFrameIterator<R> {
    fn new(decoder: Decoder<R>) -> GifFrameIterator<R> {
        let (width, height) = decoder.dimensions();

        // TODO: Avoid this cast
        let (width, height) = (width as u32, height as u32);

        // set the background color to be either the bg_color defined in the gif
        // or a transparent pixel.
        let background_color_option = decoder.reader.bg_color();
        let mut background_color = vec![0; 4];
        let background_pixel = {
            let global_palette = decoder.reader.global_palette();
            match background_color_option {
                Some(index) => {
                    // find the color by looking in the global palette
                    match global_palette {
                        // take the color from the palette that is at the index defined
                        // by background_color_option
                        Some(slice) => {
                            background_color.clone_from_slice(&slice[index..(index + 4)]);
                            Rgba::from_slice(&background_color)
                        }
                        // if there is no global palette, assign the background color to be
                        // transparent
                        None => Rgba::from_slice(&[0, 0, 0, 0]),
                    }
                }
                // return a transparent background color
                None => Rgba::from_slice(&[0, 0, 0, 0]),
            }
        };

        // create the background image to use later
        let background_img = ImageBuffer::from_pixel(width, height, *background_pixel);

        // the background image is the first non disposed frame
        let non_disposed_frame = background_img.clone();

        GifFrameIterator {
            reader: decoder.reader,
            width,
            height,
            background_img,
            non_disposed_frame,
            left: 0,
            top: 0,
            delay: Ratio::new(0, 1),
            dispose: DisposalMethod::Any
        }
    }
}


impl<R: Read> Iterator for GifFrameIterator<R> {
    type Item = ImageResult<animation::Frame>;

    fn next(&mut self) -> Option<ImageResult<animation::Frame>> {
        // begin looping over each frame
        match self.reader.next_frame_info() {
            Ok(frame_info) => {
                if let Some(frame) = frame_info {
                    self.left = u32::from(frame.left);
                    self.top = u32::from(frame.top);

                    // frame.delay is in units of 10ms so frame.delay*10 is in ms
                    self.delay = Ratio::new(frame.delay * 10, 1);
                    self.dispose = frame.dispose;
                } else {
                    // no more frames
                    return None;
                }
            },
            Err(err) => return Some(Err(err.into())),
        }

        let mut vec = vec![0; self.reader.buffer_size()];
        if let Err(err) = self.reader.fill_buffer(&mut vec) {
            return Some(Err(err.into()));
        }

        // create the image buffer from the raw frame
        let mut image_buffer = match ImageBuffer::from_raw(self.width, self.height, vec) {
            Some(buffer) => buffer,
            None => return Some(Err(ImageError::UnsupportedError(
                "Unknown error occured while reading gif frame".into()
            ))),
        };

        // loop over all pixels, checking if any pixels from the non disposed
        // frame need to be used
        for (x, y, pixel) in image_buffer.enumerate_pixels_mut() {
            let previous_img_buffer = &self.non_disposed_frame;
            let adjusted_pixel: &mut Rgba<u8> = pixel;
            let previous_pixel: &Rgba<u8> = previous_img_buffer.get_pixel(x, y);

            let pixel_alpha = adjusted_pixel.channels()[3];

            // If a pixel is not visible then we show the non disposed frame pixel instead
            if pixel_alpha == 0 {
                adjusted_pixel.blend(previous_pixel);
            }
        }

        let frame = animation::Frame::from_parts(
            image_buffer.clone(), self.left, self.top, self.delay
        );

        match self.dispose {
            DisposalMethod::Any => {
                // do nothing
                // (completely replace this frame with the next)
            }
            DisposalMethod::Keep => {
                // do not dispose
                // (keep pixels from this frame)
                self.non_disposed_frame = image_buffer;
            }
            DisposalMethod::Background => {
                // restore to background color
                // (background shows through transparent pixels in the next frame)
                self.non_disposed_frame = self.background_img.clone();
            }
            DisposalMethod::Previous => {
                // restore to previous
                // (dispose frames leaving the last none disposal frame)
            }
        };

        Some(Ok(frame))
    }
}


impl<'a, R: Read + 'a> AnimationDecoder<'a> for Decoder<R> {
    fn into_frames(self) -> animation::Frames<'a> {
        animation::Frames::new(Box::new(GifFrameIterator::new(self)))
    }
}

/// GIF encoder.
pub struct Encoder<W: Write> {
    w: Option<W>,
    gif_encoder: Option<gif::Encoder<W>>,
}

impl<W: Write> Encoder<W> {
    /// Creates a new GIF encoder.
    pub fn new(w: W) -> Encoder<W> {
        Encoder {
            w: Some(w),
            gif_encoder: None,
        }
    }
    /// Encodes a frame.
    pub fn encode(&mut self, frame: &Frame) -> ImageResult<()> {
        let result;
        if let Some(ref mut encoder) = self.gif_encoder {
            result = encoder.write_frame(frame).map_err(|err| err.into());
        } else {
            let writer = self.w.take().unwrap();
            let mut encoder = gif::Encoder::new(writer, frame.width, frame.height, &[])?;
            result = encoder.write_frame(&frame).map_err(|err| err.into());
            self.gif_encoder = Some(encoder);
        }
        result
    }

    /// Encodes Frames.
    /// Consider using `try_encode_frames` instead to encode an `animation::Frames` like iterator.
    pub fn encode_frames<F>(&mut self, frames: F) -> ImageResult<()>
    where 
        F: IntoIterator<Item = animation::Frame>
    {
        for img_frame in frames {
            self.encode_single_frame(img_frame)?;
        }
        Ok(())
    }

    /// Try to encode a collection of `ImageResult<animation::Frame>` objects.
    /// Use this function to encode an `animation::Frames` like iterator.
    /// Whenever an `Err` item is encountered, that value is returned without further actions.
    pub fn try_encode_frames<F>(&mut self, frames: F) -> ImageResult<()>
    where
        F: IntoIterator<Item = ImageResult<animation::Frame>>
    {
        for img_frame in frames {
            self.encode_single_frame(img_frame?)?;
        }
        Ok(())
    }

    fn encode_single_frame(&mut self, img_frame: animation::Frame) -> ImageResult<()> {
        // get the delay before converting img_frame
        let frame_delay = img_frame.delay().to_integer();
        // convert img_frame into RgbaImage
        let rbga_frame = img_frame.into_buffer();

        // Create the gif::Frame from the animation::Frame
        let mut frame = Frame::from_rgba(rbga_frame.width() as u16, rbga_frame.height() as u16, &mut rbga_frame.into_raw());
        frame.delay = frame_delay;

        // encode the gif::Frame
        self.encode(&frame)
    }
}

impl From<gif::DecodingError> for ImageError {
    fn from(err: gif::DecodingError) -> ImageError {
        use self::gif::DecodingError::*;
        match err {
            Format(desc) | Internal(desc) => ImageError::FormatError(desc.into()),
            Io(io_err) => ImageError::IoError(io_err),
        }
    }
}
