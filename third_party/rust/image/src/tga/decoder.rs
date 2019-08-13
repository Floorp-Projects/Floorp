use byteorder::{LittleEndian, ReadBytesExt};
use std::io;
use std::io::{Read, Seek};

use color::ColorType;
use image::{ImageDecoder, ImageError, ImageReadBuffer, ImageResult};

enum ImageType {
    NoImageData = 0,
    /// Uncompressed images
    RawColorMap = 1,
    RawTrueColor = 2,
    RawGrayScale = 3,
    /// Run length encoded images
    RunColorMap = 9,
    RunTrueColor = 10,
    RunGrayScale = 11,
    Unknown,
}

impl ImageType {
    /// Create a new image type from a u8
    fn new(img_type: u8) -> ImageType {
        match img_type {
            0 => ImageType::NoImageData,

            1 => ImageType::RawColorMap,
            2 => ImageType::RawTrueColor,
            3 => ImageType::RawGrayScale,

            9 => ImageType::RunColorMap,
            10 => ImageType::RunTrueColor,
            11 => ImageType::RunGrayScale,

            _ => ImageType::Unknown,
        }
    }

    /// Check if the image format uses colors as opposed to gray scale
    fn is_color(&self) -> bool {
        match *self {
            ImageType::RawColorMap
            | ImageType::RawTrueColor
            | ImageType::RunTrueColor
            | ImageType::RunColorMap => true,
            _ => false,
        }
    }

    /// Does the image use a color map
    fn is_color_mapped(&self) -> bool {
        match *self {
            ImageType::RawColorMap | ImageType::RunColorMap => true,
            _ => false,
        }
    }

    /// Is the image run length encoded
    fn is_encoded(&self) -> bool {
        match *self {
            ImageType::RunColorMap | ImageType::RunTrueColor | ImageType::RunGrayScale => true,
            _ => false,
        }
    }
}

/// Header used by TGA image files
#[derive(Debug)]
struct Header {
    id_length: u8,      // length of ID string
    map_type: u8,       // color map type
    image_type: u8,     // image type code
    map_origin: u16,    // starting index of map
    map_length: u16,    // length of map
    map_entry_size: u8, // size of map entries in bits
    x_origin: u16,      // x-origin of image
    y_origin: u16,      // y-origin of image
    image_width: u16,   // width of image
    image_height: u16,  // height of image
    pixel_depth: u8,    // bits per pixel
    image_desc: u8,     // image descriptor
}

impl Header {
    /// Create a header with all values set to zero
    fn new() -> Header {
        Header {
            id_length: 0,
            map_type: 0,
            image_type: 0,
            map_origin: 0,
            map_length: 0,
            map_entry_size: 0,
            x_origin: 0,
            y_origin: 0,
            image_width: 0,
            image_height: 0,
            pixel_depth: 0,
            image_desc: 0,
        }
    }

    /// Load the header with values from the reader
    fn from_reader(r: &mut dyn Read) -> ImageResult<Header> {
        Ok(Header {
            id_length: r.read_u8()?,
            map_type: r.read_u8()?,
            image_type: r.read_u8()?,
            map_origin: r.read_u16::<LittleEndian>()?,
            map_length: r.read_u16::<LittleEndian>()?,
            map_entry_size: r.read_u8()?,
            x_origin: r.read_u16::<LittleEndian>()?,
            y_origin: r.read_u16::<LittleEndian>()?,
            image_width: r.read_u16::<LittleEndian>()?,
            image_height: r.read_u16::<LittleEndian>()?,
            pixel_depth: r.read_u8()?,
            image_desc: r.read_u8()?,
        })
    }
}

struct ColorMap {
    /// sizes in bytes
    start_offset: usize,
    entry_size: usize,
    bytes: Vec<u8>,
}

impl ColorMap {
    pub fn from_reader(
        r: &mut dyn Read,
        start_offset: u16,
        num_entries: u16,
        bits_per_entry: u8,
    ) -> ImageResult<ColorMap> {
        let bytes_per_entry = (bits_per_entry as usize + 7) / 8;

        let mut bytes = vec![0; bytes_per_entry * num_entries as usize];
        r.read_exact(&mut bytes)?;

        Ok(ColorMap {
            entry_size: bytes_per_entry,
            start_offset: start_offset as usize,
            bytes,
        })
    }

