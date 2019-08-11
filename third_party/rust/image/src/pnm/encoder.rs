//! Encoding of PNM Images
use std::fmt;
use std::io;

use std::io::Write;

use super::AutoBreak;
use super::{ArbitraryHeader, ArbitraryTuplType, BitmapHeader, GraymapHeader, PixmapHeader};
use super::{HeaderRecord, PNMHeader, PNMSubtype, SampleEncoding};
use color::{num_components, ColorType};

use byteorder::{BigEndian, WriteBytesExt};

enum HeaderStrategy {
    Dynamic,
    Subtype(PNMSubtype),
    Chosen(PNMHeader),
}

#[derive(Clone, Copy)]
pub enum FlatSamples<'a> {
    U8(&'a [u8]),
    U16(&'a [u16]),
}

/// Encodes images to any of the `pnm` image formats.
pub struct PNMEncoder<W: Write> {
    writer: W,
    header: HeaderStrategy,
}

/// Encapsulate the checking system in the type system. Non of the fields are actually accessed
/// but requiring them forces us to validly construct the struct anyways.
struct CheckedImageBuffer<'a> {
    _image: FlatSamples<'a>,
    _width: u32,
    _height: u32,
    _color: ColorType,
}

// Check the header against the buffer. Each struct produces the next after a check.
struct UncheckedHeader<'a> {
    header: &'a PNMHeader,
}

struct CheckedDimensions<'a> {
    unchecked: UncheckedHeader<'a>,
    width: u32,
    height: u32,
}

struct CheckedHeaderColor<'a> {
    dimensions: CheckedDimensions<'a>,
    color: ColorType,
}

struct CheckedHeader<'a> {
    color: CheckedHeaderColor<'a>,
    encoding: TupleEncoding<'a>,
    _image: CheckedImageBuffer<'a>,
}

enum TupleEncoding<'a> {
    PbmBits {
        samples: FlatSamples<'a>,
        width: u32,
    },
    Ascii {
        samples: FlatSamples<'a>,
    },
    Bytes {
        samples: FlatSamples<'a>,
    },
}

impl<W: Write> PNMEncoder<W> {
    /// Create new PNMEncoder from the `writer`.
    ///
    /// The encoded images will have some `pnm` format. If more control over the image type is
    /// required, use either one of `with_subtype` or `with_header`. For more information on the
    /// behaviour, see `with_dynamic_header`.
    pub fn new(writer: W) -> Self {
        PNMEncoder {
            writer,
            header: HeaderStrategy::Dynamic,
        }
    }

    /// Encode a specific pnm subtype image.
    ///
    /// The magic number and encoding type will be chosen as provided while the rest of the header
    /// data will be generated dynamically. Trying to encode incompatible images (e.g. encoding an
    /// RGB image as Graymap) will result in an error.
    ///
    /// This will overwrite the effect of earlier calls to `with_header` and `with_dynamic_header`.
    pub fn with_subtype(self, subtype: PNMSubtype) -> Self {
        PNMEncoder {
            writer: self.writer,
            header: HeaderStrategy::Subtype(subtype),
        }
    }

    /// Enforce the use of a chosen header.
    ///
    /// While this option gives the most control over the actual written data, the encoding process
    /// will error in case the header data and image parameters do not agree. It is the users
    /// obligation to ensure that the width and height are set accordingly, for example.
    ///
    /// Choose this option if you want a lossless decoding/encoding round trip.
    ///
    /// This will overwrite the effect of earlier calls to `with_subtype` and `with_dynamic_header`.
    pub fn with_header(self, header: PNMHeader) -> Self {
        PNMEncoder {
            writer: self.writer,
            header: HeaderStrategy::Chosen(header),
        }
    }

    /// Create the header dynamically for each image.
    ///
    /// This is the default option upon creation of the encoder. With this, most images should be
    /// encodable but the specific format chosen is out of the users control. The pnm subtype is
    /// chosen arbitrarily by the library.
    ///
    /// This will overwrite the effect of earlier calls to `with_subtype` and `with_header`.
    pub fn with_dynamic_header(self) -> Self {
        PNMEncoder {
            writer: self.writer,
            header: HeaderStrategy::Dynamic,
        }
    }

