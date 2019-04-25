//! Common types shared between the encoder and decoder
extern crate deflate;
use filter;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum ColorType {
    Grayscale = 0,
    RGB = 2,
    Indexed = 3,
    GrayscaleAlpha = 4,
    RGBA = 6
}

impl ColorType {
    /// Returns the number of samples used per pixel of `ColorType`
    pub fn samples(&self) -> usize {
        use self::ColorType::*;
        match *self {
            Grayscale | Indexed => 1,
            RGB => 3,
            GrayscaleAlpha => 2,
            RGBA => 4
        }
    }
    
    /// u8 -> Self. Temporary solution until Rust provides a canonical one.
    pub fn from_u8(n: u8) -> Option<ColorType> {
        match n {
            0 => Some(ColorType::Grayscale),
            2 => Some(ColorType::RGB),
            3 => Some(ColorType::Indexed),
            4 => Some(ColorType::GrayscaleAlpha),
            6 => Some(ColorType::RGBA),
            _ => None
        }
    }
}

/// Bit depth of the png file
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
pub enum BitDepth {
    One     = 1,
    Two     = 2,
    Four    = 4,
    Eight   = 8,
    Sixteen = 16,
}

impl BitDepth {
    /// u8 -> Self. Temporary solution until Rust provides a canonical one.
    pub fn from_u8(n: u8) -> Option<BitDepth> {
        match n {
            1 => Some(BitDepth::One),
            2 => Some(BitDepth::Two),
            4 => Some(BitDepth::Four),
            8 => Some(BitDepth::Eight),
            16 => Some(BitDepth::Sixteen),
            _ => None
        }
    }
}

/// Pixel dimensions information
#[derive(Clone, Copy, Debug)]
pub struct PixelDimensions {
    /// Pixels per unit, X axis
    pub xppu: u32,
    /// Pixels per unit, Y axis
    pub yppu: u32,
    /// Either *Meter* or *Unspecified*
    pub unit: Unit,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
#[repr(u8)]
/// Physical unit of the pixel dimensions
pub enum Unit {
    Unspecified = 0,
    Meter = 1,
}

impl Unit {
    /// u8 -> Self. Temporary solution until Rust provides a canonical one.
    pub fn from_u8(n: u8) -> Option<Unit> {
        match n {
            0 => Some(Unit::Unspecified),
            1 => Some(Unit::Meter),
            _ => None
        }
    }
}

/// Frame control information
#[derive(Clone, Debug)]
pub struct FrameControl {
    /// Sequence number of the animation chunk, starting from 0
    pub sequence_number: u32,
    /// Width of the following frame
    pub width: u32,
    /// Height of the following frame
    pub height: u32,
    /// X position at which to render the following frame
    pub x_offset: u32,
    /// Y position at which to render the following frame
    pub y_offset: u32,
    /// Frame delay fraction numerator
    pub delay_num: u16,
    /// Frame delay fraction denominator
    pub delay_den: u16,
    /// Type of frame area disposal to be done after rendering this frame
    pub dispose_op: u8,
    /// Type of frame area rendering for this frame
    pub blend_op: u8,
}

/// Animation control information
#[derive(Clone, Copy, Debug)]
pub struct AnimationControl {
    /// Number of frames
    pub num_frames: u32,
    /// Number of times to loop this APNG.  0 indicates infinite looping.
    pub num_plays: u32,
}

#[derive(Debug, Clone)]
pub enum Compression {
    /// Default level  
    Default,
    /// Fast minimal compression
    Fast,
    /// Higher compression level  
    ///
    /// Best in this context isn't actually the highest possible level
    /// the encoder can do, but is meant to emulate the `Best` setting in the `Flate2`
    /// library.
    Best,
    Huffman,
    Rle,
}

impl From<deflate::Compression> for Compression {
    fn from(c: deflate::Compression) -> Self {
        match c {
            deflate::Compression::Default => Compression::Default,
            deflate::Compression::Fast => Compression::Fast,
            deflate::Compression::Best => Compression::Best,
        }
    }
}

impl From<Compression> for deflate::CompressionOptions {
    fn from(c: Compression) -> Self {
        match c {
            Compression::Default => deflate::CompressionOptions::default(),
            Compression::Fast => deflate::CompressionOptions::fast(),
            Compression::Best => deflate::CompressionOptions::high(),
            Compression::Huffman => deflate::CompressionOptions::huffman_only(),
            Compression::Rle => deflate::CompressionOptions::rle(),
        }
    }
}

/// PNG info struct
#[derive(Debug)]
pub struct Info {
    pub width: u32,
    pub height: u32,
    pub bit_depth: BitDepth,
    pub color_type: ColorType,
    pub interlaced: bool,
    pub trns: Option<Vec<u8>>,
    pub pixel_dims: Option<PixelDimensions>,
    pub palette: Option<Vec<u8>>,
    pub frame_control: Option<FrameControl>,
    pub animation_control: Option<AnimationControl>,
    pub compression: Compression,
    pub filter: filter::FilterType,
}

impl Default for Info {
    fn default() -> Info {
        Info {
            width: 0,
            height: 0,
            bit_depth: BitDepth::Eight,
            color_type: ColorType::Grayscale,
            interlaced: false,
            palette: None,
            trns: None,
            pixel_dims: None,
            frame_control: None,
            animation_control: None,
            // Default to `deflate::Compresion::Fast` and `filter::FilterType::Sub` 
            // to maintain backward compatible output. 
            compression: deflate::Compression::Fast.into(),
            filter: filter::FilterType::Sub,
        }
    }
}

impl Info {
    /// Size of the image
    pub fn size(&self) -> (u32, u32) {
        (self.width, self.height)
    }
    
