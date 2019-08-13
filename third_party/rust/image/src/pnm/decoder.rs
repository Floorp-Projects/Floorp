use std::io::{self, BufRead, BufReader, Cursor, Read};
use std::str::{self, FromStr};
use std::fmt::Display;
use std::marker::PhantomData;
use std::mem;

use super::{ArbitraryHeader, ArbitraryTuplType, BitmapHeader, GraymapHeader, PixmapHeader};
use super::{HeaderRecord, PNMHeader, PNMSubtype, SampleEncoding};
use color::ColorType;
use image::{ImageDecoder, ImageError, ImageResult};

use byteorder::{BigEndian, ByteOrder, NativeEndian};

/// Dynamic representation, represents all decodable (sample, depth) combinations.
#[derive(Clone, Copy)]
enum TupleType {
    PbmBit,
    BWBit,
    GrayU8,
    GrayU16,
    RGBU8,
    RGBU16,
}

trait Sample {
    fn bytelen(width: u32, height: u32, samples: u32) -> ImageResult<usize>;

    /// It is guaranteed that `bytes.len() == bytelen(width, height, samples)`
    fn from_bytes(bytes: &[u8], width: u32, height: u32, samples: u32)
        -> ImageResult<Vec<u8>>;

    fn from_ascii(reader: &mut dyn Read, width: u32, height: u32, samples: u32)
        -> ImageResult<Vec<u8>>;
}

struct U8;
struct U16;
struct PbmBit;
struct BWBit;

trait DecodableImageHeader {
    fn tuple_type(&self) -> ImageResult<TupleType>;
}

/// PNM decoder
pub struct PNMDecoder<R> {
    reader: BufReader<R>,
    header: PNMHeader,
    tuple: TupleType,
}

impl<R: Read> PNMDecoder<R> {
    /// Create a new decoder that decodes from the stream ```read```
    pub fn new(read: R) -> ImageResult<PNMDecoder<R>> {
        let mut buf = BufReader::new(read);
        let magic = buf.read_magic_constant()?;
        if magic[0] != b'P' {
            return Err(ImageError::FormatError(
                format!("Expected magic constant for pnm, P1 through P7 instead of {:?}", magic),
            ));
        }

        let subtype = match magic[1] {
            b'1' => PNMSubtype::Bitmap(SampleEncoding::Ascii),
            b'2' => PNMSubtype::Graymap(SampleEncoding::Ascii),
            b'3' => PNMSubtype::Pixmap(SampleEncoding::Ascii),
            b'4' => PNMSubtype::Bitmap(SampleEncoding::Binary),
            b'5' => PNMSubtype::Graymap(SampleEncoding::Binary),
            b'6' => PNMSubtype::Pixmap(SampleEncoding::Binary),
            b'7' => PNMSubtype::ArbitraryMap,
            _ => {
                return Err(ImageError::FormatError(
                    format!("Expected magic constant for pnm, P1 through P7 instead of {:?}", magic),
                ));
            }
        };

        match subtype {
            PNMSubtype::Bitmap(enc) => PNMDecoder::read_bitmap_header(buf, enc),
            PNMSubtype::Graymap(enc) => PNMDecoder::read_graymap_header(buf, enc),
            PNMSubtype::Pixmap(enc) => PNMDecoder::read_pixmap_header(buf, enc),
            PNMSubtype::ArbitraryMap => PNMDecoder::read_arbitrary_header(buf),
        }
    }

    /// Extract the reader and header after an image has been read.
    pub fn into_inner(self) -> (R, PNMHeader) {
        (self.reader.into_inner(), self.header)
    }

    fn read_bitmap_header(
        mut reader: BufReader<R>,
        encoding: SampleEncoding,
    ) -> ImageResult<PNMDecoder<R>> {
        let header = reader.read_bitmap_header(encoding)?;
        Ok(PNMDecoder {
            reader,
            tuple: TupleType::PbmBit,
            header: PNMHeader {
                decoded: HeaderRecord::Bitmap(header),
                encoded: None,
            },
        })
    }

    fn read_graymap_header(
        mut reader: BufReader<R>,
        encoding: SampleEncoding,
    ) -> ImageResult<PNMDecoder<R>> {
        let header = reader.read_graymap_header(encoding)?;
        let tuple_type = header.tuple_type()?;
        Ok(PNMDecoder {
            reader,
            tuple: tuple_type,
            header: PNMHeader {
                decoded: HeaderRecord::Graymap(header),
                encoded: None,
            },
        })
    }

