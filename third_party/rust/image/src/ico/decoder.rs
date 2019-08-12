use byteorder::{LittleEndian, ReadBytesExt};
use std::io::{self, Cursor, Read, Seek, SeekFrom};
use std::marker::PhantomData;
use std::mem;

use color::ColorType;
use image::{ImageDecoder, ImageError, ImageResult};

use self::InnerDecoder::*;
use bmp::BMPDecoder;
use png::PNGDecoder;

// http://www.w3.org/TR/PNG-Structure.html
// The first eight bytes of a PNG file always contain the following (decimal) values:
const PNG_SIGNATURE: [u8; 8] = [137, 80, 78, 71, 13, 10, 26, 10];

/// An ico decoder
pub struct ICODecoder<R: Read> {
    selected_entry: DirEntry,
    inner_decoder: InnerDecoder<R>,
}

enum InnerDecoder<R: Read> {
    BMP(BMPDecoder<R>),
    PNG(PNGDecoder<R>),
}

#[derive(Clone, Copy, Default)]
struct DirEntry {
    width: u8,
    height: u8,
    color_count: u8,
    reserved: u8,

    num_color_planes: u16,
    bits_per_pixel: u16,

    image_length: u32,
    image_offset: u32,
}

impl<R: Read + Seek> ICODecoder<R> {
    /// Create a new decoder that decodes from the stream ```r```
    pub fn new(mut r: R) -> ImageResult<ICODecoder<R>> {
        let entries = read_entries(&mut r)?;
        let entry = best_entry(entries)?;
        let decoder = entry.decoder(r)?;

        Ok(ICODecoder {
            selected_entry: entry,
            inner_decoder: decoder,
        })
    }
}

fn read_entries<R: Read>(r: &mut R) -> ImageResult<Vec<DirEntry>> {
    let _reserved = r.read_u16::<LittleEndian>()?;
    let _type = r.read_u16::<LittleEndian>()?;
    let count = r.read_u16::<LittleEndian>()?;
    (0..count).map(|_| read_entry(r)).collect()
}

fn read_entry<R: Read>(r: &mut R) -> ImageResult<DirEntry> {
    let mut entry = DirEntry::default();

    entry.width = r.read_u8()?;
    entry.height = r.read_u8()?;
    entry.color_count = r.read_u8()?;
    // Reserved value (not used)
    entry.reserved = r.read_u8()?;

    // This may be either the number of color planes (0 or 1), or the horizontal coordinate
    // of the hotspot for CUR files.
    entry.num_color_planes = r.read_u16::<LittleEndian>()?;
    if entry.num_color_planes > 256 {
        return Err(ImageError::FormatError(
            "ICO image entry has a too large color planes/hotspot value".to_string(),
        ));
    }

    // This may be either the bit depth (may be 0 meaning unspecified),
    // or the vertical coordinate of the hotspot for CUR files.
    entry.bits_per_pixel = r.read_u16::<LittleEndian>()?;
    if entry.bits_per_pixel > 256 {
        return Err(ImageError::FormatError(
            "ICO image entry has a too large bits per pixel/hotspot value".to_string(),
        ));
    }

    entry.image_length = r.read_u32::<LittleEndian>()?;
    entry.image_offset = r.read_u32::<LittleEndian>()?;

    Ok(entry)
}

/// Find the entry with the highest (color depth, size).
fn best_entry(mut entries: Vec<DirEntry>) -> ImageResult<DirEntry> {
    let mut best = entries.pop().ok_or(ImageError::ImageEnd)?;
    let mut best_score = (
        best.bits_per_pixel,
        u32::from(best.real_width()) * u32::from(best.real_height()),
    );

    for entry in entries {
        let score = (
            entry.bits_per_pixel,
            u32::from(entry.real_width()) * u32::from(entry.real_height()),
        );
        if score > best_score {
            best = entry;
            best_score = score;
        }
    }
    Ok(best)
}

impl DirEntry {
    fn real_width(&self) -> u16 {
        match self.width {
            0 => 256,
            w => u16::from(w),
        }
    }

    fn real_height(&self) -> u16 {
        match self.height {
            0 => 256,
            h => u16::from(h),
        }
    }

    fn matches_dimensions(&self, width: u64, height: u64) -> bool {
        u64::from(self.real_width()) == width && u64::from(self.real_height()) == height
    }

    fn seek_to_start<R: Read + Seek>(&self, r: &mut R) -> ImageResult<()> {
        r.seek(SeekFrom::Start(u64::from(self.image_offset)))?;
        Ok(())
    }

