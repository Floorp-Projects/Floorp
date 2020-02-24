//!  Decoding of GIF Images
//!
//!  GIF (Graphics Interchange Format) is an image format that supports lossless compression.
//!
//!  # Related Links
//!  * <http://www.w3.org/Graphics/GIF/spec-gif89a.txt> - The GIF Specification
//!
//! # Examples
//! ```rust,no_run
//! use image::gif::{GifDecoder, Encoder};
//! use image::{ImageDecoder, AnimationDecoder};
//! use std::fs::File;
//! # fn main() -> std::io::Result<()> {
//! // Decode a gif into frames
//! let file_in = File::open("foo.gif")?;
//! let mut decoder = GifDecoder::new(file_in).unwrap();
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
#![allow(clippy::while_let_loop)]

use std::clone::Clone;
use std::convert::TryInto;
use std::cmp::min;
use std::convert::TryFrom;
use std::io::{self, Cursor, Read, Write};
use std::marker::PhantomData;
use std::mem;

use gif::{ColorOutput, SetParameter};
use gif::{DisposalMethod, Frame};
use num_rational::Ratio;

use crate::animation;
use crate::buffer::{ImageBuffer, Pixel};
use crate::color::{ColorType, Rgba};
use crate::error::{ImageError, ImageResult};
use crate::image::{self, AnimationDecoder, ImageDecoder};

/// GIF decoder
pub struct GifDecoder<R: Read> {
    reader: gif::Reader<R>,
}