    fn read_pixmap_header(
        mut reader: BufReader<R>,
        encoding: SampleEncoding,
    ) -> ImageResult<PNMDecoder<R>> {
        let header = reader.read_pixmap_header(encoding)?;
        let tuple_type = header.tuple_type()?;
        Ok(PNMDecoder {
            reader,
            tuple: tuple_type,
            header: PNMHeader {
                decoded: HeaderRecord::Pixmap(header),
                encoded: None,
            },
        })
    }

    fn read_arbitrary_header(mut reader: BufReader<R>) -> ImageResult<PNMDecoder<R>> {
        let header = reader.read_arbitrary_header()?;
        let tuple_type = header.tuple_type()?;
        Ok(PNMDecoder {
            reader,
            tuple: tuple_type,
            header: PNMHeader {
                decoded: HeaderRecord::Arbitrary(header),
                encoded: None,
            },
        })
    }
}

trait HeaderReader: BufRead {
    /// Reads the two magic constant bytes
    fn read_magic_constant(&mut self) -> ImageResult<[u8; 2]> {
        let mut magic: [u8; 2] = [0, 0];
        self.read_exact(&mut magic)
            .map_err(ImageError::IoError)?;
        Ok(magic)
    }

    /// Reads a string as well as a single whitespace after it, ignoring comments
    fn read_next_string(&mut self) -> ImageResult<String> {
        let mut bytes = Vec::new();

        // pair input bytes with a bool mask to remove comments
        let mark_comments = self.bytes().scan(true, |partof, read| {
            let byte = match read {
                Err(err) => return Some((*partof, Err(err))),
                Ok(byte) => byte,
            };
            let cur_enabled = *partof && byte != b'#';
            let next_enabled = cur_enabled || (byte == b'\r' || byte == b'\n');
            *partof = next_enabled;
            Some((cur_enabled, Ok(byte)))
        });

        for (_, byte) in mark_comments.filter(|ref e| e.0) {
            match byte {
                Ok(b'\t') | Ok(b'\n') | Ok(b'\x0b') | Ok(b'\x0c') | Ok(b'\r') | Ok(b' ') => {
                    if !bytes.is_empty() {
                        break; // We're done as we already have some content
                    }
                }
                Ok(byte) if !byte.is_ascii() => {
                    return Err(ImageError::FormatError(
                        format!("Non ascii character {} in header", byte),
                    ));
                },
                Ok(byte) => {
                    bytes.push(byte);
                },
                Err(_) => break,
            }
        }

        if bytes.is_empty() {
            return Err(ImageError::IoError(io::ErrorKind::UnexpectedEof.into()));
        }

        if !bytes.as_slice().is_ascii() {
            // We have only filled the buffer with characters for which `byte.is_ascii()` holds.
            unreachable!("Non ascii character should have returned sooner")
        }

        let string = String::from_utf8(bytes)
            // We checked the precondition ourselves a few lines before, `bytes.as_slice().is_ascii()`.
            .unwrap_or_else(|_| unreachable!("Only ascii characters should be decoded"));

        Ok(string)
    }

    /// Read the next line
    fn read_next_line(&mut self) -> ImageResult<String> {
        let mut buffer = String::new();
        self.read_line(&mut buffer)
            .map_err(ImageError::IoError)?;
        Ok(buffer)
    }

    fn read_next_u32(&mut self) -> ImageResult<u32> {
        let s = self.read_next_string()?;
        s.parse::<u32>()
            .map_err(|err| ImageError::FormatError(
                    format!("Error parsing number {} in preamble: {}", s, err)
                ))
    }

    fn read_bitmap_header(&mut self, encoding: SampleEncoding) -> ImageResult<BitmapHeader> {
        let width = self.read_next_u32()?;
        let height = self.read_next_u32()?;
        Ok(BitmapHeader {
            encoding,
            width,
            height,
        })
    }

    fn read_graymap_header(&mut self, encoding: SampleEncoding) -> ImageResult<GraymapHeader> {
        self.read_pixmap_header(encoding).map(
            |PixmapHeader {
                 encoding,
                 width,
                 height,
                 maxval,
             }| GraymapHeader {
                encoding,
                width,
                height,
                maxwhite: maxval,
            },
        )
    }

    fn read_pixmap_header(&mut self, encoding: SampleEncoding) -> ImageResult<PixmapHeader> {
        let width = self.read_next_u32()?;
        let height = self.read_next_u32()?;
        let maxval = self.read_next_u32()?;
        Ok(PixmapHeader {
            encoding,
            width,
            height,
            maxval,
        })
    }

