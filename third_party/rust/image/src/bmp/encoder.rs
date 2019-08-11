use byteorder::{LittleEndian, WriteBytesExt};
use std::io::{self, Write};

use color;

/// The representation of a BMP encoder.
pub struct BMPEncoder<'a, W: 'a> {
    writer: &'a mut W,
}

impl<'a, W: Write + 'a> BMPEncoder<'a, W> {
    /// Create a new encoder that writes its output to ```w```.
    pub fn new(w: &'a mut W) -> Self {
        BMPEncoder { writer: w }
    }

    /// Encodes the image ```image```
    /// that has dimensions ```width``` and ```height```
    /// and ```ColorType``` ```c```.
    pub fn encode(
        &mut self,
        image: &[u8],
        width: u32,
        height: u32,
        c: color::ColorType,
    ) -> io::Result<()> {
        let bmp_header_size = 14;
        let dib_header_size = 40; // using BITMAPINFOHEADER

        let (raw_pixel_size, written_pixel_size, palette_color_count) = try!(get_pixel_info(c));
        let row_pad_size = (4 - (width * written_pixel_size) % 4) % 4; // each row must be padded to a multiple of 4 bytes

        let image_size = width * height * written_pixel_size + (height * row_pad_size);
        let palette_size = palette_color_count * 4; // all palette colors are BGRA
        let file_size = bmp_header_size + dib_header_size + palette_size + image_size;

        // write BMP header
        try!(self.writer.write_u8(b'B'));
        try!(self.writer.write_u8(b'M'));
        try!(self.writer.write_u32::<LittleEndian>(file_size)); // file size
        try!(self.writer.write_u16::<LittleEndian>(0)); // reserved 1
        try!(self.writer.write_u16::<LittleEndian>(0)); // reserved 2
        try!(
            self.writer
                .write_u32::<LittleEndian>(bmp_header_size + dib_header_size + palette_size)
        ); // image data offset

        // write DIB header
        try!(self.writer.write_u32::<LittleEndian>(dib_header_size));
        try!(self.writer.write_i32::<LittleEndian>(width as i32));
        try!(self.writer.write_i32::<LittleEndian>(height as i32));
        try!(self.writer.write_u16::<LittleEndian>(1)); // color planes
        try!(
            self.writer
                .write_u16::<LittleEndian>((written_pixel_size * 8) as u16)
        ); // bits per pixel
        try!(self.writer.write_u32::<LittleEndian>(0)); // compression method - no compression
        try!(self.writer.write_u32::<LittleEndian>(image_size));
        try!(self.writer.write_i32::<LittleEndian>(0)); // horizontal ppm
        try!(self.writer.write_i32::<LittleEndian>(0)); // vertical ppm
        try!(self.writer.write_u32::<LittleEndian>(palette_color_count));
        try!(self.writer.write_u32::<LittleEndian>(0)); // all colors are important

        // write image data
        match c {
            color::ColorType::RGB(8) | color::ColorType::RGBA(8) => {
                try!(self.encode_rgb(image, width, height, row_pad_size, raw_pixel_size))
            }
            color::ColorType::Gray(8) | color::ColorType::GrayA(8) => {
                try!(self.encode_gray(image, width, height, row_pad_size, raw_pixel_size))
            }
            _ => {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    &get_unsupported_error_message(c)[..],
                ))
            }
        }

        Ok(())
    }

    fn encode_rgb(
        &mut self,
        image: &[u8],
        width: u32,
        height: u32,
        row_pad_size: u32,
        bytes_per_pixel: u32,
    ) -> io::Result<()> {
        let x_stride = bytes_per_pixel;
        let y_stride = width * x_stride;
        for row in 0..height {
            // from the bottom up
            let row_start = (height - row - 1) * y_stride;
            for col in 0..width {
                let pixel_start = (row_start + (col * x_stride)) as usize;
                let r = image[pixel_start];
                let g = image[pixel_start + 1];
                let b = image[pixel_start + 2];
                // written as BGR
                try!(self.writer.write_u8(b));
                try!(self.writer.write_u8(g));
                try!(self.writer.write_u8(r));
                // alpha is never written as it's not widely supported
            }

            try!(self.write_row_pad(row_pad_size));
        }

        Ok(())
    }

    fn encode_gray(
        &mut self,
        image: &[u8],
        width: u32,
        height: u32,
        row_pad_size: u32,
        bytes_per_pixel: u32,
    ) -> io::Result<()> {
        // write grayscale palette
        for val in 0..256 {
            // each color is written as BGRA, where A is always 0 and since only grayscale is being written, B = G = R = index
            let val = val as u8;
            try!(self.writer.write_u8(val));
            try!(self.writer.write_u8(val));
            try!(self.writer.write_u8(val));
            try!(self.writer.write_u8(0));
        }

        // write image data
        let x_stride = bytes_per_pixel;
        let y_stride = width * x_stride;
        for row in 0..height {
            // from the bottom up
            let row_start = (height - row - 1) * y_stride;
            for col in 0..width {
                let pixel_start = (row_start + (col * x_stride)) as usize;
                // color value is equal to the palette index
                try!(self.writer.write_u8(image[pixel_start]));
                // alpha is never written as it's not widely supported
            }

            try!(self.write_row_pad(row_pad_size));
        }

        Ok(())
    }

    fn write_row_pad(&mut self, row_pad_size: u32) -> io::Result<()> {
        for _ in 0..row_pad_size {
            try!(self.writer.write_u8(0));
        }

        Ok(())
    }
}

