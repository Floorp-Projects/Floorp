use byteorder::{LittleEndian, WriteBytesExt};
use std::io::{self, Write};

use crate::color;
use crate::error::{ImageError, ImageResult};
use crate::image::ImageEncoder;

const BITMAPFILEHEADER_SIZE: u32 = 14;
const BITMAPINFOHEADER_SIZE: u32 = 40;
const BITMAPV4HEADER_SIZE: u32 = 108;

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
    ) -> ImageResult<()> {
        let bmp_header_size = BITMAPFILEHEADER_SIZE;

        let (dib_header_size, written_pixel_size, palette_color_count) = get_pixel_info(c)?;
        let row_pad_size = (4 - (width * written_pixel_size) % 4) % 4; // each row must be padded to a multiple of 4 bytes
        let image_size = width
            .checked_mul(height)
            .ok_or(ImageError::DimensionError)?
            .checked_mul(written_pixel_size)
            .ok_or(ImageError::DimensionError)?
            .checked_add(height * row_pad_size)
            .ok_or(ImageError::DimensionError)?;
        let palette_size = palette_color_count * 4; // all palette colors are BGRA
        let file_size = bmp_header_size + dib_header_size + palette_size + image_size;

        // write BMP header
        self.writer.write_u8(b'B')?;
        self.writer.write_u8(b'M')?;
        self.writer.write_u32::<LittleEndian>(file_size)?; // file size
        self.writer.write_u16::<LittleEndian>(0)?; // reserved 1
        self.writer.write_u16::<LittleEndian>(0)?; // reserved 2
        self.writer
            .write_u32::<LittleEndian>(bmp_header_size + dib_header_size + palette_size)?; // image data offset

        // write DIB header
        self.writer.write_u32::<LittleEndian>(dib_header_size)?;
        self.writer.write_i32::<LittleEndian>(width as i32)?;
        self.writer.write_i32::<LittleEndian>(height as i32)?;
        self.writer.write_u16::<LittleEndian>(1)?; // color planes
        self.writer
            .write_u16::<LittleEndian>((written_pixel_size * 8) as u16)?; // bits per pixel
        if dib_header_size >= BITMAPV4HEADER_SIZE {
            // Assume BGRA32
            self.writer.write_u32::<LittleEndian>(3)?; // compression method - bitfields
        } else {
            self.writer.write_u32::<LittleEndian>(0)?; // compression method - no compression
        }
        self.writer.write_u32::<LittleEndian>(image_size)?;
        self.writer.write_i32::<LittleEndian>(0)?; // horizontal ppm
        self.writer.write_i32::<LittleEndian>(0)?; // vertical ppm
        self.writer.write_u32::<LittleEndian>(palette_color_count)?;
        self.writer.write_u32::<LittleEndian>(0)?; // all colors are important
        if dib_header_size >= BITMAPV4HEADER_SIZE {
            // Assume BGRA32
            self.writer.write_u32::<LittleEndian>(0xff << 16)?; // red mask
            self.writer.write_u32::<LittleEndian>(0xff << 8)?; // green mask
            self.writer.write_u32::<LittleEndian>(0xff << 0)?; // blue mask
            self.writer.write_u32::<LittleEndian>(0xff << 24)?; // alpha mask
            self.writer.write_u32::<LittleEndian>(0x73524742)?; // colorspace - sRGB

            // endpoints (3x3) and gamma (3)
            for _ in 0..12 {
                self.writer.write_u32::<LittleEndian>(0)?;
            }
        }

        // write image data
        match c {
            color::ColorType::Rgb8 => {
                self.encode_rgb(image, width, height, row_pad_size, 3)?
            }
            color::ColorType::Rgba8 => {
                self.encode_rgba(image, width, height, row_pad_size, 4)?
            }
            color::ColorType::L8 => {
                self.encode_gray(image, width, height, row_pad_size, 1)?
            }
            color::ColorType::La8 => {
                self.encode_gray(image, width, height, row_pad_size, 2)?
            }
            _ => {
                return Err(ImageError::IoError(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    &get_unsupported_error_message(c)[..],
                )))
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
                self.writer.write_u8(b)?;
                self.writer.write_u8(g)?;
                self.writer.write_u8(r)?;
                // alpha is never written as it's not widely supported
            }

            self.write_row_pad(row_pad_size)?;
        }

        Ok(())
    }

    fn encode_rgba(
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
                let a = image[pixel_start + 3];
                // written as BGRA
                self.writer.write_u8(b)?;
                self.writer.write_u8(g)?;
                self.writer.write_u8(r)?;
                self.writer.write_u8(a)?;
            }

            self.write_row_pad(row_pad_size)?;
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
            self.writer.write_u8(val)?;
            self.writer.write_u8(val)?;
            self.writer.write_u8(val)?;
            self.writer.write_u8(0)?;
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
                self.writer.write_u8(image[pixel_start])?;
                // alpha is never written as it's not widely supported
            }

            self.write_row_pad(row_pad_size)?;
        }

        Ok(())
    }

    fn write_row_pad(&mut self, row_pad_size: u32) -> io::Result<()> {
        for _ in 0..row_pad_size {
            self.writer.write_u8(0)?;
        }

        Ok(())
    }
}