    fn read_arbitrary_header(&mut self) -> ImageResult<ArbitraryHeader> {
        match self.bytes().next() {
            None => return Err(ImageError::IoError(io::ErrorKind::UnexpectedEof.into())),
            Some(Err(io)) => return Err(ImageError::IoError(io)),
            Some(Ok(b'\n')) => (),
            Some(Ok(c)) => {
                return Err(ImageError::FormatError(
                    format!("Expected newline after P7 magic instead of {}", c),
                ))
            }
        }

        let mut line = String::new();
        let mut height: Option<u32> = None;
        let mut width: Option<u32> = None;
        let mut depth: Option<u32> = None;
        let mut maxval: Option<u32> = None;
        let mut tupltype: Option<String> = None;
        loop {
            line.truncate(0);
            let len = self.read_line(&mut line).map_err(ImageError::IoError)?;
            if len == 0 {
                return Err(ImageError::FormatError(
                    format!("Unexpected end of pnm header"),
                ))
            }
            if line.as_bytes()[0] == b'#' {
                continue;
            }
            if !line.is_ascii() {
                return Err(ImageError::FormatError(
                    "Only ascii characters allowed in pam header".to_string(),
                ));
            }
            #[allow(deprecated)]
            let (identifier, rest) = line.trim_left()
                .split_at(line.find(char::is_whitespace).unwrap_or_else(|| line.len()));
            match identifier {
                "ENDHDR" => break,
                "HEIGHT" => if height.is_some() {
                    return Err(ImageError::FormatError("Duplicate HEIGHT line".to_string()));
                } else {
                    let h = rest.trim()
                        .parse::<u32>()
                        .map_err(|err| ImageError::FormatError(
                                format!("Invalid height {}: {}", rest, err)
                            ))?;
                    height = Some(h);
                },
                "WIDTH" => if width.is_some() {
                    return Err(ImageError::FormatError("Duplicate WIDTH line".to_string()));
                } else {
                    let w = rest.trim()
                        .parse::<u32>()
                        .map_err(|err| ImageError::FormatError(
                                format!("Invalid width {}: {}", rest, err)
                            ))?;
                    width = Some(w);
                },
                "DEPTH" => if depth.is_some() {
                    return Err(ImageError::FormatError("Duplicate DEPTH line".to_string()));
                } else {
                    let d = rest.trim()
                        .parse::<u32>()
                        .map_err(|err| ImageError::FormatError(
                                format!("Invalid depth {}: {}", rest, err)
                            ))?;
                    depth = Some(d);
                },
                "MAXVAL" => if maxval.is_some() {
                    return Err(ImageError::FormatError("Duplicate MAXVAL line".to_string()));
                } else {
                    let m = rest.trim()
                        .parse::<u32>()
                        .map_err(|err| ImageError::FormatError(
                                format!("Invalid maxval {}: {}", rest, err)
                            ))?;
                    maxval = Some(m);
                },
                "TUPLTYPE" => {
                    let identifier = rest.trim();
                    if tupltype.is_some() {
                        let appended = tupltype.take().map(|mut v| {
                            v.push(' ');
                            v.push_str(identifier);
                            v
                        });
                        tupltype = appended;
                    } else {
                        tupltype = Some(identifier.to_string());
                    }
                }
                _ => return Err(ImageError::FormatError("Unknown header line".to_string())),
            }
        }

        let (h, w, d, m) = match (height, width, depth, maxval) {
            (None, _, _, _) => {
                return Err(ImageError::FormatError(
                    "Expected one HEIGHT line".to_string(),
                ))
            }
            (_, None, _, _) => {
                return Err(ImageError::FormatError(
                    "Expected one WIDTH line".to_string(),
                ))
            }
            (_, _, None, _) => {
                return Err(ImageError::FormatError(
                    "Expected one DEPTH line".to_string(),
                ))
            }
            (_, _, _, None) => {
                return Err(ImageError::FormatError(
                    "Expected one MAXVAL line".to_string(),
                ))
            }
            (Some(h), Some(w), Some(d), Some(m)) => (h, w, d, m),
        };

        let tupltype = match tupltype {
            None => None,
            Some(ref t) if t == "BLACKANDWHITE" => Some(ArbitraryTuplType::BlackAndWhite),
            Some(ref t) if t == "BLACKANDWHITE_ALPHA" => {
                Some(ArbitraryTuplType::BlackAndWhiteAlpha)
            }
            Some(ref t) if t == "GRAYSCALE" => Some(ArbitraryTuplType::Grayscale),
            Some(ref t) if t == "GRAYSCALE_ALPHA" => Some(ArbitraryTuplType::GrayscaleAlpha),
            Some(ref t) if t == "RGB" => Some(ArbitraryTuplType::RGB),
            Some(ref t) if t == "RGB_ALPHA" => Some(ArbitraryTuplType::RGBAlpha),
            Some(other) => Some(ArbitraryTuplType::Custom(other)),
        };

        Ok(ArbitraryHeader {
            height: h,
            width: w,
            depth: d,
            maxval: m,
            tupltype,
        })
    }
}

