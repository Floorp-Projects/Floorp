use std::io;

/// The kind of encoding used to store sample values
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum SampleEncoding {
    /// Samples are unsigned binary integers in big endian
    Binary,

    /// Samples are encoded as decimal ascii strings separated by whitespace
    Ascii,
}

/// Denotes the category of the magic number
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum PNMSubtype {
    /// Magic numbers P1 and P4
    Bitmap(SampleEncoding),

    /// Magic numbers P2 and P5
    Graymap(SampleEncoding),

    /// Magic numbers P3 and P6
    Pixmap(SampleEncoding),

    /// Magic number P7
    ArbitraryMap,
}

/// Stores the complete header data of a file.
///
/// Internally, provides mechanisms for lossless reencoding. After reading a file with the decoder
/// it is possible to recover the header and construct an encoder. Using the encoder on the just
/// loaded image should result in a byte copy of the original file (for single image pnms without
/// additional trailing data).
pub struct PNMHeader {
    pub(crate) decoded: HeaderRecord,
    pub(crate) encoded: Option<Vec<u8>>,
}

pub(crate) enum HeaderRecord {
    Bitmap(BitmapHeader),
    Graymap(GraymapHeader),
    Pixmap(PixmapHeader),
    Arbitrary(ArbitraryHeader),
}

/// Header produced by a `pbm` file ("Portable Bit Map")
#[derive(Clone, Copy, Debug)]
pub struct BitmapHeader {
    /// Binary or Ascii encoded file
    pub encoding: SampleEncoding,

    /// Height of the image file
    pub height: u32,

    /// Width of the image file
    pub width: u32,
}

/// Header produced by a `pgm` file ("Portable Gray Map")
#[derive(Clone, Copy, Debug)]
pub struct GraymapHeader {
    /// Binary or Ascii encoded file
    pub encoding: SampleEncoding,

    /// Height of the image file
    pub height: u32,

    /// Width of the image file
    pub width: u32,

    /// Maximum sample value within the image
    pub maxwhite: u32,
}

/// Header produced by a `ppm` file ("Portable Pixel Map")
#[derive(Clone, Copy, Debug)]
pub struct PixmapHeader {
    /// Binary or Ascii encoded file
    pub encoding: SampleEncoding,

    /// Height of the image file
    pub height: u32,

    /// Width of the image file
    pub width: u32,

    /// Maximum sample value within the image
    pub maxval: u32,
}

/// Header produced by a `pam` file ("Portable Arbitrary Map")
#[derive(Clone, Debug)]
pub struct ArbitraryHeader {
    /// Height of the image file
    pub height: u32,

    /// Width of the image file
    pub width: u32,

    /// Number of color channels
    pub depth: u32,

    /// Maximum sample value within the image
    pub maxval: u32,

    /// Color interpretation of image pixels
    pub tupltype: Option<ArbitraryTuplType>,
}

/// Standardized tuple type specifiers in the header of a `pam`.
#[derive(Clone, Debug)]
pub enum ArbitraryTuplType {
    /// Pixels are either black (0) or white (1)
    BlackAndWhite,

    /// Pixels are either black (0) or white (1) and a second alpha channel
    BlackAndWhiteAlpha,

    /// Pixels represent the amount of white
    Grayscale,

    /// Grayscale with an additional alpha channel
    GrayscaleAlpha,

    /// Three channels: Red, Green, Blue
    RGB,

    /// Four channels: Red, Green, Blue, Alpha
    RGBAlpha,

    /// An image format which is not standardized
    Custom(String),
}