    fn is_png<R: Read + Seek>(&self, r: &mut R) -> ImageResult<bool> {
        self.seek_to_start(r)?;

        // Read the first 8 bytes to sniff the image.
        let mut signature = [0u8; 8];
        r.read_exact(&mut signature)?;

        Ok(signature == PNG_SIGNATURE)
    }

    fn decoder<R: Read + Seek>(&self, mut r: R) -> ImageResult<InnerDecoder<R>> {
        let is_png = self.is_png(&mut r)?;
        self.seek_to_start(&mut r)?;

        if is_png {
            Ok(PNG(PNGDecoder::new(r)?))
        } else {
            Ok(BMP(BMPDecoder::new_with_ico_format(r)?))
        }
    }
}

/// Wrapper struct around a `Cursor<Vec<u8>>`
pub struct IcoReader<R>(Cursor<Vec<u8>>, PhantomData<R>);
impl<R> Read for IcoReader<R> {
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

impl<'a, R: 'a + Read + Seek> ImageDecoder<'a> for ICODecoder<R> {
    type Reader = IcoReader<R>;

    fn dimensions(&self) -> (u64, u64) {
        match self.inner_decoder {
            BMP(ref decoder) => decoder.dimensions(),
            PNG(ref decoder) => decoder.dimensions(),
        }
    }

    fn colortype(&self) -> ColorType {
        match self.inner_decoder {
            BMP(ref decoder) => decoder.colortype(),
            PNG(ref decoder) => decoder.colortype(),
        }
    }

    fn into_reader(self) -> ImageResult<Self::Reader> {
        Ok(IcoReader(Cursor::new(self.read_image()?), PhantomData))
    }

    fn read_image(self) -> ImageResult<Vec<u8>> {
        match self.inner_decoder {
            PNG(decoder) => {
                if self.selected_entry.image_length < PNG_SIGNATURE.len() as u32 {
                    return Err(ImageError::FormatError(
                        "Entry specified a length that is shorter than PNG header!".to_string(),
                    ));
                }

                // Check if the image dimensions match the ones in the image data.
                let (width, height) = decoder.dimensions();
                if !self.selected_entry.matches_dimensions(width, height) {
                    return Err(ImageError::FormatError(
                        "Entry and PNG dimensions do not match!".to_string(),
                    ));
                }

                // Embedded PNG images can only be of the 32BPP RGBA format.
                // https://blogs.msdn.microsoft.com/oldnewthing/20101022-00/?p=12473/
                let color_type = decoder.colortype();
                if let ColorType::RGBA(8) = color_type {
                } else {
                    return Err(ImageError::FormatError(
                        "The PNG is not in RGBA format!".to_string(),
                    ));
                }

                decoder.read_image()
            }
            BMP(mut decoder) => {
                let (width, height) = decoder.dimensions();
                if !self.selected_entry.matches_dimensions(width, height) {
                    return Err(ImageError::FormatError(
                        "Entry({:?}) and BMP({:?}) dimensions do not match!".to_string(),
                    ));
                }

                // The ICO decoder needs an alpha channel to apply the AND mask.
                if decoder.colortype() != ColorType::RGBA(8) {
                    return Err(ImageError::UnsupportedError(
                        "Unsupported color type".to_string(),
                    ));
                }

                let mut pixel_data = decoder.read_image_data()?;

                // If there's an AND mask following the image, read and apply it.
                let r = decoder.reader();
                let mask_start = r.seek(SeekFrom::Current(0))?;
                let mask_end =
                    u64::from(self.selected_entry.image_offset + self.selected_entry.image_length);
                let mask_length = mask_end - mask_start;

                if mask_length > 0 {
                    // A mask row contains 1 bit per pixel, padded to 4 bytes.
                    let mask_row_bytes = ((width + 31) / 32) * 4;
                    let expected_length = u64::from(mask_row_bytes * height);
                    if mask_length < expected_length {
                        return Err(ImageError::ImageEnd);
                    }

                    for y in 0..height {
                        let mut x = 0;
                        for _ in 0..mask_row_bytes {
                            // Apply the bits of each byte until we reach the end of the row.
                            let mask_byte = r.read_u8()?;
                            for bit in (0..8).rev() {
                                if x >= width {
                                    break;
                                }
                                if mask_byte & (1 << bit) != 0 {
                                    // Set alpha channel to transparent.
                                    pixel_data[((height - y - 1) * width + x) as usize * 4 + 3] = 0;
                                }
                                x += 1;
                            }
                        }
                    }
                }
                Ok(pixel_data)
            }
        }
    }
}