impl<R: Read> HeaderReader for BufReader<R> {}

/// Wrapper struct around a `Cursor<Vec<u8>>`
pub struct PnmReader<R>(Cursor<Vec<u8>>, PhantomData<R>);
impl<R> Read for PnmReader<R> {
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

impl<'a, R: 'a + Read> ImageDecoder<'a> for PNMDecoder<R> {
    type Reader = PnmReader<R>;

    fn dimensions(&self) -> (u64, u64) {
        (self.header.width() as u64, self.header.height() as u64)
    }

    fn colortype(&self) -> ColorType {
        self.tuple.color()
    }

    fn into_reader(self) -> ImageResult<Self::Reader> {
        Ok(PnmReader(Cursor::new(self.read_image()?), PhantomData))
    }

    fn read_image(mut self) -> ImageResult<Vec<u8>> {
        self.read()
    }
}

impl<R: Read> PNMDecoder<R> {
    fn read(&mut self) -> ImageResult<Vec<u8>> {
        match self.tuple {
            TupleType::PbmBit => self.read_samples::<PbmBit>(1),
            TupleType::BWBit => self.read_samples::<BWBit>(1),
            TupleType::RGBU8 => self.read_samples::<U8>(3),
            TupleType::RGBU16 => self.read_samples::<U16>(3),
            TupleType::GrayU8 => self.read_samples::<U8>(1),
            TupleType::GrayU16 => self.read_samples::<U16>(1),
        }
    }

    fn read_samples<S: Sample>(&mut self, components: u32) -> ImageResult<Vec<u8>> {
        match self.subtype().sample_encoding() {
            SampleEncoding::Binary => {
                let width = self.header.width();
                let height = self.header.height();
                let bytecount = S::bytelen(width, height, components)?;
                let mut bytes = vec![0 as u8; bytecount];
                (&mut self.reader)
                    .read_exact(&mut bytes)
                    .map_err(|_| ImageError::NotEnoughData)?;
                let samples = S::from_bytes(&bytes, width, height, components)?;
                Ok(samples.into())
            }
            SampleEncoding::Ascii => {
                let samples = self.read_ascii::<S>(components)?;
                Ok(samples.into())
            }
        }
    }

    fn read_ascii<Basic: Sample>(&mut self, components: u32) -> ImageResult<Vec<u8>> {
        Basic::from_ascii(&mut self.reader, self.header.width(), self.header.height(), components)
    }

    /// Get the pnm subtype, depending on the magic constant contained in the header
    pub fn subtype(&self) -> PNMSubtype {
        self.header.subtype()
    }
}

impl TupleType {
    fn color(self) -> ColorType {
        use self::TupleType::*;
        match self {
            PbmBit => ColorType::Gray(1),
            BWBit => ColorType::Gray(1),
            GrayU8 => ColorType::Gray(8),
            GrayU16 => ColorType::Gray(16),
            RGBU8 => ColorType::RGB(8),
            RGBU16 => ColorType::GrayA(16),
        }
    }
}

fn read_separated_ascii<T: FromStr>(reader: &mut dyn Read) -> ImageResult<T>
    where T::Err: Display
{
    let is_separator = |v: &u8| match *v {
        b'\t' | b'\n' | b'\x0b' | b'\x0c' | b'\r' | b' ' => true,
        _ => false,
    };

    let token = reader
        .bytes()
        .skip_while(|v| v.as_ref().ok().map(&is_separator).unwrap_or(false))
        .take_while(|v| v.as_ref().ok().map(|c| !is_separator(c)).unwrap_or(false))
        .collect::<Result<Vec<u8>, _>>()?;

    if !token.is_ascii() {
        return Err(ImageError::FormatError(
            "Non ascii character where sample value was expected".to_string(),
        ));
    }

    let string = str::from_utf8(&token)
        // We checked the precondition ourselves a few lines before, `token.is_ascii()`.
        .unwrap_or_else(|_| unreachable!("Only ascii characters should be decoded"));

    string
        .parse()
        .map_err(|err| ImageError::FormatError(format!("Error parsing {} as a sample: {}", string, err)))
}

impl Sample for U8 {
    fn bytelen(width: u32, height: u32, samples: u32) -> ImageResult<usize> {
        Ok((width * height * samples) as usize)
    }

    fn from_bytes(
        bytes: &[u8],
        _width: u32,
        _height: u32,
        _samples: u32,
    ) -> ImageResult<Vec<u8>> {
        let mut buffer = Vec::new();
        buffer.resize(bytes.len(), 0 as u8);
        buffer.copy_from_slice(bytes);
        Ok(buffer)
    }

