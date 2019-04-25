mod stream;

pub use self::stream::{StreamingDecoder, Decoded, DecodingError};
use self::stream::{CHUNCK_BUFFER_SIZE, get_info};

use std::mem;
use std::borrow;
use std::io::{Read, Write, BufReader, BufRead};

use traits::{HasParameters, Parameter};
use common::{ColorType, BitDepth, Info, Transformations};
use filter::{unfilter, FilterType};
use chunk::IDAT;
use utils;

/*
pub enum InterlaceHandling {
    /// Outputs the raw rows
    RawRows,
    /// Fill missing the pixels from the existing ones
    Rectangle,
    /// Only fill the needed pixels
    Sparkle
}

impl Parameter<Reader> for InterlaceHandling {
    fn set_param(self, this: &mut Reader) {
        this.color_output = self
    }
}*/


impl<R: Read> Parameter<Decoder<R>> for Transformations {
    fn set_param(self, this: &mut Decoder<R>) {
        this.transform = self
    }
}


/// Output info
pub struct OutputInfo {
    pub width: u32,
    pub height: u32,
    pub color_type: ColorType,
    pub bit_depth: BitDepth,
    pub line_size: usize,
}

impl OutputInfo {
    /// Returns the size needed to hold a decoded frame
    pub fn buffer_size(&self) -> usize {
        self.line_size * self.height as usize
    }
}

#[derive(Clone, Copy, Debug)]
pub struct Limits {
    /// max number of pixels: `width * height` (default: 67M = 2<sup>26</sup>)
    pub pixels: u64,
}

impl Default for Limits {
    fn default() -> Limits {
        Limits {
            pixels: 1 << 26,
        }
    }
}

/// PNG Decoder
pub struct Decoder<R: Read> {
    /// Reader
    r: R,
    /// Output transformations
    transform: Transformations,
    /// Images that are considered too big
    limits: Limits,
}

impl<R: Read> Decoder<R> {
    pub fn new(r: R) -> Decoder<R> {
        Decoder::new_with_limits(r, Limits::default())
    }
    
    pub fn new_with_limits(r: R, l: Limits) -> Decoder<R> {
        Decoder {
            r: r,
            transform: ::Transformations::EXPAND | ::Transformations::SCALE_16 | ::Transformations::STRIP_16,
            limits: l,
        }
    }

    /// Images that are considered too big
    ///
    /// ```
    /// use std::fs::File;
    /// use png::{Decoder, Limits};
    /// // This image is 32x32 pixels, so it's more than four pixels in size.
    /// let mut limits = Limits::default();
    /// limits.pixels = 4;
    /// let mut decoder = Decoder::new_with_limits(File::open("tests/pngsuite/basi0g01.png").unwrap(), limits);
    /// assert!(decoder.read_info().is_err());
    /// // This image is 32x32 pixels, so it's exactly 1024 pixels in size.
    /// let mut limits = Limits::default();
    /// limits.pixels = 1024;
    /// let mut decoder = Decoder::new_with_limits(File::open("tests/pngsuite/basi0g01.png").unwrap(), limits);
    /// assert!(decoder.read_info().is_ok());
    /// ```
    pub fn set_limits(&mut self, limits: Limits) {
        self.limits = limits;
    }

    /// Reads all meta data until the first IDAT chunk
    pub fn read_info(self) -> Result<(OutputInfo, Reader<R>), DecodingError> {
        let mut r = Reader::new(self.r, StreamingDecoder::new(), self.transform);
        try!(r.init());
        let (ct, bits) = r.output_color_type();
        let info = {
            let info = r.info();
            OutputInfo {
                width: info.width,
                height: info.height,
                color_type: ct,
                bit_depth: bits,
                line_size: r.output_line_size(info.width),
            }
        };
        let (width, height, pixels) = (info.width as u64, info.height as u64, self.limits.pixels);
        if width.checked_mul(height).map(|p| p > pixels).unwrap_or(true) {
            // DecodingError::Other is used for backwards compatibility.
            // In the next major version, add a variant for this.
            return Err(DecodingError::Other(borrow::Cow::Borrowed("pixels limit exceeded")));
        }
        Ok((info, r))
    }
}

impl<R: Read> HasParameters for Decoder<R> {}

struct ReadDecoder<R: Read> {
    reader: BufReader<R>,
    decoder: StreamingDecoder,
    at_eof: bool
}