    /// Get one entry from the color map
    pub fn get(&self, index: usize) -> &[u8] {
        let entry = self.start_offset + self.entry_size * index;
        &self.bytes[entry..entry + self.entry_size]
    }
}

/// The representation of a TGA decoder
pub struct TGADecoder<R> {
    r: R,

    width: usize,
    height: usize,
    bytes_per_pixel: usize,
    has_loaded_metadata: bool,

    image_type: ImageType,
    color_type: ColorType,

    header: Header,
    color_map: Option<ColorMap>,

    // Used in read_scanline
    line_read: Option<usize>,
    line_remain_buff: Vec<u8>,
}

impl<R: Read + Seek> TGADecoder<R> {
    /// Create a new decoder that decodes from the stream `r`
    pub fn new(r: R) -> ImageResult<TGADecoder<R>> {
        let mut decoder = TGADecoder {
            r,

            width: 0,
            height: 0,
            bytes_per_pixel: 0,
            has_loaded_metadata: false,

            image_type: ImageType::Unknown,
            color_type: ColorType::Gray(1),

            header: Header::new(),
            color_map: None,

            line_read: None,
            line_remain_buff: Vec::new(),
        };
        decoder.read_metadata()?;
        Ok(decoder)
    }

    fn read_header(&mut self) -> ImageResult<()> {
        self.header = Header::from_reader(&mut self.r)?;
        self.image_type = ImageType::new(self.header.image_type);
        self.width = self.header.image_width as usize;
        self.height = self.header.image_height as usize;
        self.bytes_per_pixel = (self.header.pixel_depth as usize + 7) / 8;
        Ok(())
    }

    fn read_metadata(&mut self) -> ImageResult<()> {
        if !self.has_loaded_metadata {
            self.read_header()?;
            self.read_image_id()?;
            self.read_color_map()?;
            self.read_color_information()?;
            self.has_loaded_metadata = true;
        }
        Ok(())
    }

    /// Loads the color information for the decoder
    ///
    /// To keep things simple, we won't handle bit depths that aren't divisible
    /// by 8 and are less than 32.
    fn read_color_information(&mut self) -> ImageResult<()> {
        if self.header.pixel_depth % 8 != 0 {
            return Err(ImageError::UnsupportedError(
                "Bit depth must be divisible by 8".to_string(),
            ));
        }
        if self.header.pixel_depth > 32 {
            return Err(ImageError::UnsupportedError(
                "Bit depth must be less than 32".to_string(),
            ));
        }

        let num_alpha_bits = self.header.image_desc & 0b1111;

        let other_channel_bits = if self.header.map_type != 0 {
            self.header.map_entry_size
        } else {
            if num_alpha_bits > self.header.pixel_depth {
                return Err(ImageError::UnsupportedError(
                    format!("Color format not supported. Alpha bits: {}", num_alpha_bits)
                        .to_string(),
                ));
            }

            self.header.pixel_depth - num_alpha_bits
        };
        let color = self.image_type.is_color();

        match (num_alpha_bits, other_channel_bits, color) {
            // really, the encoding is BGR and BGRA, this is fixed
            // up with `TGADecoder::reverse_encoding`.
            (0, 32, true) => self.color_type = ColorType::RGBA(8),
            (8, 24, true) => self.color_type = ColorType::RGBA(8),
            (0, 24, true) => self.color_type = ColorType::RGB(8),
            (8, 8, false) => self.color_type = ColorType::GrayA(8),
            (0, 8, false) => self.color_type = ColorType::Gray(8),
            _ => {
                return Err(ImageError::UnsupportedError(
                    format!(
                        "Color format not supported. Bit depth: {}, Alpha bits: {}",
                        other_channel_bits, num_alpha_bits
                    ).to_string(),
                ))
            }
        }
        Ok(())
    }

    /// Read the image id field
    ///
    /// We're not interested in this field, so this function skips it if it
    /// is present
    fn read_image_id(&mut self) -> ImageResult<()> {
        try!(
            self.r
                .seek(io::SeekFrom::Current(i64::from(self.header.id_length)))
        );
        Ok(())
    }