impl PNMSubtype {
    /// Get the two magic constant bytes corresponding to this format subtype.
    pub fn magic_constant(self) -> &'static [u8; 2] {
        match self {
            PNMSubtype::Bitmap(SampleEncoding::Ascii) => b"P1",
            PNMSubtype::Graymap(SampleEncoding::Ascii) => b"P2",
            PNMSubtype::Pixmap(SampleEncoding::Ascii) => b"P3",
            PNMSubtype::Bitmap(SampleEncoding::Binary) => b"P4",
            PNMSubtype::Graymap(SampleEncoding::Binary) => b"P5",
            PNMSubtype::Pixmap(SampleEncoding::Binary) => b"P6",
            PNMSubtype::ArbitraryMap => b"P7",
        }
    }

    /// Whether samples are stored as binary or as decimal ascii
    pub fn sample_encoding(self) -> SampleEncoding {
        match self {
            PNMSubtype::ArbitraryMap => SampleEncoding::Binary,
            PNMSubtype::Bitmap(enc) => enc,
            PNMSubtype::Graymap(enc) => enc,
            PNMSubtype::Pixmap(enc) => enc,
        }
    }
}

impl PNMHeader {
    /// Retrieve the format subtype from which the header was created.
    pub fn subtype(&self) -> PNMSubtype {
        match self.decoded {
            HeaderRecord::Bitmap(BitmapHeader { encoding, .. }) => PNMSubtype::Bitmap(encoding),
            HeaderRecord::Graymap(GraymapHeader { encoding, .. }) => PNMSubtype::Graymap(encoding),
            HeaderRecord::Pixmap(PixmapHeader { encoding, .. }) => PNMSubtype::Pixmap(encoding),
            HeaderRecord::Arbitrary(ArbitraryHeader { .. }) => PNMSubtype::ArbitraryMap,
        }
    }

    /// The width of the image this header is for.
    pub fn width(&self) -> u32 {
        match self.decoded {
            HeaderRecord::Bitmap(BitmapHeader { width, .. }) => width,
            HeaderRecord::Graymap(GraymapHeader { width, .. }) => width,
            HeaderRecord::Pixmap(PixmapHeader { width, .. }) => width,
            HeaderRecord::Arbitrary(ArbitraryHeader { width, .. }) => width,
        }
    }

    /// The height of the image this header is for.
    pub fn height(&self) -> u32 {
        match self.decoded {
            HeaderRecord::Bitmap(BitmapHeader { height, .. }) => height,
            HeaderRecord::Graymap(GraymapHeader { height, .. }) => height,
            HeaderRecord::Pixmap(PixmapHeader { height, .. }) => height,
            HeaderRecord::Arbitrary(ArbitraryHeader { height, .. }) => height,
        }
    }

    /// The biggest value a sample can have. In other words, the colour resolution.
    pub fn maximal_sample(&self) -> u32 {
        match self.decoded {
            HeaderRecord::Bitmap(BitmapHeader { .. }) => 1,
            HeaderRecord::Graymap(GraymapHeader { maxwhite, .. }) => maxwhite,
            HeaderRecord::Pixmap(PixmapHeader { maxval, .. }) => maxval,
            HeaderRecord::Arbitrary(ArbitraryHeader { maxval, .. }) => maxval,
        }
    }

    /// Retrieve the underlying bitmap header if any
    pub fn as_bitmap(&self) -> Option<&BitmapHeader> {
        match self.decoded {
            HeaderRecord::Bitmap(ref bitmap) => Some(bitmap),
            _ => None,
        }
    }

    /// Retrieve the underlying graymap header if any
    pub fn as_graymap(&self) -> Option<&GraymapHeader> {
        match self.decoded {
            HeaderRecord::Graymap(ref graymap) => Some(graymap),
            _ => None,
        }
    }

    /// Retrieve the underlying pixmap header if any
    pub fn as_pixmap(&self) -> Option<&PixmapHeader> {
        match self.decoded {
            HeaderRecord::Pixmap(ref pixmap) => Some(pixmap),
            _ => None,
        }
    }

    /// Retrieve the underlying arbitrary header if any
    pub fn as_arbitrary(&self) -> Option<&ArbitraryHeader> {
        match self.decoded {
            HeaderRecord::Arbitrary(ref arbitrary) => Some(arbitrary),
            _ => None,
        }
    }