impl<R: Read> ReadDecoder<R> {
    /// Returns the next decoded chunk. If the chunk is an ImageData chunk, its contents are written
    /// into image_data.
    fn decode_next(&mut self, image_data: &mut Vec<u8>) -> Result<Option<Decoded>, DecodingError> {
        while !self.at_eof {
            let (consumed, result) = {
                let buf = try!(self.reader.fill_buf());
                if buf.is_empty() {
                    return Err(DecodingError::Format(
                        "unexpected EOF".into()
                    ))
                }
                try!(self.decoder.update(buf, image_data))
            };
            self.reader.consume(consumed);
            match result {
                Decoded::Nothing => (),
                Decoded::ImageEnd => self.at_eof = true,
                result => return Ok(Some(result))
            }
        }
        Ok(None)
    }

    fn info(&self) -> Option<&Info> {
        get_info(&self.decoder)
    }
}

/// PNG reader (mostly high-level interface)
///
/// Provides a high level that iterates over lines or whole images.
pub struct Reader<R: Read> {
    decoder: ReadDecoder<R>,
    bpp: usize,
    rowlen: usize,
    adam7: Option<utils::Adam7Iterator>,
    /// Previous raw line
    prev: Vec<u8>,
    /// Current raw line
    current: Vec<u8>,
    /// Output transformations
    transform: Transformations,
    /// Processed line
    processed: Vec<u8>
}

macro_rules! get_info(
    ($this:expr) => {
        $this.decoder.info().unwrap()
    }
);

impl<R: Read> Reader<R> {
    /// Creates a new PNG reader
    fn new(r: R, d: StreamingDecoder, t: Transformations) -> Reader<R> {
        Reader {
            decoder: ReadDecoder {
                reader: BufReader::with_capacity(CHUNCK_BUFFER_SIZE, r),
                decoder: d,
                at_eof: false
            },
            bpp: 0,
            rowlen: 0,
            adam7: None,
            prev: Vec::new(),
            current: Vec::new(),
            transform: t,
            processed: Vec::new()
        }
    }

    /// Reads all meta data until the first IDAT chunk
    fn init(&mut self) -> Result<(), DecodingError> {
        use Decoded::*;
        if self.decoder.info().is_some() {
            Ok(())
        } else {
            loop {
                match try!(self.decoder.decode_next(&mut Vec::new())) {
                    Some(ChunkBegin(_, IDAT)) => break,
                    None => return Err(DecodingError::Format(
                        "IDAT chunk missing".into()
                    )),
                    _ => (),
                }
            }
            {
                let info = match self.decoder.info() {
                    Some(info) => info,
                    None => return Err(DecodingError::Format(
                      "IHDR chunk missing".into()
                    ))
                };
                self.bpp = info.bytes_per_pixel();
                self.rowlen = info.raw_row_length();
                if info.interlaced {
                    self.adam7 = Some(utils::Adam7Iterator::new(info.width, info.height))
                }
            }
            self.allocate_out_buf();
            self.prev = vec![0; self.rowlen];
            Ok(())
        }
    }

    pub fn info(&self) -> &Info {
        get_info!(self)
    }

    /// Decodes the next frame into `buf`
    pub fn next_frame(&mut self, buf: &mut [u8]) -> Result<(), DecodingError> {
        // TODO 16 bit
        let (color_type, _) = self.output_color_type();
        let width = get_info!(self).width;
        if buf.len() < self.output_buffer_size() {
            return Err(DecodingError::Other(
                "supplied buffer is too small to hold the image".into()
            ))
        }
        if get_info!(self).interlaced {
             while let Some((row, adam7)) = try!(self.next_interlaced_row()) {
                 let (pass, line, _) = adam7.unwrap();
                 let bytes = color_type.samples() as u8;
                 utils::expand_pass(buf, width * bytes as u32, row, pass, line, bytes);
             }
        } else {
            let mut len = 0;
            while let Some(row) = try!(self.next_row()) {
                len += try!((&mut buf[len..]).write(row));
            }
        }
        Ok(())
    }

    /// Returns the next processed row of the image
    pub fn next_row(&mut self) -> Result<Option<&[u8]>, DecodingError> {
        self.next_interlaced_row().map(|v| v.map(|v| v.0))
    }