impl<R: Read> GifDecoder<R> {
    /// Creates a new decoder that decodes the input steam ```r```
    pub fn new(r: R) -> ImageResult<GifDecoder<R>> {
        let mut decoder = gif::Decoder::new(r);
        decoder.set(ColorOutput::RGBA);

        Ok(GifDecoder {
            reader: decoder.read_info().map_err(ImageError::from_gif)?,
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

impl<'a, R: 'a + Read> ImageDecoder<'a> for GifDecoder<R> {
    type Reader = GifReader<R>;

    fn dimensions(&self) -> (u32, u32) {
        (u32::from(self.reader.width()), u32::from(self.reader.height()))
    }

    fn color_type(&self) -> ColorType {
        ColorType::Rgba8
    }

    fn into_reader(self) -> ImageResult<Self::Reader> {
        Ok(GifReader(Cursor::new(image::decoder_to_vec(self)?), PhantomData))
    }

    fn read_image(mut self, buf: &mut [u8]) -> ImageResult<()> {
        assert_eq!(u64::try_from(buf.len()), Ok(self.total_bytes()));

        let (f_width, f_height, left, top);

        if let Some(frame) = self.reader.next_frame_info().map_err(ImageError::from_gif)? {
            left = u32::from(frame.left);
            top = u32::from(frame.top);
            f_width = u32::from(frame.width);
            f_height = u32::from(frame.height);
        } else {
            return Err(ImageError::ImageEnd);
        }

        self.reader.read_into_buffer(buf).map_err(ImageError::from_gif)?;

        let (width, height) = (u32::from(self.reader.width()), u32::from(self.reader.height()));
        if (left, top) != (0, 0) || (width, height) != (f_width, f_height) {
            // This is somewhat of an annoying case. The image we read into `buf` doesn't take up
            // the whole buffer and now we need to properly insert borders. For simplicity this code
            // currently takes advantage of the `ImageBuffer::from_fn` function to make a second
            // ImageBuffer that is properly positioned, and then copies it back into `buf`.
            //
            // TODO: Implement this without any allocation.

            // Recover the full image
            let image_buffer = {
                // See the comments inside `<GifFrameIterator as Iterator>::next` about
                // the error handling of `from_raw`.
                let image = ImageBuffer::from_raw(f_width, f_height, &mut *buf).ok_or_else(
                    || ImageError::UnsupportedError("Image dimensions are too large".into())
                )?;

                ImageBuffer::from_fn(width, height, |x, y| {
                    let x = x.wrapping_sub(left);
                    let y = y.wrapping_sub(top);
                    if x < image.width() && y < image.height() {
                        *image.get_pixel(x, y)
                    } else {
                        Rgba([0, 0, 0, 0])
                    }
                })
            };
            buf.copy_from_slice(&mut image_buffer.into_raw());
        }
        Ok(())
    }
}

struct GifFrameIterator<R: Read> {
    reader: gif::Reader<R>,

    width: u32,
    height: u32,

    non_disposed_frame: ImageBuffer<Rgba<u8>, Vec<u8>>,
}


impl<R: Read> GifFrameIterator<R> {
    fn new(decoder: GifDecoder<R>) -> GifFrameIterator<R> {
        let (width, height) = decoder.dimensions();

        // TODO: Avoid this cast
        let (width, height) = (width as u32, height as u32);

        // intentionally ignore the background color for web compatibility

        // create the first non disposed frame
        let non_disposed_frame = ImageBuffer::from_pixel(width, height, Rgba([0, 0, 0, 0]));

        GifFrameIterator {
            reader: decoder.reader,
            width,
            height,
            non_disposed_frame,
        }
    }
}


impl<R: Read> Iterator for GifFrameIterator<R> {
    type Item = ImageResult<animation::Frame>;

    fn next(&mut self) -> Option<ImageResult<animation::Frame>> {
        // begin looping over each frame
        let (left, top, delay, dispose, f_width, f_height);

        match self.reader.next_frame_info() {
            Ok(frame_info) => {
                if let Some(frame) = frame_info {
                    left = u32::from(frame.left);
                    top = u32::from(frame.top);
                    f_width = u32::from(frame.width);
                    f_height = u32::from(frame.height);

                    // frame.delay is in units of 10ms so frame.delay*10 is in ms
                    delay = Ratio::new(u32::from(frame.delay) * 10, 1);
                    dispose = frame.dispose;
                } else {
                    // no more frames
                    return None;
                }
            },
            Err(err) => return Some(Err(ImageError::from_gif(err))),
        }

        let mut vec = vec![0; self.reader.buffer_size()];
        if let Err(err) = self.reader.read_into_buffer(&mut vec) {
            return Some(Err(ImageError::from_gif(err)));
        }

        // create the image buffer from the raw frame.
        // `buffer_size` uses wrapping arithmetics, thus might not report the
        // correct storage requirement if the result does not fit in `usize`.
        // on the other hand, `ImageBuffer::from_raw` detects overflow and
        // reports by returning `None`.
        let image_buffer_raw = match ImageBuffer::from_raw(f_width, f_height, vec) {
            Some(image_buffer_raw) => image_buffer_raw,
            None => {
                return Some(Err(ImageError::UnsupportedError(
                    "Image dimensions are too large".into(),
                )))
            }
        };

        // if `image_buffer_raw`'s frame exactly matches the entire image, then
        // use it directly.
        //
        // otherwise, `image_buffer_raw` represents a smaller image.
        // create a new image of the target size and place
        // `image_buffer_raw` within it. the outside region is filled with
        // transparent pixels.
        let mut image_buffer =
            full_image_from_frame(self.width, self.height, image_buffer_raw, left, top);

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
            image_buffer.clone(), 0, 0, animation::Delay::from_ratio(delay),
        );

        match dispose {
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
                for y in top..min(top + f_height, self.height) {
                    for x in left..min(left + f_width, self.width) {
                        self.non_disposed_frame.put_pixel(x, y, Rgba([0, 0, 0, 0]));
                    }
                }
            }
            DisposalMethod::Previous => {
                // restore to previous
                // (dispose frames leaving the last none disposal frame)
            }
        };

        Some(Ok(frame))
    }
}

/// Given a frame subimage, construct a full image of size
/// `(screen_width, screen_height)` by placing it at the top-left coordinates
/// `(left, top)`. The remaining portion is filled with transparent pixels.
fn full_image_from_frame(
    screen_width: u32,
    screen_height: u32,
    image: crate::RgbaImage,
    left: u32,
    top: u32,
) -> crate::RgbaImage {
    if (left, top) == (0, 0) && (screen_width, screen_height) == (image.width(), image.height()) {
        image
    } else {
        ImageBuffer::from_fn(screen_width, screen_height, |x, y| {
            let x = x.wrapping_sub(left);
            let y = y.wrapping_sub(top);
            if x < image.width() && y < image.height() {
                *image.get_pixel(x, y)
            } else {
                Rgba([0, 0, 0, 0])
            }
        })
    }
}

impl<'a, R: Read + 'a> AnimationDecoder<'a> for GifDecoder<R> {
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