fn get_unsupported_error_message(c: color::ColorType) -> String {
    format!(
        "Unsupported color type {:?}.  Supported types: RGB(8), RGBA(8), Gray(8), GrayA(8).",
        c
    )
}

/// Returns a tuple representing: (raw pixel size, written pixel size, palette color count).
fn get_pixel_info(c: color::ColorType) -> io::Result<(u32, u32, u32)> {
    let sizes = match c {
        color::ColorType::RGB(8) => (3, 3, 0),
        color::ColorType::RGBA(8) => (4, 3, 0),
        color::ColorType::Gray(8) => (1, 1, 256),
        color::ColorType::GrayA(8) => (2, 1, 256),
        _ => {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                &get_unsupported_error_message(c)[..],
            ))
        }
    };

    Ok(sizes)
}

#[cfg(test)]
mod tests {
    use super::super::BMPDecoder;
    use super::BMPEncoder;
    use color::ColorType;
    use image::ImageDecoder;
    use std::io::Cursor;

    fn round_trip_image(image: &[u8], width: u32, height: u32, c: ColorType) -> Vec<u8> {
        let mut encoded_data = Vec::new();
        {
            let mut encoder = BMPEncoder::new(&mut encoded_data);
            encoder
                .encode(&image, width, height, c)
                .expect("could not encode image");
        }

        let decoder = BMPDecoder::new(Cursor::new(&encoded_data)).expect("failed to decode");
        decoder.read_image().expect("failed to decode")
    }

    #[test]
    fn round_trip_single_pixel_rgb() {
        let image = [255u8, 0, 0]; // single red pixel
        let decoded = round_trip_image(&image, 1, 1, ColorType::RGB(8));
        assert_eq!(3, decoded.len());
        assert_eq!(255, decoded[0]);
        assert_eq!(0, decoded[1]);
        assert_eq!(0, decoded[2]);
    }

    #[test]
    fn round_trip_single_pixel_rgba() {
        let image = [255u8, 0, 0, 0]; // single red pixel
        let decoded = round_trip_image(&image, 1, 1, ColorType::RGBA(8));
        assert_eq!(3, decoded.len());
        assert_eq!(255, decoded[0]);
        assert_eq!(0, decoded[1]);
        assert_eq!(0, decoded[2]);
    }

    #[test]
    fn round_trip_3px_rgb() {
        let image = [0u8; 3 * 3 * 3]; // 3x3 pixels, 3 bytes per pixel
        let _decoded = round_trip_image(&image, 3, 3, ColorType::RGB(8));
    }

    #[test]
    fn round_trip_gray() {
        let image = [0u8, 1, 2]; // 3 pixels
        let decoded = round_trip_image(&image, 3, 1, ColorType::Gray(8));
        // should be read back as 3 RGB pixels
        assert_eq!(9, decoded.len());
        assert_eq!(0, decoded[0]);
        assert_eq!(0, decoded[1]);
        assert_eq!(0, decoded[2]);
        assert_eq!(1, decoded[3]);
        assert_eq!(1, decoded[4]);
        assert_eq!(1, decoded[5]);
        assert_eq!(2, decoded[6]);
        assert_eq!(2, decoded[7]);
        assert_eq!(2, decoded[8]);
    }

    #[test]
    fn round_trip_graya() {
        let image = [0u8, 0, 1, 0, 2, 0]; // 3 pixels, each with an alpha channel
        let decoded = round_trip_image(&image, 1, 3, ColorType::GrayA(8));
        // should be read back as 3 RGB pixels
        assert_eq!(9, decoded.len());
        assert_eq!(0, decoded[0]);
        assert_eq!(0, decoded[1]);
        assert_eq!(0, decoded[2]);
        assert_eq!(1, decoded[3]);
        assert_eq!(1, decoded[4]);
        assert_eq!(1, decoded[5]);
        assert_eq!(2, decoded[6]);
        assert_eq!(2, decoded[7]);
        assert_eq!(2, decoded[8]);
    }
}