    /// Encode an image whose samples are represented as `u8`.
    ///
    /// Some `pnm` subtypes are incompatible with some color options, a chosen header most
    /// certainly with any deviation from the original decoded image.
    pub fn encode<'s, S>(
        &mut self,
        image: S,
        width: u32,
        height: u32,
        color: ColorType,
    ) -> io::Result<()>
    where
        S: Into<FlatSamples<'s>>,
    {
        let image = image.into();
        match self.header {
            HeaderStrategy::Dynamic => self.write_dynamic_header(image, width, height, color),
            HeaderStrategy::Subtype(subtype) => {
                self.write_subtyped_header(subtype, image, width, height, color)
            }
            HeaderStrategy::Chosen(ref header) => {
                Self::write_with_header(&mut self.writer, header, image, width, height, color)
            }
        }
    }

    /// Choose any valid pnm format that the image can be expressed in and write its header.
    ///
    /// Returns how the body should be written if successful.
    fn write_dynamic_header(
        &mut self,
        image: FlatSamples,
        width: u32,
        height: u32,
        color: ColorType,
    ) -> io::Result<()> {
        let depth = num_components(color) as u32;
        let (maxval, tupltype) = match color {
            ColorType::Gray(1) => (1, ArbitraryTuplType::BlackAndWhite),
            ColorType::GrayA(1) => (1, ArbitraryTuplType::BlackAndWhiteAlpha),
            ColorType::Gray(n @ 1..=16) => ((1 << n) - 1, ArbitraryTuplType::Grayscale),
            ColorType::GrayA(n @ 1..=16) => ((1 << n) - 1, ArbitraryTuplType::GrayscaleAlpha),
            ColorType::RGB(n @ 1..=16) => ((1 << n) - 1, ArbitraryTuplType::RGB),
            ColorType::RGBA(n @ 1..=16) => ((1 << n) - 1, ArbitraryTuplType::RGBAlpha),
            _ => {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    &format!("Encoding colour type {:?} is not supported", color)[..],
                ))
            }
        };

        let header = PNMHeader {
            decoded: HeaderRecord::Arbitrary(ArbitraryHeader {
                width,
                height,
                depth,
                maxval,
                tupltype: Some(tupltype),
            }),
            encoded: None,
        };

        Self::write_with_header(&mut self.writer, &header, image, width, height, color)
    }

    /// Try to encode the image with the chosen format, give its corresponding pixel encoding type.
    fn write_subtyped_header(
        &mut self,
        subtype: PNMSubtype,
        image: FlatSamples,
        width: u32,
        height: u32,
        color: ColorType,
    ) -> io::Result<()> {
        let header = match (subtype, color) {
            (PNMSubtype::ArbitraryMap, color) => {
                return self.write_dynamic_header(image, width, height, color)
            }
            (PNMSubtype::Pixmap(encoding), ColorType::RGB(8)) => PNMHeader {
                decoded: HeaderRecord::Pixmap(PixmapHeader {
                    encoding,
                    width,
                    height,
                    maxval: 255,
                }),
                encoded: None,
            },
            (PNMSubtype::Graymap(encoding), ColorType::Gray(8)) => PNMHeader {
                decoded: HeaderRecord::Graymap(GraymapHeader {
                    encoding,
                    width,
                    height,
                    maxwhite: 255,
                }),
                encoded: None,
            },
            (PNMSubtype::Bitmap(encoding), ColorType::Gray(8))
            | (PNMSubtype::Bitmap(encoding), ColorType::Gray(1)) => PNMHeader {
                decoded: HeaderRecord::Bitmap(BitmapHeader {
                    encoding,
                    width,
                    height,
                }),
                encoded: None,
            },
            (_, _) => {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    "Color type can not be represented in the chosen format",
                ))
            }
        };

        Self::write_with_header(&mut self.writer, &header, image, width, height, color)
    }

    /// Try to encode the image with the chosen header, checking if values are correct.
    ///
    /// Returns how the body should be written if successful.
    fn write_with_header(
        writer: &mut dyn Write,
        header: &PNMHeader,
        image: FlatSamples,
        width: u32,
        height: u32,
        color: ColorType,
    ) -> io::Result<()> {
        let unchecked = UncheckedHeader { header };

        unchecked
            .check_header_dimensions(width, height)?
            .check_header_color(color)?
            .check_sample_values(image)?
            .write_header(writer)?
            .write_image(writer)
    }
}