    fn read_color_map(&mut self) -> ImageResult<()> {
        if self.header.map_type == 1 {
            self.color_map = Some(try!(ColorMap::from_reader(
                &mut self.r,
                self.header.map_origin,
                self.header.map_length,
                self.header.map_entry_size
            )));
        }
        Ok(())
    }

    /// Expands indices into its mapped color
    fn expand_color_map(&self, pixel_data: &[u8]) -> Vec<u8> {
        #[inline]
        fn bytes_to_index(bytes: &[u8]) -> usize {
            let mut result = 0usize;
            for byte in bytes.iter() {
                result = result << 8 | *byte as usize;
            }
            result
        }

        let bytes_per_entry = (self.header.map_entry_size as usize + 7) / 8;
        let mut result = Vec::with_capacity(self.width * self.height * bytes_per_entry);

        let color_map = match self.color_map {
            Some(ref color_map) => color_map,
            None => unreachable!(),
        };

        for chunk in pixel_data.chunks(self.bytes_per_pixel) {
            let index = bytes_to_index(chunk);
            result.extend(color_map.get(index).iter().cloned());
        }

        result
    }

    fn read_image_data(&mut self) -> ImageResult<Vec<u8>> {
        // read the pixels from the data region
        let mut pixel_data = if self.image_type.is_encoded() {
            try!(self.read_all_encoded_data())
        } else {
            let num_raw_bytes = self.width * self.height * self.bytes_per_pixel;
            let mut buf = vec![0; num_raw_bytes];
            self.r.by_ref().read_exact(&mut buf)?;
            buf
        };

        // expand the indices using the color map if necessary
        if self.image_type.is_color_mapped() {
            pixel_data = self.expand_color_map(&pixel_data)
        }

        self.reverse_encoding(&mut pixel_data);

        self.flip_vertically(&mut pixel_data);

        Ok(pixel_data)
    }

    /// Reads a run length encoded data for given number of bytes
    fn read_encoded_data(&mut self, num_bytes: usize) -> io::Result<Vec<u8>> {
        let mut pixel_data = Vec::with_capacity(num_bytes);

        while pixel_data.len() < num_bytes {
            let run_packet = self.r.read_u8()?;
            // If the highest bit in `run_packet` is set, then we repeat pixels
            //
            // Note: the TGA format adds 1 to both counts because having a count
            // of 0 would be pointless.
            if (run_packet & 0x80) != 0 {
                // high bit set, so we will repeat the data
                let repeat_count = ((run_packet & !0x80) + 1) as usize;
                let mut data = Vec::with_capacity(self.bytes_per_pixel);
                try!(
                    self.r
                        .by_ref()
                        .take(self.bytes_per_pixel as u64)
                        .read_to_end(&mut data)
                );
                for _ in 0usize..repeat_count {
                    pixel_data.extend(data.iter().cloned());
                }
            } else {
                // not set, so `run_packet+1` is the number of non-encoded pixels
                let num_raw_bytes = (run_packet + 1) as usize * self.bytes_per_pixel;
                try!(
                    self.r
                        .by_ref()
                        .take(num_raw_bytes as u64)
                        .read_to_end(&mut pixel_data)
                );
            }
        }

        Ok(pixel_data)
    }

    /// Reads a run length encoded packet
    fn read_all_encoded_data(&mut self) -> ImageResult<Vec<u8>> {
        let num_bytes = self.width * self.height * self.bytes_per_pixel;

        Ok(self.read_encoded_data(num_bytes)?)
    }

    /// Reads a run length encoded line
    fn read_encoded_line(&mut self) -> io::Result<Vec<u8>> {
        let line_num_bytes = self.width * self.bytes_per_pixel;
        let remain_len = self.line_remain_buff.len();

        if remain_len >= line_num_bytes {
            let remain_buf = self.line_remain_buff.clone();

            self.line_remain_buff = remain_buf[line_num_bytes..].to_vec();
            return Ok(remain_buf[0..line_num_bytes].to_vec());
        }

        let num_bytes = line_num_bytes - remain_len;

        let line_data = self.read_encoded_data(num_bytes)?;

        let mut pixel_data = Vec::with_capacity(line_num_bytes);
        pixel_data.append(&mut self.line_remain_buff);
        pixel_data.extend_from_slice(&line_data[..num_bytes]);

        // put the remain data to line_remain_buff
        self.line_remain_buff = line_data[num_bytes..].to_vec();

        Ok(pixel_data)
    }