impl<'a, W: Write> ImageEncoder for BMPEncoder<'a, W> {
    fn write_image(
        mut self,
        buf: &[u8],
        width: u32,
        height: u32,
        color_type: color::ColorType,
    ) -> ImageResult<()> {
        self.encode(buf, width, height, color_type)
    }
}

fn get_unsupported_error_message(c: color::ColorType) -> String {
    format!(
        "Unsupported color type {:?}.  Supported types: RGB(8), RGBA(8), Gray(8), GrayA(8).",
        c
    )
}

/// Returns a tuple representing: (dib header size, written pixel size, palette color count).
fn get_pixel_info(c: color::ColorType) -> io::Result<(u32, u32, u32)> {
    let sizes = match c {
        color::ColorType::Rgb8 => (BITMAPINFOHEADER_SIZE, 3, 0),
        color::ColorType::Rgba8 => (BITMAPV4HEADER_SIZE, 4, 0),
        color::ColorType::L8 => (BITMAPINFOHEADER_SIZE, 1, 256),
        color::ColorType::La8 => (BITMAPINFOHEADER_SIZE, 1, 256),
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
    use super::super::BmpDecoder;
    use super::BMPEncoder;
    use crate::color::ColorType;
    use crate::image::ImageDecoder;
    use std::io::Cursor;

    fn round_trip_image(image: &[u8], width: u32, height: u32, c: ColorType) -> Vec<u8> {
        let mut encoded_data = Vec::new();
        {
            let mut encoder = BMPEncoder::new(&mut encoded_data);
            encoder
                .encode(&image, width, height, c)
                .expect("could not encode image");
        }

        let decoder = BmpDecoder::new(Cursor::new(&encoded_data)).expect("failed to decode");

        let mut buf = vec![0; decoder.total_bytes() as usize];
        decoder.read_image(&mut buf).expect("failed to decode");
        buf
    }

    #[test]
    fn round_trip_single_pixel_rgb() {
        let image = [255u8, 0, 0]; // single red pixel
        let decoded = round_trip_image(&image, 1, 1, ColorType::Rgb8);
        assert_eq!(3, decoded.len());
        assert_eq!(255, decoded[0]);
        assert_eq!(0, decoded[1]);
        assert_eq!(0, decoded[2]);
    }

    #[test]
    fn huge_files_return_error() {
        let mut encoded_data = Vec::new();
        let image = vec![0u8; 3 * 40_000 * 40_000]; // 40_000x40_000 pixels, 3 bytes per pixel, allocated on the heap
        let mut encoder = BMPEncoder::new(&mut encoded_data);
        let result = encoder.encode(&image, 40_000, 40_000, ColorType::Rgb8);
        assert!(result.is_err());
    }

    #[test]
    fn round_trip_single_pixel_rgba() {
        let image = [1, 2, 3, 4];
        let decoded = round_trip_image(&image, 1, 1, ColorType::Rgba8);
        assert_eq!(&decoded[..], &image[..]);
    }

    #[test]
    fn round_trip_3px_rgb() {
        let image = [0u8; 3 * 3 * 3]; // 3x3 pixels, 3 bytes per pixel
        let _decoded = round_trip_image(&image, 3, 3, ColorType::Rgb8);
    }

    #[test]
    fn round_trip_gray() {
        let image = [0u8, 1, 2]; // 3 pixels
        let decoded = round_trip_image(&image, 3, 1, ColorType::L8);
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
        let decoded = round_trip_image(&image, 1, 3, ColorType::La8);
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