    fn from_ascii(
        reader: &mut dyn Read,
        width: u32,
        height: u32,
        samples: u32,
    ) -> ImageResult<Vec<u8>> {
        (0..width*height*samples)
            .map(|_| read_separated_ascii(reader))
            .collect()
    }
}

impl Sample for U16 {
    fn bytelen(width: u32, height: u32, samples: u32) -> ImageResult<usize> {
        Ok((width * height * samples * 2) as usize)
    }

    fn from_bytes(
        bytes: &[u8],
        width: u32,
        height: u32,
        samples: u32,
    ) -> ImageResult<Vec<u8>> {
        assert_eq!(bytes.len(), (width*height*samples*2) as usize);

        let mut buffer = bytes.to_vec();
        for chunk in buffer.chunks_mut(2) {
            let v = BigEndian::read_u16(chunk);
            NativeEndian::write_u16(chunk, v);
        }
        Ok(buffer)
    }

    fn from_ascii(
        reader: &mut dyn Read,
        width: u32,
        height: u32,
        samples: u32,
    ) -> ImageResult<Vec<u8>> {
        let mut buffer = vec![0; (width * height * samples * 2) as usize];
        for i in 0..(width*height*samples) as usize {
            let v = read_separated_ascii::<u16>(reader)?;
            NativeEndian::write_u16(&mut buffer[2*i..][..2], v);
        }
        Ok(buffer)
    }
}

// The image is encoded in rows of bits, high order bits first. Any bits beyond the row bits should
// be ignored. Also, contrary to rgb, black pixels are encoded as a 1 while white is 0. This will
// need to be reversed for the grayscale output.
impl Sample for PbmBit {
    fn bytelen(width: u32, height: u32, samples: u32) -> ImageResult<usize> {
        let count = width * samples;
        let linelen = (count / 8) + ((count % 8) != 0) as u32;
        Ok((linelen * height) as usize)
    }

    fn from_bytes(
        bytes: &[u8],
        _width: u32,
        _height: u32,
        _samples: u32,
    ) -> ImageResult<Vec<u8>> {
        Ok(bytes.iter().map(|pixel| !pixel).collect())
    }

    fn from_ascii(
        reader: &mut dyn Read,
        width: u32,
        height: u32,
        samples: u32,
    ) -> ImageResult<Vec<u8>> {
        let count = (width*height*samples) as usize;
        let raw_samples = reader.bytes()
            .filter_map(|ascii| match ascii {
                Ok(b'0') => Some(Ok(1)),
                Ok(b'1') => Some(Ok(0)),
                Err(err) => Some(Err(ImageError::IoError(err))),
                Ok(b'\t')
                | Ok(b'\n')
                | Ok(b'\x0b')
                | Ok(b'\x0c')
                | Ok(b'\r')
                | Ok(b' ') => None,
                Ok(c) => Some(Err(ImageError::FormatError(
                        format!("Unexpected character {} within sample raster", c),
                    ))),
            })
            .take(count)
            .collect::<ImageResult<Vec<u8>>>()?;

        if raw_samples.len() < count {
            return Err(ImageError::NotEnoughData)
        }

        Ok(raw_samples)
    }
}

// Encoded just like a normal U8 but we check the values.
impl Sample for BWBit {
    fn bytelen(width: u32, height: u32, samples: u32) -> ImageResult<usize> {
        U8::bytelen(width, height, samples)
    }

    fn from_bytes(
        bytes: &[u8],
        width: u32,
        height: u32,
        samples: u32,
    ) -> ImageResult<Vec<u8>> {
        let values = U8::from_bytes(bytes, width, height, samples)?;
        if let Some(val) = values.iter().find(|&val| *val > 1) {
            return Err(ImageError::FormatError(
                format!("Sample value {} outside of bounds", val),
            ));
        };
        Ok(values)
    }

    fn from_ascii(
        _reader: &mut dyn Read,
        _width: u32,
        _height: u32,
        _samples: u32,
    ) -> ImageResult<Vec<u8>> {
        unreachable!("BW bits from anymaps are never encoded as ascii")
    }
}

impl DecodableImageHeader for BitmapHeader {
    fn tuple_type(&self) -> ImageResult<TupleType> {
        Ok(TupleType::PbmBit)
    }
}

impl DecodableImageHeader for GraymapHeader {
    fn tuple_type(&self) -> ImageResult<TupleType> {
        match self.maxwhite {
            v if v <= 0xFF => Ok(TupleType::GrayU8),
            v if v <= 0xFFFF => Ok(TupleType::GrayU16),
            _ => Err(ImageError::FormatError(
                "Image maxval is not less or equal to 65535".to_string(),
            )),
        }
    }
}