    /// Returns the next processed row of the image
    pub fn next_interlaced_row(&mut self) -> Result<Option<(&[u8], Option<(u8, u32, u32)>)>, DecodingError> {
        use common::ColorType::*;
        let transform = self.transform;
        if transform == ::Transformations::IDENTITY {
            self.next_raw_interlaced_row()
        } else {
            // swap buffer to circumvent borrow issues
            let mut buffer = mem::replace(&mut self.processed, Vec::new());
            let (got_next, adam7) = if let Some((row, adam7)) = try!(self.next_raw_interlaced_row()) {
                try!((&mut buffer[..]).write(row));
                (true, adam7)
            } else {
                (false, None)
            };
            // swap back
            let _ = mem::replace(&mut self.processed, buffer);
            if got_next {
                let (color_type, bit_depth, trns) = {
                    let info = get_info!(self);
                    (info.color_type, info.bit_depth as u8, info.trns.is_some())
                };
                let output_buffer = if let Some((_, _, width)) = adam7 {
                    let width = self.line_size(width);
                    &mut self.processed[..width]
                } else {
                    &mut *self.processed
                };
                let mut len = output_buffer.len();
                if transform.contains(::Transformations::EXPAND) {
                    match color_type {
                        Indexed => {
                            expand_paletted(output_buffer, get_info!(self))?
                        }
                        Grayscale | GrayscaleAlpha if bit_depth < 8 => expand_gray_u8(
                            output_buffer, get_info!(self)
                        ),
                        Grayscale | RGB if trns => {
                            let channels = color_type.samples();
                            let trns = get_info!(self).trns.as_ref().unwrap();
                            if bit_depth == 8 {
                                utils::expand_trns_line(output_buffer, &*trns, channels);
                            } else {
                                utils::expand_trns_line16(output_buffer, &*trns, channels);
                            }
                        },
                        _ => ()
                    }
                }
                if bit_depth == 16 && transform.intersects(::Transformations::SCALE_16 | ::Transformations::STRIP_16) {
                    len /= 2;
                    for i in 0..len {
                        output_buffer[i] = output_buffer[2 * i];
                    }
                }
                Ok(Some((
                    &output_buffer[..len],
                    adam7
                )))
            } else {
                Ok(None)
            }
        }
    }

    /// Returns the color type and the number of bits per sample
    /// of the data returned by `Reader::next_row` and Reader::frames`.
    pub fn output_color_type(&mut self) -> (ColorType, BitDepth) {
        use common::ColorType::*;
        let t = self.transform;
        let info = get_info!(self);
        if t == ::Transformations::IDENTITY {
            (info.color_type, info.bit_depth)
        } else {
            let bits = match info.bit_depth as u8 {
                16 if t.intersects(
                    ::Transformations::SCALE_16 | ::Transformations::STRIP_16
                ) => 8,
                _ if t.contains(::Transformations::EXPAND) => 8,
                n => n
            };
            let color_type = if t.contains(::Transformations::EXPAND) {
                let has_trns = info.trns.is_some();
                match info.color_type {
                    Grayscale if has_trns => GrayscaleAlpha,
                    RGB if has_trns => RGBA,
                    Indexed if has_trns => RGBA,
                    Indexed => RGB,
                    ct => ct
                }
            } else {
                info.color_type
            };
            (color_type, BitDepth::from_u8(bits).unwrap())
        }
    }

    /// Returns the number of bytes required to hold a deinterlaced image frame
    /// that is decoded using the given input transformations.
    pub fn output_buffer_size(&self) -> usize {
        let (width, height) = get_info!(self).size();
        let size = self.output_line_size(width);
        size * height as usize
    }

    /// Returns the number of bytes required to hold a deinterlaced row.
    pub fn output_line_size(&self, width: u32) -> usize {
        let size = self.line_size(width);
        if get_info!(self).bit_depth as u8 == 16 && self.transform.intersects(
            ::Transformations::SCALE_16 | ::Transformations::STRIP_16
        ) {
            size / 2
        } else {
            size
        }
    }

    /// Returns the number of bytes required to decode a deinterlaced row.
    fn line_size(&self, width: u32) -> usize {
        use common::ColorType::*;
        let t = self.transform;
        let info = get_info!(self);
        let trns = info.trns.is_some();
        // TODO 16 bit
        let bits = match info.color_type {
            Indexed if trns && t.contains(::Transformations::EXPAND) => 4 * 8,
            Indexed if t.contains(::Transformations::EXPAND) => 3 * 8,
            RGB if trns && t.contains(::Transformations::EXPAND) => 4 * 8,
            Grayscale if trns && t.contains(::Transformations::EXPAND) => 2 * 8,
            Grayscale if t.contains(::Transformations::EXPAND) => 1 * 8,
            GrayscaleAlpha if t.contains(::Transformations::EXPAND) => 2 * 8,
            // divide by 2 as it will get mutiplied by two later
            _ if info.bit_depth as u8 == 16 => info.bits_per_pixel() / 2,
            _ => info.bits_per_pixel()
        }
        * width as usize
        * if info.bit_depth as u8 == 16 { 2 } else { 1 };
        let len = bits / 8;
        let extra = bits % 8;
        len + match extra { 0 => 0, _ => 1 }
    }

    fn allocate_out_buf(&mut self) {
        let width = get_info!(self).width;
        self.processed = vec![0; self.line_size(width)]
    }