    /// Write the header back into a binary stream
    pub fn write(&self, writer: &mut io::Write) -> io::Result<()> {
        writer.write_all(self.subtype().magic_constant())?;
        match *self {
            PNMHeader {
                encoded: Some(ref content),
                ..
            } => writer.write_all(content),
            PNMHeader {
                decoded:
                    HeaderRecord::Bitmap(BitmapHeader {
                        encoding: _encoding,
                        width,
                        height,
                    }),
                ..
            } => writeln!(writer, "\n{} {}", width, height),
            PNMHeader {
                decoded:
                    HeaderRecord::Graymap(GraymapHeader {
                        encoding: _encoding,
                        width,
                        height,
                        maxwhite,
                    }),
                ..
            } => writeln!(writer, "\n{} {} {}", width, height, maxwhite),
            PNMHeader {
                decoded:
                    HeaderRecord::Pixmap(PixmapHeader {
                        encoding: _encoding,
                        width,
                        height,
                        maxval,
                    }),
                ..
            } => writeln!(writer, "\n{} {} {}", width, height, maxval),
            PNMHeader {
                decoded:
                    HeaderRecord::Arbitrary(ArbitraryHeader {
                        width,
                        height,
                        depth,
                        maxval,
                        ref tupltype,
                    }),
                ..
            } => {
                #[allow(unused_assignments)]
                // Declared here so its lifetime exceeds the matching. This is a trivial
                // constructor, no allocation takes place and in the custom case we must allocate
                // regardless due to borrow. Still, the warnings checker does pick this up :/
                // Could use std::borrow::Cow instead but that really doesn't achieve anything but
                // increasing type complexity.
                let mut custom_fallback = String::new();

                let tupltype = match *tupltype {
                    None => "",
                    Some(ArbitraryTuplType::BlackAndWhite) => "TUPLTYPE BLACKANDWHITE\n",
                    Some(ArbitraryTuplType::BlackAndWhiteAlpha) => "TUPLTYPE BLACKANDWHITE_ALPHA\n",
                    Some(ArbitraryTuplType::Grayscale) => "TUPLTYPE GRAYSCALE\n",
                    Some(ArbitraryTuplType::GrayscaleAlpha) => "TUPLTYPE GRAYSCALE_ALPHA\n",
                    Some(ArbitraryTuplType::RGB) => "TUPLTYPE RGB\n",
                    Some(ArbitraryTuplType::RGBAlpha) => "TUPLTYPE RGB_ALPHA\n",
                    Some(ArbitraryTuplType::Custom(ref custom)) => {
                        custom_fallback = format!("TUPLTYPE {}", custom);
                        &custom_fallback
                    }
                };

                writeln!(
                    writer,
                    "\nWIDTH {}\nHEIGHT {}\nDEPTH {}\nMAXVAL {}\n{}ENDHDR",
                    width, height, depth, maxval, tupltype
                )
            }
        }
    }
}

impl From<BitmapHeader> for PNMHeader {
    fn from(header: BitmapHeader) -> Self {
        PNMHeader {
            decoded: HeaderRecord::Bitmap(header),
            encoded: None,
        }
    }
}

impl From<GraymapHeader> for PNMHeader {
    fn from(header: GraymapHeader) -> Self {
        PNMHeader {
            decoded: HeaderRecord::Graymap(header),
            encoded: None,
        }
    }
}

impl From<PixmapHeader> for PNMHeader {
    fn from(header: PixmapHeader) -> Self {
        PNMHeader {
            decoded: HeaderRecord::Pixmap(header),
            encoded: None,
        }
    }
}

impl From<ArbitraryHeader> for PNMHeader {
    fn from(header: ArbitraryHeader) -> Self {
        PNMHeader {
            decoded: HeaderRecord::Arbitrary(header),
            encoded: None,
        }
    }
}