impl<'a> CheckedImageBuffer<'a> {
    fn check(
        image: FlatSamples<'a>,
        width: u32,
        height: u32,
        color: ColorType,
    ) -> io::Result<CheckedImageBuffer<'a>> {
        let components = num_components(color);
        let uwidth = width as usize;
        let uheight = height as usize;
        match Some(components)
            .and_then(|v| v.checked_mul(uwidth))
            .and_then(|v| v.checked_mul(uheight))
        {
            None => Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                &format!(
                    "Image dimensions invalid: {}×{}×{} (w×h×d)",
                    width, height, components
                )[..],
            )),
            Some(v) if v == image.len() => Ok(CheckedImageBuffer {
                _image: image,
                _width: width,
                _height: height,
                _color: color,
            }),
            Some(_) => Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                &"Image buffer does not correspond to size and colour".to_string()[..],
            )),
        }
    }
}

impl<'a> UncheckedHeader<'a> {
    fn check_header_dimensions(self, width: u32, height: u32) -> io::Result<CheckedDimensions<'a>> {
        if self.header.width() != width || self.header.height() != height {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "Chosen header does not match Image dimensions",
            ));
        }

        Ok(CheckedDimensions {
            unchecked: self,
            width,
            height,
        })
    }
}

impl<'a> CheckedDimensions<'a> {
    // Check color compatibility with the header. This will only error when we are certain that
    // the comination is bogus (e.g. combining Pixmap and Palette) but allows uncertain
    // combinations (basically a ArbitraryTuplType::Custom with any color of fitting depth).
    fn check_header_color(self, color: ColorType) -> io::Result<CheckedHeaderColor<'a>> {
        let components = match color {
            ColorType::Gray(_) => 1,
            ColorType::GrayA(_) => 2,
            ColorType::Palette(_) | ColorType::RGB(_) | ColorType::BGR(_)=> 3,
            ColorType::RGBA(_) | ColorType::BGRA(_) => 4,
        };

        match *self.unchecked.header {
            PNMHeader {
                decoded: HeaderRecord::Bitmap(_),
                ..
            } => match color {
                ColorType::Gray(_) => (),
                _ => {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        "PBM format only support ColorType::Gray",
                    ))
                }
            },
            PNMHeader {
                decoded: HeaderRecord::Graymap(_),
                ..
            } => match color {
                ColorType::Gray(_) => (),
                _ => {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        "PGM format only support ColorType::Gray",
                    ))
                }
            },
            PNMHeader {
                decoded: HeaderRecord::Pixmap(_),
                ..
            } => match color {
                ColorType::RGB(_) => (),
                _ => {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        "PPM format only support ColorType::RGB",
                    ))
                }
            },
            PNMHeader {
                decoded:
                    HeaderRecord::Arbitrary(ArbitraryHeader {
                        depth,
                        ref tupltype,
                        ..
                    }),
                ..
            } => match (tupltype, color) {
                (&Some(ArbitraryTuplType::BlackAndWhite), ColorType::Gray(_)) => (),
                (&Some(ArbitraryTuplType::BlackAndWhiteAlpha), ColorType::GrayA(_)) => (),

                (&Some(ArbitraryTuplType::Grayscale), ColorType::Gray(_)) => (),
                (&Some(ArbitraryTuplType::GrayscaleAlpha), ColorType::GrayA(_)) => (),

                (&Some(ArbitraryTuplType::RGB), ColorType::RGB(_)) => (),
                (&Some(ArbitraryTuplType::RGBAlpha), ColorType::RGBA(_)) => (),

                (&None, _) if depth == components => (),
                (&Some(ArbitraryTuplType::Custom(_)), _) if depth == components => (),
                _ if depth != components => {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        format!("Depth mismatch: header {} vs. color {}", depth, components),
                    ))
                }
                _ => {
                    return Err(io::Error::new(
                        io::ErrorKind::InvalidInput,
                        "Invalid color type for selected PAM color type",
                    ))
                }
            },
        }

        Ok(CheckedHeaderColor {
            dimensions: self,
            color,
        })
    }
}