    /// Reverse from BGR encoding to RGB encoding
    ///
    /// TGA files are stored in the BGRA encoding. This function swaps
    /// the blue and red bytes in the `pixels` array.
    fn reverse_encoding(&mut self, pixels: &mut [u8]) {
        // We only need to reverse the encoding of color images
        match self.color_type {
            ColorType::RGB(8) | ColorType::RGBA(8) => {
                for chunk in pixels.chunks_mut(self.bytes_per_pixel) {
                    chunk.swap(0, 2);
                }
            }
            _ => {}
        }
    }

    /// Flip the image vertically depending on the screen origin bit
    ///
    /// The bit in position 5 of the image descriptor byte is the screen origin bit.
    /// If it's 1, the origin is in the top left corner.
    /// If it's 0, the origin is in the bottom left corner.
    /// This function checks the bit, and if it's 0, flips the image vertically.
    fn flip_vertically(&mut self, pixels: &mut [u8]) {
        if self.is_flipped_vertically() {
            let num_bytes = pixels.len();

            let width_bytes = num_bytes / self.height;

            // Flip the image vertically.
            for vertical_index in 0..(self.height / 2) {
                let vertical_target = (self.height - vertical_index) * width_bytes - width_bytes;

                for horizontal_index in 0..width_bytes {
                    let source = vertical_index * width_bytes + horizontal_index;
                    let target = vertical_target + horizontal_index;

                    pixels.swap(target, source);
                }
            }
        }
    }

    /// Check whether the image is vertically flipped
    ///
    /// The bit in position 5 of the image descriptor byte is the screen origin bit.
    /// If it's 1, the origin is in the top left corner.
    /// If it's 0, the origin is in the bottom left corner.
    /// This function checks the bit, and if it's 0, flips the image vertically.
    fn is_flipped_vertically(&self) -> bool {
        let screen_origin_bit = 0b10_0000 & self.header.image_desc != 0;
        !screen_origin_bit
    }

    fn read_scanline(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        if let Some(line_read) = self.line_read {
            if line_read == self.height {
                return Ok(0);
            }
        }

        // read the pixels from the data region
        let mut pixel_data = if self.image_type.is_encoded() {
            self.read_encoded_line()?
        } else {
            let num_raw_bytes = self.width * self.bytes_per_pixel;
            let mut buf = vec![0; num_raw_bytes];
            self.r.by_ref().read_exact(&mut buf)?;
            buf
        };

        // expand the indices using the color map if necessary
        if self.image_type.is_color_mapped() {
            pixel_data = self.expand_color_map(&pixel_data)
        }
        self.reverse_encoding(&mut pixel_data);

        // copy to the output buffer
        buf[..pixel_data.len()].copy_from_slice(&pixel_data);

        self.line_read = Some(self.line_read.unwrap_or(0) + 1);

        Ok(pixel_data.len())
    }
}

impl<'a, R: 'a + Read + Seek> ImageDecoder<'a> for TGADecoder<R> {
    type Reader = TGAReader<R>;

    fn dimensions(&self) -> (u64, u64) {
        (self.width as u64, self.height as u64)
    }

    fn colortype(&self) -> ColorType {
        self.color_type
    }

    fn scanline_bytes(&self) -> u64 {
        self.row_bytes()
    }

    fn into_reader(self) -> ImageResult<Self::Reader> {
        if self.total_bytes() > usize::max_value() as u64 {
            return Err(ImageError::InsufficientMemory);
        }

        Ok(TGAReader {
            buffer: ImageReadBuffer::new(self.scanline_bytes() as usize, self.total_bytes() as usize),
            decoder: self,
        })
    }

    fn read_image(mut self) -> ImageResult<Vec<u8>> {
        self.read_image_data()
    }
}

pub struct TGAReader<R> {
    buffer: ImageReadBuffer,
    decoder: TGADecoder<R>,
}
impl<R: Read + Seek> Read for TGAReader<R> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let ref mut decoder = &mut self.decoder;
        self.buffer.read(buf, |buf| decoder.read_scanline(buf))
    }
}

