extern crate crc32fast;
extern crate inflate;

use std::borrow::Cow;
use std::default::Default;
use std::error;
use std::fmt;
use std::io;
use std::cmp::min;
use std::convert::From;

use crc32fast::Hasher as Crc32;

use self::inflate::InflateStream;
use crate::traits::ReadBytesExt;
use crate::common::{BitDepth, BlendOp, ColorType, DisposeOp, Info, Unit, PixelDimensions, AnimationControl, FrameControl};
use crate::chunk::{self, ChunkType, IHDR, IDAT, IEND};

/// TODO check if these size are reasonable
pub const CHUNCK_BUFFER_SIZE: usize = 32*1024;

/// Determines if checksum checks should be disabled globally.
///
/// This is used only in fuzzing. `afl` automatically adds `--cfg fuzzing` to RUSTFLAGS which can
/// be used to detect that build.
const CHECKSUM_DISABLED: bool = cfg!(fuzzing);

fn zlib_stream() -> InflateStream {
    if CHECKSUM_DISABLED {
        InflateStream::from_zlib_no_checksum()
    } else {
        InflateStream::from_zlib()
    }
}

#[derive(Debug)]
enum U32Value {
    // CHUNKS
    Length,
    Type(u32),
    Crc(ChunkType)
}

#[derive(Debug)]
enum State {
    Signature(u8, [u8; 7]),
    U32Byte3(U32Value, u32),
    U32Byte2(U32Value, u32),
    U32Byte1(U32Value, u32),
    U32(U32Value),
    ReadChunk(ChunkType, bool),
    PartialChunk(ChunkType),
    DecodeData(ChunkType, usize),
}

#[derive(Debug)]
/// Result of the decoding process
pub enum Decoded {
    /// Nothing decoded yet
    Nothing,
    Header(u32, u32, BitDepth, ColorType, bool),
    ChunkBegin(u32, ChunkType),
    ChunkComplete(u32, ChunkType),
    PixelDimensions(PixelDimensions),
    AnimationControl(AnimationControl),
    FrameControl(FrameControl),
    /// Decoded raw image data.
    ImageData,
    PartialChunk(ChunkType),
    ImageEnd,
}

