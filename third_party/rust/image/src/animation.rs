use std::iter::Iterator;

use num_rational::Ratio;

use buffer::RgbaImage;
use image::ImageResult;

/// An implementation dependent iterator, reading the frames as requested
pub struct Frames<'a> {
    iterator: Box<dyn Iterator<Item = ImageResult<Frame>> + 'a>
}

impl<'a> Frames<'a> {
    /// Creates a new `Frames` from an implementation specific iterator.
    pub fn new(iterator: Box<dyn Iterator<Item = ImageResult<Frame>> + 'a>) -> Self {
        Frames { iterator }
    }

    /// Steps through the iterator from the current frame until the end and pushes each frame into
    /// a `Vec`.
    /// If en error is encountered that error is returned instead.
    /// 
    /// Note: This is equivalent to `Frames::collect::<ImageResult<Vec<Frame>>>()`
    pub fn collect_frames(self) -> ImageResult<Vec<Frame>> {
        self.collect()
    }
}

impl<'a> Iterator for Frames<'a> {
    type Item = ImageResult<Frame>;
    fn next(&mut self) -> Option<ImageResult<Frame>> {
        self.iterator.next()
    }
}

/// A single animation frame
#[derive(Clone)]
pub struct Frame {
    /// Delay between the frames in s
    delay: Ratio<u16>,
    /// x offset
    left: u32,
    /// y offset
    top: u32,
    buffer: RgbaImage,
}

impl Frame {
    /// Contructs a new frame
    pub fn new(buffer: RgbaImage) -> Frame {
        Frame {
            delay: Ratio::from_integer(0),
            left: 0,
            top: 0,
            buffer,
        }
    }

    /// Contructs a new frame
    pub fn from_parts(buffer: RgbaImage, left: u32, top: u32, delay: Ratio<u16>) -> Frame {
        Frame {
            delay,
            left,
            top,
            buffer,
        }
    }

    /// Delay of this frame
    pub fn delay(&self) -> Ratio<u16> {
        self.delay
    }

    /// Returns the image buffer
    pub fn buffer(&self) -> &RgbaImage {
        &self.buffer
    }

    /// Returns the image buffer
    pub fn into_buffer(self) -> RgbaImage {
        self.buffer
    }

    /// Returns the x offset
    pub fn left(&self) -> u32 {
        self.left
    }

    /// Returns the y offset
    pub fn top(&self) -> u32 {
        self.top
    }
}