    /// Returns the next raw row of the image
    fn next_raw_interlaced_row(&mut self) -> Result<Option<(&[u8], Option<(u8, u32, u32)>)>, DecodingError> {
        let _ = get_info!(self);
        let bpp = self.bpp;
        let (rowlen, passdata) = if let Some(ref mut adam7) = self.adam7 {
            let last_pass = adam7.current_pass();
            if let Some((pass, line, len)) = adam7.next() {
                let rowlen = get_info!(self).raw_row_length_from_width(len);
                if last_pass != pass {
                    self.prev.clear();
                    for _ in 0..rowlen {
                        self.prev.push(0);
                    }
                }
                (rowlen, Some((pass, line, len)))
            } else {
                return Ok(None)
            }
        } else {
            (self.rowlen, None)
        };
        loop {
            if self.current.len() >= rowlen {
                if let Some(filter) = FilterType::from_u8(self.current[0]) {
                    if let Err(message) = unfilter(filter, bpp, &self.prev[1..rowlen], &mut self.current[1..rowlen]) {
                        return Err(DecodingError::Format(
                            borrow::Cow::Borrowed(message)
                        ))
                    }
                    self.prev[..rowlen].copy_from_slice(&self.current[..rowlen]);
                    self.current.drain(0..rowlen);
                    return Ok(
                        Some((
                            &self.prev[1..rowlen],
                            passdata
                        ))
                    )
                } else {
                    return Err(DecodingError::Format(
                        format!("invalid filter method ({})", self.current[0]).into()
                    ))
                }
            } else {
                let val = try!(self.decoder.decode_next(&mut self.current));
                match val {
                    Some(Decoded::ImageData) => {}
                    None => {
                        if self.current.len() > 0 {
                            return Err(DecodingError::Format(
                              "file truncated".into()
                            ))
                        } else {
                            return Ok(None)
                        }
                    }
                    _ => ()
                }
            }
        }
    }
}

fn expand_paletted(buffer: &mut [u8], info: &Info) -> Result<(), DecodingError> {
    if let Some(palette) = info.palette.as_ref() {
        if let BitDepth::Sixteen = info.bit_depth {
            Err(DecodingError::Format("Bit depth '16' is not valid for paletted images".into()))
        } else {
            let black = [0, 0, 0];
            if let Some(ref trns) = info.trns {
                utils::unpack_bits(buffer, 4, info.bit_depth as u8, |i, chunk| {
                    let (rgb, a) = (
                        palette.get(3*i as usize..3*i as usize+3).unwrap_or(&black),
                        *trns.get(i as usize).unwrap_or(&0xFF)
                    );
                    chunk[0] = rgb[0];
                    chunk[1] = rgb[1];
                    chunk[2] = rgb[2];
                    chunk[3] = a;
                });
            } else {
                utils::unpack_bits(buffer, 3, info.bit_depth as u8, |i, chunk| {
                    let rgb = palette.get(3*i as usize..3*i as usize+3).unwrap_or(&black);
                    chunk[0] = rgb[0];
                    chunk[1] = rgb[1];
                    chunk[2] = rgb[2];
                })
            }
            Ok(())
        }
    } else {
        Err(DecodingError::Format("missing palette".into()))
    }
}

fn expand_gray_u8(buffer: &mut [u8], info: &Info) {
    let rescale = true;
    let scaling_factor = if rescale {
        (255)/((1u16 << info.bit_depth as u8) - 1) as u8
    } else {
        1
    };
    if let Some(ref trns) = info.trns {
        utils::unpack_bits(buffer, 2, info.bit_depth as u8, |pixel, chunk| {
            if pixel == trns[0] {
                chunk[1] = 0
            } else {
                chunk[1] = 0xFF
            }
            chunk[0] = pixel * scaling_factor
        })
    } else {
        utils::unpack_bits(buffer, 1, info.bit_depth as u8, |val, chunk| {
            chunk[0] = val * scaling_factor
        })
    }
}
/*
#[cfg(test)]
mod test {
    extern crate test;

    use std::fs::File;
    use std::io::Read;

    use super::Decoder;
    use HasParameters;

    #[bench]
    fn bench_big(b: &mut test::Bencher) {
        let mut data = Vec::new();
        File::open("tests/pngsuite/PngSuite.png").unwrap().read_to_end(&mut data).unwrap();
        let mut decoder = Decoder::new(&*data);
        decoder.set(::Transformations::IDENTITY);
        let (info, _) = decoder.read_info().unwrap();
        let mut image = vec![0; info.buffer_size()];
        b.iter(|| {
            let mut decoder = Decoder::new(&*data);
            decoder.set(::Transformations::IDENTITY);
            let (_, mut decoder) = decoder.read_info().unwrap();
            test::black_box(decoder.next_frame(&mut image)).unwrap();
        });
        b.bytes = info.buffer_size() as u64
    }
}
*/