impl<'a> CheckedHeaderColor<'a> {
    fn check_sample_values(self, image: FlatSamples<'a>) -> io::Result<CheckedHeader<'a>> {
        let header_maxval = match self.dimensions.unchecked.header.decoded {
            HeaderRecord::Bitmap(_) => 1,
            HeaderRecord::Graymap(GraymapHeader { maxwhite, .. }) => maxwhite,
            HeaderRecord::Pixmap(PixmapHeader { maxval, .. }) => maxval,
            HeaderRecord::Arbitrary(ArbitraryHeader { maxval, .. }) => maxval,
        };

        // We trust the image color bit count to be correct at least.
        let max_sample = match self.color {
            // Protects against overflows from shifting and gives a better error.
            ColorType::Gray(n)
            | ColorType::GrayA(n)
            | ColorType::Palette(n)
            | ColorType::RGB(n)
            | ColorType::RGBA(n)
            | ColorType::BGR(n)
            | ColorType::BGRA(n) if n > 16 =>
            {
                return Err(io::Error::new(
                    io::ErrorKind::InvalidInput,
                    "Encoding colors with a bit depth greater 16 not supported",
                ))
            }
            ColorType::Gray(n)
            | ColorType::GrayA(n)
            | ColorType::Palette(n)
            | ColorType::RGB(n)
            | ColorType::RGBA(n)
            | ColorType::BGR(n)
            | ColorType::BGRA(n) => (1 << n) - 1,
        };

        // Avoid the performance heavy check if possible, e.g. if the header has been chosen by us.
        if header_maxval < max_sample && !image.all_smaller(header_maxval) {
            return Err(io::Error::new(
                io::ErrorKind::InvalidInput,
                "Sample value greater than allowed for chosen header",
            ));
        }

        let encoding = image.encoding_for(&self.dimensions.unchecked.header.decoded);

        let image = CheckedImageBuffer::check(
            image,
            self.dimensions.width,
            self.dimensions.height,
            self.color,
        )?;

        Ok(CheckedHeader {
            color: self,
            encoding,
            _image: image,
        })
    }
}

impl<'a> CheckedHeader<'a> {
    fn write_header(self, writer: &mut dyn Write) -> io::Result<TupleEncoding<'a>> {
        self.header().write(writer)?;
        Ok(self.encoding)
    }

    fn header(&self) -> &PNMHeader {
        self.color.dimensions.unchecked.header
    }
}

struct SampleWriter<'a>(&'a mut dyn Write);

impl<'a> SampleWriter<'a> {
    fn write_samples_ascii<V>(self, samples: V) -> io::Result<()>
    where
        V: Iterator,
        V::Item: fmt::Display,
    {
        let mut auto_break_writer = AutoBreak::new(self.0, 70);
        for value in samples {
            write!(auto_break_writer, "{} ", value)?;
        }
        auto_break_writer.flush()
    }

    fn write_pbm_bits<V>(self, samples: &[V], width: u32) -> io::Result<()>
    /* Default gives 0 for all primitives. TODO: replace this with `Zeroable` once it hits stable */
    where
        V: Default + Eq + Copy,
    {
        // The length of an encoded scanline
        let line_width = (width - 1) / 8 + 1;

        // We'll be writing single bytes, so buffer
        let mut line_buffer = Vec::with_capacity(line_width as usize);

        for line in samples.chunks(width as usize) {
            for byte_bits in line.chunks(8) {
                let mut byte = 0u8;
                for i in 0..8 {
                    // Black pixels are encoded as 1s
                    if let Some(&v) = byte_bits.get(i) {
                        if v == V::default() {
                            byte |= 1u8 << (7 - i)
                        }
                    }
                }
                line_buffer.push(byte)
            }
            self.0.write_all(line_buffer.as_slice())?;
            line_buffer.clear();
        }

        self.0.flush()
    }
}

