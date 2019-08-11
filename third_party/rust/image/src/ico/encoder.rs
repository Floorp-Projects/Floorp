use byteorder::{LittleEndian, WriteBytesExt};
use std::io::{self, Write};

use color::{bits_per_pixel, ColorType};

use png::PNGEncoder;

// Enum value indicating an ICO image (as opposed to a CUR image):
const ICO_IMAGE_TYPE: u16 = 1;
// The length of an ICO file ICONDIR structure, in bytes:
const ICO_ICONDIR_SIZE: u32 = 6;
// The length of an ICO file DIRENTRY structure, in bytes:
const ICO_DIRENTRY_SIZE: u32 = 16;

/// ICO encoder
pub struct ICOEncoder<W: Write> {
    w: W,
}

impl<W: Write> ICOEncoder<W> {
    /// Create a new encoder that writes its output to ```w```.
    pub fn new(w: W) -> ICOEncoder<W> {
        ICOEncoder { w }
    }

    /// Encodes the image ```image``` that has dimensions ```width``` and
    /// ```height``` and ```ColorType``` ```c```.  The dimensions of the image
    /// must be between 1 and 256 (inclusive) or an error will be returned.
    pub fn encode(
        mut self,
        data: &[u8],
        width: u32,
        height: u32,
        color: ColorType,
    ) -> io::Result<()> {
        let mut image_data: Vec<u8> = Vec::new();
        try!(PNGEncoder::new(&mut image_data).encode(data, width, height, color));

        try!(write_icondir(&mut self.w, 1));
        try!(write_direntry(
            &mut self.w,
            width,
            height,
            color,
            ICO_ICONDIR_SIZE + ICO_DIRENTRY_SIZE,
            image_data.len() as u32
        ));
        try!(self.w.write_all(&image_data));
        Ok(())
    }
}

fn write_icondir<W: Write>(w: &mut W, num_images: u16) -> io::Result<()> {
    // Reserved field (must be zero):
    try!(w.write_u16::<LittleEndian>(0));
    // Image type (ICO or CUR):
    try!(w.write_u16::<LittleEndian>(ICO_IMAGE_TYPE));
    // Number of images in the file:
    try!(w.write_u16::<LittleEndian>(num_images));
    Ok(())
}

fn write_direntry<W: Write>(
    w: &mut W,
    width: u32,
    height: u32,
    color: ColorType,
    data_start: u32,
    data_size: u32,
) -> io::Result<()> {
    // Image dimensions:
    try!(write_width_or_height(w, width));
    try!(write_width_or_height(w, height));
    // Number of colors in palette (or zero for no palette):
    try!(w.write_u8(0));
    // Reserved field (must be zero):
    try!(w.write_u8(0));
    // Color planes:
    try!(w.write_u16::<LittleEndian>(0));
    // Bits per pixel:
    try!(w.write_u16::<LittleEndian>(bits_per_pixel(color) as u16));
    // Image data size, in bytes:
    try!(w.write_u32::<LittleEndian>(data_size));
    // Image data offset, in bytes:
    try!(w.write_u32::<LittleEndian>(data_start));
    Ok(())
}

/// Encode a width/height value as a single byte, where 0 means 256.
fn write_width_or_height<W: Write>(w: &mut W, value: u32) -> io::Result<()> {
    if value < 1 || value > 256 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidData,
            "Invalid ICO dimensions (width and \
             height must be between 1 and 256)",
        ));
    }
    w.write_u8(if value < 256 { value as u8 } else { 0 })
}