    /// Returns true if the image is an APNG image.
    pub fn is_animated(&self) -> bool {
        self.frame_control.is_some() && self.animation_control.is_some()
    }
    
    /// Returns the frame control information of the image
    pub fn animation_control(&self) -> Option<&AnimationControl> {
        self.animation_control.as_ref()
    }
    
    /// Returns the frame control information of the current frame
    pub fn frame_control(&self) -> Option<&FrameControl> {
        self.frame_control.as_ref()
    }
    
    /// Returns the bits per pixel
    pub fn bits_per_pixel(&self) -> usize {
        self.color_type.samples() * self.bit_depth as usize
    }
    
    /// Returns the bytes per pixel
    pub fn bytes_per_pixel(&self) -> usize {
        self.color_type.samples() * ((self.bit_depth as usize + 7) >> 3)
    }
    
    /// Returns the number of bytes needed for one deinterlaced image
    pub fn raw_bytes(&self) -> usize {
        self.height as usize * self.raw_row_length()
    }
    
    /// Returns the number of bytes needed for one deinterlaced row 
    pub fn raw_row_length(&self) -> usize {
        let bits = self.width as usize * self.color_type.samples() * self.bit_depth as usize;
        let extra = bits % 8;
        bits/8
        + match extra { 0 => 0, _ => 1 }
        + 1 // filter method
    }
    
    /// Returns the number of bytes needed for one deinterlaced row of width `width`
    pub fn raw_row_length_from_width(&self, width: u32) -> usize {
        let bits = width as usize * self.color_type.samples() * self.bit_depth as usize;
        let extra = bits % 8;
        bits/8
        + match extra { 0 => 0, _ => 1 }
        + 1 // filter method
    }
}


bitflags! {
    /// # Output transformations
    ///
    /// Only `IDENTITY` and `TRANSFORM_EXPAND | TRANSFORM_STRIP_ALPHA` can be used at the moment.
    pub struct Transformations: u32 {
        /// No transformation
        const IDENTITY            = 0x0000; // read and write */
        /// Strip 16-bit samples to 8 bits
        const STRIP_16            = 0x0001; // read only */
        /// Discard the alpha channel
        const STRIP_ALPHA         = 0x0002; // read only */
        /// Expand 1; 2 and 4-bit samples to bytes
        const PACKING             = 0x0004; // read and write */
        /// Change order of packed pixels to LSB first
        const PACKSWAP            = 0x0008; // read and write */
        /// Expand paletted images to RGB; expand grayscale images of
        /// less than 8-bit depth to 8-bit depth; and expand tRNS chunks
        /// to alpha channels.
        const EXPAND              = 0x0010; // read only */
        /// Invert monochrome images
        const INVERT_MONO         = 0x0020; // read and write */
        /// Normalize pixels to the sBIT depth
        const SHIFT               = 0x0040; // read and write */
        /// Flip RGB to BGR; RGBA to BGRA
        const BGR                 = 0x0080; // read and write */
        /// Flip RGBA to ARGB or GA to AG
        const SWAP_ALPHA          = 0x0100; // read and write */
        /// Byte-swap 16-bit samples
        const SWAP_ENDIAN         = 0x0200; // read and write */
        /// Change alpha from opacity to transparency
        const INVERT_ALPHA        = 0x0400; // read and write */
        const STRIP_FILLER        = 0x0800; // write only */
        const STRIP_FILLER_BEFORE = 0x0800; // write only
        const STRIP_FILLER_AFTER  = 0x1000; // write only */
        const GRAY_TO_RGB         = 0x2000; // read only */
        const EXPAND_16           = 0x4000; // read only */
        const SCALE_16            = 0x8000; // read only */
    }
}