impl DecodableImageHeader for PixmapHeader {
    fn tuple_type(&self) -> ImageResult<TupleType> {
        match self.maxval {
            v if v <= 0xFF => Ok(TupleType::RGBU8),
            v if v <= 0xFFFF => Ok(TupleType::RGBU16),
            _ => Err(ImageError::FormatError(
                "Image maxval is not less or equal to 65535".to_string(),
            )),
        }
    }
}

impl DecodableImageHeader for ArbitraryHeader {
    fn tuple_type(&self) -> ImageResult<TupleType> {
        match self.tupltype {
            None if self.depth == 1 => Ok(TupleType::GrayU8),
            None if self.depth == 2 => Err(ImageError::UnsupportedColor(ColorType::GrayA(8))),
            None if self.depth == 3 => Ok(TupleType::RGBU8),
            None if self.depth == 4 => Err(ImageError::UnsupportedColor(ColorType::RGBA(8))),

            Some(ArbitraryTuplType::BlackAndWhite) if self.maxval == 1 && self.depth == 1 => {
                Ok(TupleType::BWBit)
            }
            Some(ArbitraryTuplType::BlackAndWhite) => Err(ImageError::FormatError(
                "Invalid depth or maxval for tuple type BLACKANDWHITE".to_string(),
            )),

            Some(ArbitraryTuplType::Grayscale) if self.depth == 1 && self.maxval <= 0xFF => {
                Ok(TupleType::GrayU8)
            }
            Some(ArbitraryTuplType::Grayscale) if self.depth <= 1 && self.maxval <= 0xFFFF => {
                Ok(TupleType::GrayU16)
            }
            Some(ArbitraryTuplType::Grayscale) => Err(ImageError::FormatError(
                "Invalid depth or maxval for tuple type GRAYSCALE".to_string(),
            )),

            Some(ArbitraryTuplType::RGB) if self.depth == 3 && self.maxval <= 0xFF => {
                Ok(TupleType::RGBU8)
            }
            Some(ArbitraryTuplType::RGB) if self.depth == 3 && self.maxval <= 0xFFFF => {
                Ok(TupleType::RGBU16)
            }
            Some(ArbitraryTuplType::RGB) => Err(ImageError::FormatError(
                "Invalid depth for tuple type RGB".to_string(),
            )),

            Some(ArbitraryTuplType::BlackAndWhiteAlpha) => {
                Err(ImageError::UnsupportedColor(ColorType::GrayA(1)))
            }
            Some(ArbitraryTuplType::GrayscaleAlpha) => {
                Err(ImageError::UnsupportedColor(ColorType::GrayA(8)))
            }
            Some(ArbitraryTuplType::RGBAlpha) => {
                Err(ImageError::UnsupportedColor(ColorType::RGBA(8)))
            }
            _ => Err(ImageError::FormatError(
                "Tuple type not recognized".to_string(),
            )),
        }
    }
}
#[cfg(test)]
mod tests {
    use super::*;
    /// Tests reading of a valid blackandwhite pam
    #[test]
    fn pam_blackandwhite() {
        let pamdata = b"P7
WIDTH 4
HEIGHT 4
DEPTH 1
MAXVAL 1
TUPLTYPE BLACKANDWHITE
# Comment line
ENDHDR
\x01\x00\x00\x01\x01\x00\x00\x01\x01\x00\x00\x01\x01\x00\x00\x01";
        let decoder = PNMDecoder::new(&pamdata[..]).unwrap();
        assert_eq!(decoder.colortype(), ColorType::Gray(1));
        assert_eq!(decoder.dimensions(), (4, 4));
        assert_eq!(decoder.subtype(), PNMSubtype::ArbitraryMap);

        assert_eq!(
            decoder.read_image().unwrap(),
            vec![0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00, 0x00, 0x01, 0x01, 0x00,
                 0x00, 0x01]
        );
        match PNMDecoder::new(&pamdata[..]).unwrap().into_inner() {
            (
                _,
                PNMHeader {
                    decoded:
                        HeaderRecord::Arbitrary(ArbitraryHeader {
                            width: 4,
                            height: 4,
                            maxval: 1,
                            depth: 1,
                            tupltype: Some(ArbitraryTuplType::BlackAndWhite),
                        }),
                    encoded: _,
                },
            ) => (),
            _ => panic!("Decoded header is incorrect"),
        }
    }