impl<'a> FlatSamples<'a> {
    fn len(&self) -> usize {
        match *self {
            FlatSamples::U8(arr) => arr.len(),
            FlatSamples::U16(arr) => arr.len(),
        }
    }

    fn all_smaller(&self, max_val: u32) -> bool {
        match *self {
            FlatSamples::U8(arr) => arr.iter().any(|&val| u32::from(val) > max_val),
            FlatSamples::U16(arr) => arr.iter().any(|&val| u32::from(val) > max_val),
        }
    }

    fn encoding_for(&self, header: &HeaderRecord) -> TupleEncoding<'a> {
        match *header {
            HeaderRecord::Bitmap(BitmapHeader {
                encoding: SampleEncoding::Binary,
                width,
                ..
            }) => TupleEncoding::PbmBits {
                samples: *self,
                width,
            },

            HeaderRecord::Bitmap(BitmapHeader {
                encoding: SampleEncoding::Ascii,
                ..
            }) => TupleEncoding::Ascii { samples: *self },

            HeaderRecord::Arbitrary(_) => TupleEncoding::Bytes { samples: *self },

            HeaderRecord::Graymap(GraymapHeader {
                encoding: SampleEncoding::Ascii,
                ..
            })
            | HeaderRecord::Pixmap(PixmapHeader {
                encoding: SampleEncoding::Ascii,
                ..
            }) => TupleEncoding::Ascii { samples: *self },

            HeaderRecord::Graymap(GraymapHeader {
                encoding: SampleEncoding::Binary,
                ..
            })
            | HeaderRecord::Pixmap(PixmapHeader {
                encoding: SampleEncoding::Binary,
                ..
            }) => TupleEncoding::Bytes { samples: *self },
        }
    }
}

impl<'a> From<&'a [u8]> for FlatSamples<'a> {
    fn from(samples: &'a [u8]) -> Self {
        FlatSamples::U8(samples)
    }
}

impl<'a> From<&'a [u16]> for FlatSamples<'a> {
    fn from(samples: &'a [u16]) -> Self {
        FlatSamples::U16(samples)
    }
}

impl<'a> TupleEncoding<'a> {
    fn write_image(&self, writer: &mut dyn Write) -> io::Result<()> {
        match *self {
            TupleEncoding::PbmBits {
                samples: FlatSamples::U8(samples),
                width,
            } => SampleWriter(writer).write_pbm_bits(samples, width),
            TupleEncoding::PbmBits {
                samples: FlatSamples::U16(samples),
                width,
            } => SampleWriter(writer).write_pbm_bits(samples, width),

            TupleEncoding::Bytes {
                samples: FlatSamples::U8(samples),
            } => writer.write_all(samples),
            TupleEncoding::Bytes {
                samples: FlatSamples::U16(samples),
            } => samples
                .iter()
                .map(|&sample| writer.write_u16::<BigEndian>(sample))
                .collect(),

            TupleEncoding::Ascii {
                samples: FlatSamples::U8(samples),
            } => SampleWriter(writer).write_samples_ascii(samples.iter()),
            TupleEncoding::Ascii {
                samples: FlatSamples::U16(samples),
            } => SampleWriter(writer).write_samples_ascii(samples.iter()),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn custom_header_and_color() {
        let data: [u8; 12] = [0, 0, 0, 1, 1, 1, 255, 255, 255, 0, 0, 0];

        let header = ArbitraryHeader {
            width: 2,
            height: 2,
            depth: 3,
            maxval: 255,
            tupltype: Some(ArbitraryTuplType::Custom("Palette".to_string())),
        };

        let mut output = Vec::new();

        PNMEncoder::new(&mut output)
            .with_header(header.into())
            .encode(&data[..], 2, 2, ColorType::Palette(8))
            .expect("Failed encoding custom color value");
    }
}