#[derive(Debug)]
pub enum DecodingError {
    IoError(io::Error),
    Format(Cow<'static, str>),
    InvalidSignature,
    CrcMismatch {
        /// bytes to skip to try to recover from this error
        recover: usize,
        /// Stored CRC32 value
        crc_val: u32,
        /// Calculated CRC32 sum
        crc_sum: u32,
        chunk: ChunkType
    },
    Other(Cow<'static, str>),
    CorruptFlateStream,
    LimitsExceeded,
}

impl error::Error for DecodingError {
    fn description(&self) -> &str {
        use self::DecodingError::*;
        match *self {
            IoError(ref err) => err.description(),
            Format(ref desc) | Other(ref desc) => &desc,
            InvalidSignature => "invalid signature",
            CrcMismatch { .. } => "CRC error",
            CorruptFlateStream => "compressed data stream corrupted",
            LimitsExceeded => "limits are exceeded"
        }
    }
}

impl fmt::Display for DecodingError {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> Result<(), fmt::Error> {
        write!(fmt, "{}", (self as &dyn error::Error).description())
    }
}

impl From<io::Error> for DecodingError {
    fn from(err: io::Error) -> DecodingError {
        DecodingError::IoError(err)
    }
}

impl From<String> for DecodingError {
    fn from(err: String) -> DecodingError {
        DecodingError::Other(err.into())
    }
}

impl From<DecodingError> for io::Error {
    fn from(err: DecodingError) -> io::Error {
        use std::error::Error;
        match err {
            DecodingError::IoError(err) => err,
            err => io::Error::new(
                io::ErrorKind::Other,
                err.description()
            )
        }
    }
}

/// PNG StreamingDecoder (low-level interface)
pub struct StreamingDecoder {
    state: Option<State>,
    current_chunk: (Crc32, u32, Vec<u8>),
    inflater: InflateStream,
    info: Option<Info>,
    current_seq_no: Option<u32>,
    have_idat: bool,
}

impl StreamingDecoder {
    /// Creates a new StreamingDecoder
    ///
    /// Allocates the internal buffers.
    pub fn new() -> StreamingDecoder {
        StreamingDecoder {
            state: Some(State::Signature(0, [0; 7])),
            current_chunk: (Crc32::new(), 0, Vec::with_capacity(CHUNCK_BUFFER_SIZE)),
            inflater: zlib_stream(),
            info: None,
            current_seq_no: None,
            have_idat: false
        }
    }

    /// Resets the StreamingDecoder
    pub fn reset(&mut self) {
        self.state = Some(State::Signature(0, [0; 7]));
        self.current_chunk.0 = Crc32::new();
        self.current_chunk.1 = 0;
        self.current_chunk.2.clear();
        self.inflater = zlib_stream();
        self.info = None;
        self.current_seq_no = None;
        self.have_idat = false;
    }

    /// Low level StreamingDecoder interface.
    ///
    /// Allows to stream partial data to the encoder. Returns a tuple containing the bytes that have
    /// been consumed from the input buffer and the current decoding result. If the decoded chunk
    /// was an image data chunk, it also appends the read data to `image_data`.
    pub fn update(&mut self, mut buf: &[u8], image_data: &mut Vec<u8>)
    -> Result<(usize, Decoded), DecodingError> {
        let len = buf.len();
        while buf.len() > 0 && self.state.is_some() {
            match self.next_state(buf, image_data) {
                Ok((bytes, Decoded::Nothing)) => {
                    buf = &buf[bytes..]
                }
                Ok((bytes, result)) => {
                    buf = &buf[bytes..];
                    return Ok((len-buf.len(), result));
                }
                Err(err) => return Err(err)
            }
        }
        Ok((len-buf.len(), Decoded::Nothing))
    }

    fn next_state<'a>(&'a mut self, buf: &[u8], image_data: &mut Vec<u8>)
    -> Result<(usize, Decoded), DecodingError> {
        use self::State::*;

        macro_rules! goto (
            ($n:expr, $state:expr) => ({
                self.state = Some($state);
                Ok(($n, Decoded::Nothing))
            });
            ($state:expr) => ({
                self.state = Some($state);
                Ok((1, Decoded::Nothing))
            });
            ($n:expr, $state:expr, emit $res:expr) => ({
                self.state = Some($state);
                Ok(($n, $res))
            });
            ($state:expr, emit $res:expr) => ({
                self.state = Some($state);
                Ok((1, $res))
            })
        );

        let current_byte = buf[0];

        // Driver should ensure that state is never None
        let state = self.state.take().unwrap();
        //println!("state: {:?}", state);

        match state {
            Signature(i, mut signature) => if i < 7 {
                signature[i as usize] = current_byte;
                goto!(Signature(i+1, signature))
            } else {
                if signature == [137, 80, 78, 71, 13, 10, 26] && current_byte == 10 {
                    goto!(U32(U32Value::Length))
                } else {
                    Err(DecodingError::InvalidSignature)
                }
            },
            U32Byte3(type_, mut val) => {
                use self::U32Value::*;
                val |= current_byte as u32;
                match type_ {
                    Length => goto!(U32(Type(val))),
                    Type(length) => {
                        let type_str = [
                            (val >> 24) as u8,
                            (val >> 16) as u8,
                            (val >> 8) as u8,
                            val as u8
                        ];
                        self.current_chunk.0.reset();
                        self.current_chunk.0.update(&type_str);
                        self.current_chunk.1 = length;
                        goto!(
                            ReadChunk(type_str, true),
                            emit Decoded::ChunkBegin(length, type_str)
                        )
                    },
                    Crc(type_str) => {
                        let sum = self.current_chunk.0.clone().finalize();
                        if CHECKSUM_DISABLED || val == sum {
                            goto!(
                                State::U32(U32Value::Length),
                                emit if type_str == IEND {
                                    Decoded::ImageEnd
                                } else {
                                    Decoded::ChunkComplete(val, type_str)
                                }
                            )
                        } else {
                            Err(DecodingError::CrcMismatch {
                                recover: 1,
                                crc_val: val,
                                crc_sum: sum,
                                chunk: type_str
                            })
                        }
                    },
                }
            },
            U32Byte2(type_, val) => {
                goto!(U32Byte3(type_, val | (current_byte as u32) << 8))
            },
            U32Byte1(type_, val) => {
                goto!(U32Byte2(type_, val | (current_byte as u32) << 16))
            },
            U32(type_) => {
                goto!(U32Byte1(type_,       (current_byte as u32) << 24))
            },
            PartialChunk(type_str) => {
                match type_str {
                    IDAT => {
                        self.have_idat = true;
                        goto!(
                            0,
                            DecodeData(type_str, 0),
                            emit Decoded::PartialChunk(type_str)
                        )
                    },
                    chunk::fdAT => {
                        if let Some(seq_no) = self.current_seq_no {
                            let mut buf = &self.current_chunk.2[..];
                            let next_seq_no = buf.read_be()?;
                            if next_seq_no != seq_no + 1 {
                                return Err(DecodingError::Format(format!(
                                    "Sequence is not in order, expected #{} got #{}.",
                                    seq_no + 1,
                                    next_seq_no
                                ).into()))
                            }
                            self.current_seq_no = Some(next_seq_no);
                        } else {
                            return Err(DecodingError::Format("fcTL chunk missing before fdAT chunk.".into()))
                        }
                        goto!(
                            0,
                            DecodeData(type_str, 4),
                            emit Decoded::PartialChunk(type_str)
                        )
                    },
                    // Handle other chunks
                    _ => {
                        if self.current_chunk.1 == 0 { // complete chunk
                            Ok((0, self.parse_chunk(type_str)?))
                        } else {
                            goto!(
                                0, ReadChunk(type_str, true),
                                emit Decoded::PartialChunk(type_str)
                            )
                        }
                    }
                }

            },
            ReadChunk(type_str, clear) => {
                if clear {
                    self.current_chunk.2.clear();
                }
                if self.current_chunk.1 > 0 {
                    let (ref mut crc, ref mut remaining, ref mut c_buf) = self.current_chunk;
                    let buf_avail = c_buf.capacity() - c_buf.len();
                    let bytes_avail = min(buf.len(), buf_avail);
                    let n = min(*remaining, bytes_avail as u32);
                    if buf_avail == 0 {
                        goto!(0, PartialChunk(type_str))
                    } else {
                        let buf = &buf[..n as usize];
                        crc.update(buf);
                        c_buf.extend(buf.iter().map(|&v| v));
                        *remaining -= n;
                        if *remaining == 0 {
                            goto!(n as usize, PartialChunk(type_str
                            ))
                        } else {
                            goto!(n as usize, ReadChunk(type_str, false))
                        }

                    }
                } else {
                    goto!(0, U32(U32Value::Crc(type_str)))
                }
            }
            DecodeData(type_str, mut n) => {
                let chunk_len = self.current_chunk.2.len();
                let (c, data) = self.inflater.update(&self.current_chunk.2[n..])?;
                image_data.extend_from_slice(data);
                n += c;
                if n == chunk_len && data.len() == 0 && c == 0 {
                    goto!(
                        0,
                        ReadChunk(type_str, true),
                        emit Decoded::ImageData
                    )
                } else {
                    goto!(
                        0,
                        DecodeData(type_str, n),
                        emit Decoded::ImageData
                    )
                }
            }
        }
    }

    fn parse_chunk(&mut self, type_str: [u8; 4])
    -> Result<Decoded, DecodingError> {
        self.state = Some(State::U32(U32Value::Crc(type_str)));
        if self.info.is_none() && type_str != IHDR {
            return Err(DecodingError::Format(format!(
                "{} chunk appeared before IHDR chunk", String::from_utf8_lossy(&type_str)
            ).into()))
        }
        match match type_str {
            IHDR => {
                self.parse_ihdr()
            }
            chunk::PLTE => {
                self.parse_plte()
            }
            chunk::tRNS => {
                self.parse_trns()
            }
            chunk::pHYs => {
                self.parse_phys()
            }
            chunk::acTL => {
                self.parse_actl()
            }
            chunk::fcTL => {
                self.parse_fctl()
            }
            _ => Ok(Decoded::PartialChunk(type_str))
        } {
            Err(err) =>{
                // Borrow of self ends here, because Decoding error does not borrow self.
                self.state = None;
                Err(err)
            },
            ok => ok
        }
    }

    fn get_info_or_err(&self) -> Result<&Info, DecodingError> {
        self.info.as_ref().ok_or(DecodingError::Format(
            "IHDR chunk missing".into()
        ))
    }

    fn parse_fctl(&mut self)
    -> Result<Decoded, DecodingError> {
        let mut buf = &self.current_chunk.2[..];
        let next_seq_no = buf.read_be()?;

        // Asuming that fcTL is required before *every* fdAT-sequence
        self.current_seq_no = Some(if let Some(seq_no) = self.current_seq_no {
            if next_seq_no != seq_no + 1 {
                return Err(DecodingError::Format(format!(
                    "Sequence is not in order, expected #{} got #{}.",
                    seq_no + 1,
                    next_seq_no
                ).into()))
            }
            next_seq_no
        } else {
            if next_seq_no != 0 {
                return Err(DecodingError::Format(format!(
                    "Sequence is not in order, expected #{} got #{}.",
                    0,
                    next_seq_no
                ).into()))
            }
            0
        });
        self.inflater = zlib_stream();
        let fc = FrameControl {
            sequence_number: next_seq_no,
            width: buf.read_be()?,
            height: buf.read_be()?,
            x_offset: buf.read_be()?,
            y_offset: buf.read_be()?,
            delay_num: buf.read_be()?,
            delay_den: buf.read_be()?,
            dispose_op: match DisposeOp::from_u8(buf.read_be()?) {
                Some(dispose_op) => dispose_op,
                None => return Err(DecodingError::Format("invalid dispose operation".into()))
            },
            blend_op : match BlendOp::from_u8(buf.read_be()?) {
                Some(blend_op) => blend_op,
                None => return Err(DecodingError::Format("invalid blend operation".into()))
            },
        };
        self.info.as_mut().unwrap().frame_control = Some(fc.clone());
        Ok(Decoded::FrameControl(fc))
    }

    fn parse_actl(&mut self)
    -> Result<Decoded, DecodingError> {
        if self.have_idat {
            Err(DecodingError::Format(
                "acTL chunk appeared after first IDAT chunk".into()
            ))
        } else {
            let mut buf = &self.current_chunk.2[..];
            let actl = AnimationControl {
                num_frames: buf.read_be()?,
                num_plays: buf.read_be()?
            };
            self.info.as_mut().unwrap().animation_control = Some(actl);
            Ok(Decoded::AnimationControl(actl))
        }
    }

    fn parse_plte(&mut self)
    -> Result<Decoded, DecodingError> {
        let mut vec = Vec::new();
        vec.extend(self.current_chunk.2.iter().map(|&v| v));
        self.info.as_mut().map(
            |info| info.palette = Some(vec)
        );
        Ok(Decoded::Nothing)
    }

    fn parse_trns(&mut self)
    -> Result<Decoded, DecodingError> {
        use crate::common::ColorType::*;
        let (color_type, bit_depth) = {
            let info = self.get_info_or_err()?;
            (info.color_type, info.bit_depth as u8)
        };
        let mut vec = Vec::new();
        vec.extend(self.current_chunk.2.iter().map(|&v| v));
        let len = vec.len();
        let info = match self.info {
            Some(ref mut info) => info,
            None => return Err(DecodingError::Format(
              "tRNS chunk occured before IHDR chunk".into()
            ))
        };
        info.trns = Some(vec);
        let vec = info.trns.as_mut().unwrap();
        match color_type {
            Grayscale => {
                if len < 2 {
                    return Err(DecodingError::Format(
                        "not enough palette entries".into()
                    ))
                }
                if bit_depth < 16 {
                    vec[0] = vec[1];
                    vec.truncate(1);
                }
                Ok(Decoded::Nothing)
            },
            RGB => {
                if len < 6 {
                    return Err(DecodingError::Format(
                        "not enough palette entries".into()
                    ))
                }
                if bit_depth < 16 {
                    vec[0] = vec[1];
                    vec[1] = vec[3];
                    vec[2] = vec[5];
                    vec.truncate(3);
                }
                Ok(Decoded::Nothing)
            },
            Indexed => {
                let _ = info.palette.as_ref().ok_or(DecodingError::Format(
                    "tRNS chunk occured before PLTE chunk".into()
                ));
                Ok(Decoded::Nothing)
            },
            c => Err(DecodingError::Format(
                format!("tRNS chunk found for color type ({})", c as u8).into()
            ))
        }

    }

    fn parse_phys(&mut self)
    -> Result<Decoded, DecodingError> {
        if self.have_idat {
            Err(DecodingError::Format(
                "pHYs chunk appeared after first IDAT chunk".into()
            ))
        } else {
            let mut buf = &self.current_chunk.2[..];
            let xppu = buf.read_be()?;
            let yppu = buf.read_be()?;
            let unit = buf.read_be()?;
            let unit = match Unit::from_u8(unit) {
                Some(unit) => unit,
                None => return Err(DecodingError::Format(
                    format!("invalid unit ({})", unit).into()
                ))
            };
            let pixel_dims = PixelDimensions {
                xppu: xppu,
                yppu: yppu,
                unit: unit,
            };
            self.info.as_mut().unwrap().pixel_dims = Some(pixel_dims);
            Ok(Decoded::PixelDimensions(pixel_dims))
        }
    }

    fn parse_ihdr(&mut self)
    -> Result<Decoded, DecodingError> {
        // TODO: check if color/bit depths combination is valid
        let mut buf = &self.current_chunk.2[..];
        let width = buf.read_be()?;
        let height = buf.read_be()?;
        let bit_depth = buf.read_be()?;
        let bit_depth = match BitDepth::from_u8(bit_depth) {
            Some(bits) => bits,
            None => return Err(DecodingError::Format(
                format!("invalid bit depth ({})", bit_depth).into()
            ))
        };
        let color_type = buf.read_be()?;
        let color_type = match ColorType::from_u8(color_type) {
            Some(color_type) => color_type,
            None => return Err(DecodingError::Format(
                format!("invalid color type ({})", color_type).into()
            ))
        };
        match buf.read_be()? { // compression method
            0u8 => (),
            n => return Err(DecodingError::Format(
                format!("unknown compression method ({})", n).into()
            ))
        }
        match buf.read_be()? { // filter method
            0u8 => (),
            n => return Err(DecodingError::Format(
                format!("unknown filter method ({})", n).into()
            ))
        }
        let interlaced = match buf.read_be()? {
            0u8 => false,
            1 => {
                true
            },
            n => return Err(DecodingError::Format(
                format!("unknown interlace method ({})", n).into()
            ))
        };
        let mut info = Info::default();

        info.width = width;
        info.height = height;
        info.bit_depth = bit_depth;
        info.color_type = color_type;
        info.interlaced = interlaced;
        self.info = Some(info);
        Ok(Decoded::Header(
            width,
            height,
            bit_depth,
            color_type,
            interlaced
        ))
    }
}

#[inline(always)]
pub fn get_info(d: &StreamingDecoder) -> Option<&Info> {
    d.info.as_ref()
}