    /// Tests reading of a valid grayscale pam
    #[test]
    fn pam_grayscale() {
        let pamdata = b"P7
WIDTH 4
HEIGHT 4
DEPTH 1
MAXVAL 255
TUPLTYPE GRAYSCALE
# Comment line
ENDHDR
\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad\xbe\xef";
        let decoder = PNMDecoder::new(&pamdata[..]).unwrap();
        assert_eq!(decoder.colortype(), ColorType::Gray(8));
        assert_eq!(decoder.dimensions(), (4, 4));
        assert_eq!(decoder.subtype(), PNMSubtype::ArbitraryMap);
        assert_eq!(
            decoder.read_image().unwrap(),
            vec![0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad,
                 0xbe, 0xef]
        );
        match PNMDecoder::new(&pamdata[..]).unwrap().into_inner() {
            (
                _,
                PNMHeader {
                    decoded:
                        HeaderRecord::Arbitrary(ArbitraryHeader {
                            width: 4,
                            height: 4,
                            depth: 1,
                            maxval: 255,
                            tupltype: Some(ArbitraryTuplType::Grayscale),
                        }),
                    encoded: _,
                },
            ) => (),
            _ => panic!("Decoded header is incorrect"),
        }
    }

    /// Tests reading of a valid rgb pam
    #[test]
    fn pam_rgb() {
        let pamdata = b"P7
# Comment line
MAXVAL 255
TUPLTYPE RGB
DEPTH 3
WIDTH 2
HEIGHT 2
ENDHDR
\xde\xad\xbe\xef\xde\xad\xbe\xef\xde\xad\xbe\xef";
        let decoder = PNMDecoder::new(&pamdata[..]).unwrap();
        assert_eq!(decoder.colortype(), ColorType::RGB(8));
        assert_eq!(decoder.dimensions(), (2, 2));
        assert_eq!(decoder.subtype(), PNMSubtype::ArbitraryMap);

        assert_eq!(decoder.read_image().unwrap(),
                   vec![0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef, 0xde, 0xad, 0xbe, 0xef]);
        match PNMDecoder::new(&pamdata[..]).unwrap().into_inner() {
            (
                _,
                PNMHeader {
                    decoded:
                        HeaderRecord::Arbitrary(ArbitraryHeader {
                            maxval: 255,
                            tupltype: Some(ArbitraryTuplType::RGB),
                            depth: 3,
                            width: 2,
                            height: 2,
                        }),
                    encoded: _,
                },
            ) => (),
            _ => panic!("Decoded header is incorrect"),
        }
    }

    #[test]
    fn pbm_binary() {
        // The data contains two rows of the image (each line is padded to the full byte). For
        // comments on its format, see documentation of `impl SampleType for PbmBit`.
        let pbmbinary = [&b"P4 6 2\n"[..], &[0b01101100 as u8, 0b10110111]].concat();
        let decoder = PNMDecoder::new(&pbmbinary[..]).unwrap();
        assert_eq!(decoder.colortype(), ColorType::Gray(1));
        assert_eq!(decoder.dimensions(), (6, 2));
        assert_eq!(
            decoder.subtype(),
            PNMSubtype::Bitmap(SampleEncoding::Binary)
        );
        assert_eq!(decoder.read_image().unwrap(), vec![0b10010011, 0b01001000]);
        match PNMDecoder::new(&pbmbinary[..]).unwrap().into_inner() {
            (
                _,
                PNMHeader {
                    decoded:
                        HeaderRecord::Bitmap(BitmapHeader {
                            encoding: SampleEncoding::Binary,
                            width: 6,
                            height: 2,
                        }),
                    encoded: _,
                },
            ) => (),
            _ => panic!("Decoded header is incorrect"),
        }
    }