    /// Encode a single image.
    pub fn encode(
        &mut self,
        data: &[u8],
        width: u32,
        height: u32,
        color: ColorType,
    ) -> ImageResult<()> {
        let (width, height) = self.gif_dimensions(width, height)?;
        match color {
            ColorType::Rgb8 => self.encode_gif(Frame::from_rgb(width, height, data)),
            ColorType::Rgba8 => {
                self.encode_gif(Frame::from_rgb(width, height, &mut data.to_owned()))
            },
            _ => Err(ImageError::UnsupportedColor(color.into())),
        }
    }

    /// Encode one frame of animation.
    pub fn encode_frame(&mut self, img_frame: animation::Frame) -> ImageResult<()> {
        let frame = self.convert_frame(img_frame)?;
        self.encode_gif(frame)
    }

    /// Encodes Frames.
    /// Consider using `try_encode_frames` instead to encode an `animation::Frames` like iterator.
    pub fn encode_frames<F>(&mut self, frames: F) -> ImageResult<()>
    where
        F: IntoIterator<Item = animation::Frame>
    {
        for img_frame in frames {
            self.encode_frame(img_frame)?;
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
            self.encode_frame(img_frame?)?;
        }
        Ok(())
    }

    pub(crate) fn convert_frame(&mut self, img_frame: animation::Frame)
        -> ImageResult<Frame<'static>>
    {
        // get the delay before converting img_frame
        let frame_delay = img_frame.delay().into_ratio().to_integer();
        // convert img_frame into RgbaImage
        let mut rbga_frame = img_frame.into_buffer();
        let (width, height) = self.gif_dimensions(
            rbga_frame.width(),
            rbga_frame.height())?;

        // Create the gif::Frame from the animation::Frame
        let mut frame = Frame::from_rgba(width, height, &mut *rbga_frame);
        frame.delay = (frame_delay / 10).try_into().map_err(|_|ImageError::DimensionError)?;

        Ok(frame)
    }

    fn gif_dimensions(&self, width: u32, height: u32) -> ImageResult<(u16, u16)> {
        fn inner_dimensions(width: u32, height: u32) -> Option<(u16, u16)> {
            let width = u16::try_from(width).ok()?;
            let height = u16::try_from(height).ok()?;
            Some((width, height))
        }

        inner_dimensions(width, height).ok_or(ImageError::DimensionError)
    }

    pub(crate) fn encode_gif(&mut self, frame: Frame) -> ImageResult<()> {
        let gif_encoder;
        if let Some(ref mut encoder) = self.gif_encoder {
            gif_encoder = encoder;
        } else {
            let writer = self.w.take().unwrap();
            let encoder = gif::Encoder::new(writer, frame.width, frame.height, &[])?;
            self.gif_encoder = Some(encoder);
            gif_encoder = self.gif_encoder.as_mut().unwrap()
        }

        gif_encoder.write_frame(&frame).map_err(|err| err.into())
    }
}

impl ImageError {
    fn from_gif(err: gif::DecodingError) -> ImageError {
        use gif::DecodingError::*;
        match err {
            Format(desc) | Internal(desc) => ImageError::FormatError(desc.into()),
            Io(io_err) => ImageError::IoError(io_err),
        }
    }
}