    /// A previous inifite loop.
    #[test]
    fn pbm_binary_ascii_termination() {
        use std::io::{Cursor, Error, ErrorKind, Read, Result};
        struct FailRead(Cursor<&'static [u8]>);

        impl Read for FailRead {
            fn read(&mut self, buf: &mut [u8]) -> Result<usize> {
                match self.0.read(buf) {
                    Ok(n) if n > 0 => Ok(n),
                    _ => Err(Error::new(
                        ErrorKind::BrokenPipe,
                        "Simulated broken pipe error"
                    )),
                }
            }
        }

        let pbmbinary = FailRead(Cursor::new(b"P1 1 1\n"));

        PNMDecoder::new(pbmbinary).unwrap()
            .read_image().expect_err("Image is malformed");
    }

    #[test]
    fn pbm_ascii() {
        // The data contains two rows of the image (each line is padded to the full byte). For
        // comments on its format, see documentation of `impl SampleType for PbmBit`.  Tests all
        // whitespace characters that should be allowed (the 6 characters according to POSIX).
        let pbmbinary = b"P1 6 2\n 0 1 1 0 1 1\n1 0 1 1 0\t\n\x0b\x0c\r1";
        let decoder = PNMDecoder::new(&pbmbinary[..]).unwrap();
        assert_eq!(decoder.colortype(), ColorType::Gray(1));
        assert_eq!(decoder.dimensions(), (6, 2));
        assert_eq!(decoder.subtype(), PNMSubtype::Bitmap(SampleEncoding::Ascii));
        assert_eq!(decoder.read_image().unwrap(), vec![1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0]);
        match PNMDecoder::new(&pbmbinary[..]).unwrap().into_inner() {
            (
                _,
                PNMHeader {
                    decoded:
                        HeaderRecord::Bitmap(BitmapHeader {
                            encoding: SampleEncoding::Ascii,
                            width: 6,
                            height: 2,
                        }),
                    encoded: _,
                },
            ) => (),
            _ => panic!("Decoded header is incorrect"),
        }
    }

    #[test]
    fn pbm_ascii_nospace() {
        // The data contains two rows of the image (each line is padded to the full byte). Notably,
        // it is completely within specification for the ascii data not to contain separating
        // whitespace for the pbm format or any mix.
        let pbmbinary = b"P1 6 2\n011011101101";
        let decoder = PNMDecoder::new(&pbmbinary[..]).unwrap();
        assert_eq!(decoder.colortype(), ColorType::Gray(1));
        assert_eq!(decoder.dimensions(), (6, 2));
        assert_eq!(decoder.subtype(), PNMSubtype::Bitmap(SampleEncoding::Ascii));
        assert_eq!(decoder.read_image().unwrap(), vec![1, 0, 0, 1, 0, 0, 0, 1, 0, 0, 1, 0]);
        match PNMDecoder::new(&pbmbinary[..]).unwrap().into_inner() {
            (
                _,
                PNMHeader {
                    decoded:
                        HeaderRecord::Bitmap(BitmapHeader {
                            encoding: SampleEncoding::Ascii,
                            width: 6,
                            height: 2,
                        }),
                    encoded: _,
                },
            ) => (),
            _ => panic!("Decoded header is incorrect"),
        }
    }

    #[test]
    fn pgm_binary() {
        // The data contains two rows of the image (each line is padded to the full byte). For
        // comments on its format, see documentation of `impl SampleType for PbmBit`.
        let elements = (0..16).collect::<Vec<_>>();
        let pbmbinary = [&b"P5 4 4 255\n"[..], &elements].concat();
        let decoder = PNMDecoder::new(&pbmbinary[..]).unwrap();
        assert_eq!(decoder.colortype(), ColorType::Gray(8));
        assert_eq!(decoder.dimensions(), (4, 4));
        assert_eq!(
            decoder.subtype(),
            PNMSubtype::Graymap(SampleEncoding::Binary)
        );
        assert_eq!(decoder.read_image().unwrap(), elements);
        match PNMDecoder::new(&pbmbinary[..]).unwrap().into_inner() {
            (
                _,
                PNMHeader {
                    decoded:
                        HeaderRecord::Graymap(GraymapHeader {
                            encoding: SampleEncoding::Binary,
                            width: 4,
                            height: 4,
                            maxwhite: 255,
                        }),
                    encoded: _,
                },
            ) => (),
            _ => panic!("Decoded header is incorrect"),
        }
    }

    #[test]
    fn pgm_ascii() {
        // The data contains two rows of the image (each line is padded to the full byte). For
        // comments on its format, see documentation of `impl SampleType for PbmBit`.
        let pbmbinary = b"P2 4 4 255\n 0 1 2 3 4 5 6 7 8 9 10 11 12 13 14 15";
        let decoder = PNMDecoder::new(&pbmbinary[..]).unwrap();
        assert_eq!(decoder.colortype(), ColorType::Gray(8));
        assert_eq!(decoder.dimensions(), (4, 4));
        assert_eq!(
            decoder.subtype(),
            PNMSubtype::Graymap(SampleEncoding::Ascii)
        );
        assert_eq!(decoder.read_image().unwrap(), (0..16).collect::<Vec<_>>());
        match PNMDecoder::new(&pbmbinary[..]).unwrap().into_inner() {
            (
                _,
                PNMHeader {
                    decoded:
                        HeaderRecord::Graymap(GraymapHeader {
                            encoding: SampleEncoding::Ascii,
                            width: 4,
                            height: 4,
                            maxwhite: 255,
                        }),
                    encoded: _,
                },
            ) => (),
            _ => panic!("Decoded header is incorrect"),
        }
    }
}
