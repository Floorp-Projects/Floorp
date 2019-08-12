//!  Decoding of DXT (S3TC) compression
//!
//!  DXT is an image format that supports lossy compression
//!
//!  # Related Links
//!  * <https://www.khronos.org/registry/OpenGL/extensions/EXT/EXT_texture_compression_s3tc.txt> - Description of the DXT compression OpenGL extensions.
//!
//!  Note: this module only implements bare DXT encoding/decoding, it does not parse formats that can contain DXT files like .dds

use std::io::{self, Read, Seek, SeekFrom, Write};

use color::ColorType;
use image::{self, ImageDecoder, ImageDecoderExt, ImageError, ImageReadBuffer, ImageResult, Progress};

/// What version of DXT compression are we using?
/// Note that DXT2 and DXT4 are left away as they're
/// just DXT3 and DXT5 with premultiplied alpha
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum DXTVariant {
    /// The DXT1 format. 48 bytes of RGB data in a 4x4 pixel square is
    /// compressed into an 8 byte block of DXT1 data
    DXT1,
    /// The DXT3 format. 64 bytes of RGBA data in a 4x4 pixel square is
    /// compressed into a 16 byte block of DXT3 data
    DXT3,
    /// The DXT5 format. 64 bytes of RGBA data in a 4x4 pixel square is
    /// compressed into a 16 byte block of DXT5 data
    DXT5,
}

impl DXTVariant {
    /// Returns the amount of bytes of raw image data
    /// that is encoded in a single DXTn block
    fn decoded_bytes_per_block(self) -> usize {
        match self {
            DXTVariant::DXT1 => 48,
            DXTVariant::DXT3 | DXTVariant::DXT5 => 64,
        }
    }

    /// Returns the amount of bytes per block of encoded DXTn data
    fn encoded_bytes_per_block(self) -> usize {
        match self {
            DXTVariant::DXT1 => 8,
            DXTVariant::DXT3 | DXTVariant::DXT5 => 16,
        }
    }

    /// Returns the colortype that is stored in this DXT variant
    pub fn colortype(self) -> ColorType {
        match self {
            DXTVariant::DXT1 => ColorType::RGB(8),
            DXTVariant::DXT3 | DXTVariant::DXT5 => ColorType::RGBA(8),
        }
    }
}

/// DXT decoder
pub struct DXTDecoder<R: Read> {
    inner: R,
    width_blocks: u32,
    height_blocks: u32,
    variant: DXTVariant,
    row: u32,
}

impl<R: Read> DXTDecoder<R> {
    /// Create a new DXT decoder that decodes from the stream ```r```.
    /// As DXT is often stored as raw buffers with the width/height
    /// somewhere else the width and height of the image need
    /// to be passed in ```width``` and ```height```, as well as the
    /// DXT variant in ```variant```.
    /// width and height are required to be powers of 2 and at least 4.
    /// otherwise an error will be returned
    pub fn new(
        r: R,
        width: u32,
        height: u32,
        variant: DXTVariant,
    ) -> Result<DXTDecoder<R>, ImageError> {
        if width % 4 != 0 || height % 4 != 0 {
            return Err(ImageError::DimensionError);
        }
        let width_blocks = width / 4;
        let height_blocks = height / 4;
        Ok(DXTDecoder {
            inner: r,
            width_blocks,
            height_blocks,
            variant,
            row: 0,
        })
    }

    fn read_scanline(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        assert_eq!(buf.len() as u64, self.scanline_bytes());

        let mut src =
            vec![0u8; self.variant.encoded_bytes_per_block() * self.width_blocks as usize];
        self.inner.read_exact(&mut src)?;
        match self.variant {
            DXTVariant::DXT1 => decode_dxt1_row(&src, buf),
            DXTVariant::DXT3 => decode_dxt3_row(&src, buf),
            DXTVariant::DXT5 => decode_dxt5_row(&src, buf),
        }
        self.row += 1;
        Ok(buf.len())
    }
}

// Note that, due to the way that DXT compression works, a scanline is considered to consist out of
// 4 lines of pixels.
impl<'a, R: 'a + Read> ImageDecoder<'a> for DXTDecoder<R> {
    type Reader = DXTReader<R>;

    fn dimensions(&self) -> (u64, u64) {
        (self.width_blocks as u64 * 4, self.height_blocks as u64 * 4)
    }

    fn colortype(&self) -> ColorType {
        self.variant.colortype()
    }

    fn scanline_bytes(&self) -> u64 {
        self.variant.decoded_bytes_per_block() as u64 * self.width_blocks as u64
    }

    fn into_reader(self) -> ImageResult<Self::Reader> {
        if self.total_bytes() > usize::max_value() as u64 {
            return Err(ImageError::InsufficientMemory);
        }

        Ok(DXTReader {
            buffer: ImageReadBuffer::new(self.scanline_bytes() as usize, self.total_bytes() as usize),
            decoder: self,
        })
    }

    fn read_image(mut self) -> ImageResult<Vec<u8>> {
        if self.total_bytes() > usize::max_value() as u64 {
            return Err(ImageError::InsufficientMemory);
        }

        let mut dest = vec![0u8; self.total_bytes() as usize];
        for chunk in dest.chunks_mut(self.scanline_bytes() as usize) {
            self.read_scanline(chunk)?;
        }
        Ok(dest)
    }
}

impl<'a, R: 'a + Read + Seek> ImageDecoderExt<'a> for DXTDecoder<R> {
    fn read_rect_with_progress<F: Fn(Progress)>(
        &mut self,
        x: u64,
        y: u64,
        width: u64,
        height: u64,
        buf: &mut [u8],
        progress_callback: F,
    ) -> ImageResult<()> {
        let encoded_scanline_bytes = self.variant.encoded_bytes_per_block() as u64
            * self.width_blocks as u64;

        let start = self.inner.seek(SeekFrom::Current(0))?;
        image::load_rect(x, y, width, height, buf, progress_callback, self,
                         |s, scanline| {
                             s.inner.seek(SeekFrom::Start(start + scanline * encoded_scanline_bytes))?;
                             Ok(())
                         },
                         |s, buf| s.read_scanline(buf))?;
        self.inner.seek(SeekFrom::Start(start))?;
        Ok(())
    }
}

/// DXT reader
pub struct DXTReader<R: Read> {
    buffer: ImageReadBuffer,
    decoder: DXTDecoder<R>,
}
impl<R: Read> Read for DXTReader<R> {
    fn read(&mut self, buf: &mut [u8]) -> io::Result<usize> {
        let ref mut decoder = &mut self.decoder;
        self.buffer.read(buf, |buf| decoder.read_scanline(buf))
    }
}

/// DXT encoder
pub struct DXTEncoder<W: Write> {
    w: W,
}

impl<W: Write> DXTEncoder<W> {
    /// Create a new encoder that writes its output to ```w```
    pub fn new(w: W) -> DXTEncoder<W> {
        DXTEncoder { w }
    }

    /// Encodes the image data ```data```
    /// that has dimensions ```width``` and ```height```
    /// in ```DXTVariant``` ```variant```
    /// data is assumed to be in variant.colortype()
    pub fn encode(
        mut self,
        data: &[u8],
        width: u32,
        height: u32,
        variant: DXTVariant,
    ) -> ImageResult<()> {
        if width % 4 != 0 || height % 4 != 0 {
            return Err(ImageError::DimensionError);
        }
        let width_blocks = width / 4;
        let height_blocks = height / 4;

        let stride = variant.decoded_bytes_per_block();

        assert!(data.len() >= width_blocks as usize * height_blocks as usize * stride);

        for chunk in data.chunks(width_blocks as usize * stride) {
            let data = match variant {
                DXTVariant::DXT1 => encode_dxt1_row(chunk),
                DXTVariant::DXT3 => encode_dxt3_row(chunk),
                DXTVariant::DXT5 => encode_dxt5_row(chunk),
            };
            self.w.write_all(&data)?;
        }
        Ok(())
    }
}

/**
 * Actual encoding/decoding logic below.
 */
use std::mem::swap;

type Rgb = [u8; 3];

/// decodes a 5-bit R, 6-bit G, 5-bit B 16-bit packed color value into 8-bit RGB
/// mapping is done so min/max range values are preserved. So for 5-bit
/// values 0x00 -> 0x00 and 0x1F -> 0xFF
fn enc565_decode(value: u16) -> Rgb {
    let red = (value >> 11) & 0x1F;
    let green = (value >> 5) & 0x3F;
    let blue = (value) & 0x1F;
    [
        (red * 0xFF / 0x1F) as u8,
        (green * 0xFF / 0x3F) as u8,
        (blue * 0xFF / 0x1F) as u8,
    ]
}

/// encodes an 8-bit RGB value into a 5-bit R, 6-bit G, 5-bit B 16-bit packed color value
/// mapping preserves min/max values. It is guaranteed that i == encode(decode(i)) for all i
fn enc565_encode(rgb: Rgb) -> u16 {
    let red = (u16::from(rgb[0]) * 0x1F + 0x7E) / 0xFF;
    let green = (u16::from(rgb[1]) * 0x3F + 0x7E) / 0xFF;
    let blue = (u16::from(rgb[2]) * 0x1F + 0x7E) / 0xFF;
    (red << 11) | (green << 5) | blue
}

/// utility function: squares a value
fn square(a: i32) -> i32 {
    a * a
}

/// returns the squared error between two RGB values
fn diff(a: Rgb, b: Rgb) -> i32 {
    square(i32::from(a[0]) - i32::from(b[0])) + square(i32::from(a[1]) - i32::from(b[1]))
        + square(i32::from(a[2]) - i32::from(b[2]))
}

/*
 * Functions for decoding DXT compression
 */

/// Constructs the DXT5 alpha lookup table from the two alpha entries
/// if alpha0 > alpha1, constructs a table of [a0, a1, 6 linearly interpolated values from a0 to a1]
/// if alpha0 <= alpha1, constructs a table of [a0, a1, 4 linearly interpolated values from a0 to a1, 0, 0xFF]
fn alpha_table_dxt5(alpha0: u8, alpha1: u8) -> [u8; 8] {
    let mut table = [alpha0, alpha1, 0, 0, 0, 0, 0, 0xFF];
    if alpha0 > alpha1 {
        for i in 2..8u16 {
            table[i as usize] =
                (((8 - i) * u16::from(alpha0) + (i - 1) * u16::from(alpha1)) / 7) as u8;
        }
    } else {
        for i in 2..6u16 {
            table[i as usize] =
                (((6 - i) * u16::from(alpha0) + (i - 1) * u16::from(alpha1)) / 5) as u8;
        }
    }
    table
}

/// decodes an 8-byte dxt color block into the RGB channels of a 16xRGB or 16xRGBA block.
/// source should have a length of 8, dest a length of 48 (RGB) or 64 (RGBA)
fn decode_dxt_colors(source: &[u8], dest: &mut [u8]) {
    // sanity checks, also enable the compiler to elide all following bound checks
    assert!(source.len() == 8 && (dest.len() == 48 || dest.len() == 64));
    // calculate pitch to store RGB values in dest (3 for RGB, 4 for RGBA)
    let pitch = dest.len() / 16;

    // extract color data
    let color0 = u16::from(source[0]) | (u16::from(source[1]) << 8);
    let color1 = u16::from(source[2]) | (u16::from(source[3]) << 8);
    let color_table = u32::from(source[4]) | (u32::from(source[5]) << 8)
        | (u32::from(source[6]) << 16) | (u32::from(source[7]) << 24);
    // let color_table = source[4..8].iter().rev().fold(0, |t, &b| (t << 8) | b as u32);

    // decode the colors to rgb format
    let mut colors = [[0; 3]; 4];
    colors[0] = enc565_decode(color0);
    colors[1] = enc565_decode(color1);

    // determine color interpolation method
    if color0 > color1 {
        // linearly interpolate the other two color table entries
        for i in 0..3 {
            colors[2][i] = ((u16::from(colors[0][i]) * 2 + u16::from(colors[1][i]) + 1) / 3) as u8;
            colors[3][i] = ((u16::from(colors[0][i]) + u16::from(colors[1][i]) * 2 + 1) / 3) as u8;
        }
    } else {
        // linearly interpolate one other entry, keep the other at 0
        for i in 0..3 {
            colors[2][i] = ((u16::from(colors[0][i]) + u16::from(colors[1][i]) + 1) / 2) as u8;
        }
    }

    // serialize the result. Every color is determined by looking up
    // two bits in color_table which identify which color to actually pick from the 4 possible colors
    for i in 0..16 {
        dest[i * pitch..i * pitch + 3]
            .copy_from_slice(&colors[(color_table >> (i * 2)) as usize & 3]);
    }
}

/// Decodes a 16-byte bock of dxt5 data to a 16xRGBA block
fn decode_dxt5_block(source: &[u8], dest: &mut [u8]) {
    assert!(source.len() == 16 && dest.len() == 64);

    // extract alpha index table (stored as little endian 64-bit value)
    let alpha_table = source[2..8]
        .iter()
        .rev()
        .fold(0, |t, &b| (t << 8) | u64::from(b));

    // alhpa level decode
    let alphas = alpha_table_dxt5(source[0], source[1]);

    // serialize alpha
    for i in 0..16 {
        dest[i * 4 + 3] = alphas[(alpha_table >> (i * 3)) as usize & 7];
    }

    // handle colors
    decode_dxt_colors(&source[8..16], dest);
}

/// Decodes a 16-byte bock of dxt3 data to a 16xRGBA block
fn decode_dxt3_block(source: &[u8], dest: &mut [u8]) {
    assert!(source.len() == 16 && dest.len() == 64);

    // extract alpha index table (stored as little endian 64-bit value)
    let alpha_table = source[0..8]
        .iter()
        .rev()
        .fold(0, |t, &b| (t << 8) | u64::from(b));

    // serialize alpha (stored as 4-bit values)
    for i in 0..16 {
        dest[i * 4 + 3] = ((alpha_table >> (i * 4)) as u8 & 0xF) * 0x11;
    }

    // handle colors
    decode_dxt_colors(&source[8..16], dest);
}

/// Decodes a 8-byte bock of dxt5 data to a 16xRGB block
fn decode_dxt1_block(source: &[u8], dest: &mut [u8]) {
    assert!(source.len() == 8 && dest.len() == 48);
    decode_dxt_colors(&source, dest);
}

/// Decode a row of DXT1 data to four rows of RGBA data.
/// source.len() should be a multiple of 8, otherwise this panics.
fn decode_dxt1_row(source: &[u8], dest: &mut [u8]) {
    assert!(source.len() % 8 == 0);
    let block_count = source.len() / 8;
    assert!(dest.len() >= block_count * 48);

    // contains the 16 decoded pixels per block
    let mut decoded_block = [0u8; 48];

    for (x, encoded_block) in source.chunks(8).enumerate() {
        decode_dxt1_block(encoded_block, &mut decoded_block);

        // copy the values from the decoded block to linewise RGB layout
        for line in 0..4 {
            let offset = (block_count * line + x) * 12;
            dest[offset..offset + 12].copy_from_slice(&decoded_block[line * 12..(line + 1) * 12]);
        }
    }
}

/// Decode a row of DXT3 data to four rows of RGBA data.
/// source.len() should be a multiple of 16, otherwise this panics.
fn decode_dxt3_row(source: &[u8], dest: &mut [u8]) {
    assert!(source.len() % 16 == 0);
    let block_count = source.len() / 16;
    assert!(dest.len() >= block_count * 64);

    // contains the 16 decoded pixels per block
    let mut decoded_block = [0u8; 64];

    for (x, encoded_block) in source.chunks(16).enumerate() {
        decode_dxt3_block(encoded_block, &mut decoded_block);

        // copy the values from the decoded block to linewise RGB layout
        for line in 0..4 {
            let offset = (block_count * line + x) * 16;
            dest[offset..offset + 16].copy_from_slice(&decoded_block[line * 16..(line + 1) * 16]);
        }
    }
}

/// Decode a row of DXT5 data to four rows of RGBA data.
/// source.len() should be a multiple of 16, otherwise this panics.
fn decode_dxt5_row(source: &[u8], dest: &mut [u8]) {
    assert!(source.len() % 16 == 0);
    let block_count = source.len() / 16;
    assert!(dest.len() >= block_count * 64);

    // contains the 16 decoded pixels per block
    let mut decoded_block = [0u8; 64];

    for (x, encoded_block) in source.chunks(16).enumerate() {
        decode_dxt5_block(encoded_block, &mut decoded_block);

        // copy the values from the decoded block to linewise RGB layout
        for line in 0..4 {
            let offset = (block_count * line + x) * 16;
            dest[offset..offset + 16].copy_from_slice(&decoded_block[line * 16..(line + 1) * 16]);
        }
    }
}

/*
 * Functions for encoding DXT compression
 */

/// Tries to perform the color encoding part of dxt compression
/// the approach taken is simple, it picks unique combinations
/// of the colors present in the block, and attempts to encode the
/// block with each, picking the encoding that yields the least
/// squared error out of all of them.
///
/// This could probably be faster but is already reasonably fast
/// and a good reference impl to optimize others against.
///
/// Another way to perform this analysis would be to perform a
/// singular value decomposition of the different colors, and
/// then pick 2 points on this line as the base colors. But
/// this is still rather unwieldly math and has issues
/// with the 3-linear-colors-and-0 case, it's also worse
/// at conserving the original colors.
///
/// source: should be RGBAx16 or RGBx16 bytes of data,
/// dest 8 bytes of resulting encoded color data
fn encode_dxt_colors(source: &[u8], dest: &mut [u8]) {
    // sanity checks and determine stride when parsing the source data
    assert!((source.len() == 64 || source.len() == 48) && dest.len() == 8);
    let stride = source.len() / 16;

    // reference colors array
    let mut colors = [[0u8; 3]; 4];

    // Put the colors we're going to be processing in an array with pure RGB layout
    // note: we reverse the pixel order here. The reason for this is found in the inner quantization loop.
    let mut targets = [[0u8; 3]; 16];
    for (s, d) in source.chunks(stride).rev().zip(&mut targets) {
        *d = [s[0], s[1], s[2]];
    }

    // and a set of colors to pick from.
    let mut colorspace = targets.to_vec();

    // roundtrip all colors through the r5g6b5 encoding
    for rgb in &mut colorspace {
        *rgb = enc565_decode(enc565_encode(*rgb));
    }

    // and deduplicate the set of colors to choose from as the algorithm is O(N^2) in this
    colorspace.dedup();

    // in case of slight gradients it can happen that there's only one entry left in the color table.
    // as the resulting banding can be quite bad if we would just left the block at the closest
    // encodable color, we have a special path here that tries to emulate the wanted color
    // using the linear interpolation between gradients
    if colorspace.len() == 1 {
        // the base color we got from colorspace reduction
        let ref_rgb = colorspace[0];
        // the unreduced color in this block that's the furthest away from the actual block
        let mut rgb = targets
            .iter()
            .cloned()
            .max_by_key(|rgb| diff(*rgb, ref_rgb))
            .unwrap();
        // amplify differences by 2.5, which should push them to the next quantized value
        // if possible without overshoot
        for i in 0..3 {
            rgb[i] =
                ((i16::from(rgb[i]) - i16::from(ref_rgb[i])) * 5 / 2 + i16::from(ref_rgb[i])) as u8;
        }

        // roundtrip it through quantization
        let encoded = enc565_encode(rgb);
        let rgb = enc565_decode(encoded);

        // in case this didn't land us a different color the best way to represent this field is
        // as a single color block
        if rgb == ref_rgb {
            dest[0] = encoded as u8;
            dest[1] = (encoded >> 8) as u8;

            for d in dest.iter_mut().take(8).skip(2) {
                *d = 0;
            }
            return;
        }

        // we did find a separate value: add it to the options so after one round of quantization
        // we're done
        colorspace.push(rgb);
    }

    // block quantization loop: we basically just try every possible combination, returning
    // the combination with the least squared error
    // stores the best candidate colors
    let mut chosen_colors = [[0; 3]; 4];
    // did this index table use the [0,0,0] variant
    let mut chosen_use_0 = false;
    // error calculated for the last entry
    let mut chosen_error = 0xFFFF_FFFFu32;

    // loop through unique permutations of the colorspace, where c1 != c2
    'search: for (i, &c1) in colorspace.iter().enumerate() {
        colors[0] = c1;

        for &c2 in &colorspace[0..i] {
            colors[1] = c2;

            // what's inside here is ran at most 120 times.
            for use_0 in 0..2 {
                // and 240 times here.

                if use_0 != 0 {
                    // interpolate one color, set the other to 0
                    for i in 0..3 {
                        colors[2][i] =
                            ((u16::from(colors[0][i]) + u16::from(colors[1][i]) + 1) / 2) as u8;
                    }
                    colors[3] = [0, 0, 0];
                } else {
                    // interpolate to get 2 more colors
                    for i in 0..3 {
                        colors[2][i] =
                            ((u16::from(colors[0][i]) * 2 + u16::from(colors[1][i]) + 1) / 3) as u8;
                        colors[3][i] =
                            ((u16::from(colors[0][i]) + u16::from(colors[1][i]) * 2 + 1) / 3) as u8;
                    }
                }

                // calculate the total error if we were to quantize the block with these color combinations
                // both these loops have statically known iteration counts and are well vectorizable
                // note that the inside of this can be run about 15360 times worst case, i.e. 960 times per
                // pixel.
                let total_error = targets
                    .iter()
                    .map(|t| colors.iter().map(|c| diff(*c, *t) as u32).min().unwrap())
                    .sum();

                // update the match if we found a better one
                if total_error < chosen_error {
                    chosen_colors = colors;
                    chosen_use_0 = use_0 != 0;
                    chosen_error = total_error;

                    // if we've got a perfect or at most 1 LSB off match, we're done
                    if total_error < 4 {
                        break 'search;
                    }
                }
            }
        }
    }

    // calculate the final indices
    // note that targets is already in reverse pixel order, to make the index computation easy.
    let mut chosen_indices = 0u32;
    for t in &targets {
        let (idx, _) = chosen_colors
            .iter()
            .enumerate()
            .min_by_key(|&(_, c)| diff(*c, *t))
            .unwrap();
        chosen_indices = (chosen_indices << 2) | idx as u32;
    }

    // encode the colors
    let mut color0 = enc565_encode(chosen_colors[0]);
    let mut color1 = enc565_encode(chosen_colors[1]);

    // determine encoding. Note that color0 == color1 is impossible at this point
    if color0 > color1 {
        if chosen_use_0 {
            swap(&mut color0, &mut color1);
            // Indexes are packed 2 bits wide, swap index 0/1 but preserve 2/3.
            let filter = (chosen_indices & 0xAAAA_AAAA) >> 1;
            chosen_indices ^= filter ^ 0x5555_5555;
        }
    } else if !chosen_use_0 {
        swap(&mut color0, &mut color1);
        // Indexes are packed 2 bits wide, swap index 0/1 and 2/3.
        chosen_indices ^= 0x5555_5555;
    }

    // encode everything.
    dest[0] = color0 as u8;
    dest[1] = (color0 >> 8) as u8;
    dest[2] = color1 as u8;
    dest[3] = (color1 >> 8) as u8;
    for i in 0..4 {
        dest[i + 4] = (chosen_indices >> (i * 8)) as u8;
    }
}

/// Encodes a buffer of 16 alpha bytes into a dxt5 alpha index table,
/// where the alpha table they are indexed against is created by
/// calling alpha_table_dxt5(alpha0, alpha1)
/// returns the resulting error and alpha table
fn encode_dxt5_alpha(alpha0: u8, alpha1: u8, alphas: &[u8; 16]) -> (i32, u64) {
    // create a table for the given alpha ranges
    let table = alpha_table_dxt5(alpha0, alpha1);
    let mut indices = 0u64;
    let mut total_error = 0i32;

    // least error brute force search
    for (i, &a) in alphas.iter().enumerate() {
        let (index, error) = table
            .iter()
            .enumerate()
            .map(|(i, &e)| (i, square(i32::from(e) - i32::from(a))))
            .min_by_key(|&(_, e)| e)
            .unwrap();
        total_error += error;
        indices |= (index as u64) << (i * 3);
    }

    (total_error, indices)
}

/// Encodes a RGBAx16 sequence of bytes to a 16 bytes DXT5 block
fn encode_dxt5_block(source: &[u8], dest: &mut [u8]) {
    assert!(source.len() == 64 && dest.len() == 16);

    // perform dxt color encoding
    encode_dxt_colors(source, &mut dest[8..16]);

    // copy out the alpha bytes
    let mut alphas = [0; 16];
    for i in 0..16 {
        alphas[i] = source[i * 4 + 3];
    }

    // try both alpha compression methods, see which has the least error.
    let alpha07 = alphas.iter().cloned().min().unwrap();
    let alpha17 = alphas.iter().cloned().max().unwrap();
    let (error7, indices7) = encode_dxt5_alpha(alpha07, alpha17, &alphas);

    // if all alphas are 0 or 255 it doesn't particularly matter what we do here.
    let alpha05 = alphas
        .iter()
        .cloned()
        .filter(|&i| i != 255)
        .max()
        .unwrap_or(255);
    let alpha15 = alphas
        .iter()
        .cloned()
        .filter(|&i| i != 0)
        .min()
        .unwrap_or(0);
    let (error5, indices5) = encode_dxt5_alpha(alpha05, alpha15, &alphas);

    // pick the best one, encode the min/max values
    let mut alpha_table = if error5 < error7 {
        dest[0] = alpha05;
        dest[1] = alpha15;
        indices5
    } else {
        dest[0] = alpha07;
        dest[1] = alpha17;
        indices7
    };

    // encode the alphas
    for byte in dest[2..8].iter_mut() {
        *byte = alpha_table as u8;
        alpha_table >>= 8;
    }
}

/// Encodes a RGBAx16 sequence of bytes into a 16 bytes DXT3 block
fn encode_dxt3_block(source: &[u8], dest: &mut [u8]) {
    assert!(source.len() == 64 && dest.len() == 16);

    // perform dxt color encoding
    encode_dxt_colors(source, &mut dest[8..16]);

    // DXT3 alpha compression is very simple, just round towards the nearest value

    // index the alpha values into the 64bit alpha table
    let mut alpha_table = 0u64;
    for i in 0..16 {
        let alpha = u64::from(source[i * 4 + 3]);
        let alpha = (alpha + 0x8) / 0x11;
        alpha_table |= alpha << (i * 4);
    }

    // encode the alpha values
    for byte in &mut dest[0..8] {
        *byte = alpha_table as u8;
        alpha_table >>= 8;
    }
}

/// Encodes a RGBx16 sequence of bytes into a 8 bytes DXT1 block
fn encode_dxt1_block(source: &[u8], dest: &mut [u8]) {
    assert!(source.len() == 48 && dest.len() == 8);

    // perform dxt color encoding
    encode_dxt_colors(source, dest);
}

/// Decode a row of DXT1 data to four rows of RGBA data.
/// source.len() should be a multiple of 8, otherwise this panics.
fn encode_dxt1_row(source: &[u8]) -> Vec<u8> {
    assert!(source.len() % 48 == 0);
    let block_count = source.len() / 48;

    let mut dest = vec![0u8; block_count * 8];
    // contains the 16 decoded pixels per block
    let mut decoded_block = [0u8; 48];

    for (x, encoded_block) in dest.chunks_mut(8).enumerate() {
        // copy the values from the decoded block to linewise RGB layout
        for line in 0..4 {
            let offset = (block_count * line + x) * 12;
            decoded_block[line * 12..(line + 1) * 12].copy_from_slice(&source[offset..offset + 12]);
        }

        encode_dxt1_block(&decoded_block, encoded_block);
    }
    dest
}

/// Decode a row of DXT3 data to four rows of RGBA data.
/// source.len() should be a multiple of 16, otherwise this panics.
fn encode_dxt3_row(source: &[u8]) -> Vec<u8> {
    assert!(source.len() % 64 == 0);
    let block_count = source.len() / 64;

    let mut dest = vec![0u8; block_count * 16];
    // contains the 16 decoded pixels per block
    let mut decoded_block = [0u8; 64];

    for (x, encoded_block) in dest.chunks_mut(16).enumerate() {
        // copy the values from the decoded block to linewise RGB layout
        for line in 0..4 {
            let offset = (block_count * line + x) * 16;
            decoded_block[line * 16..(line + 1) * 16].copy_from_slice(&source[offset..offset + 16]);
        }

        encode_dxt3_block(&decoded_block, encoded_block);
    }
    dest
}

/// Decode a row of DXT5 data to four rows of RGBA data.
/// source.len() should be a multiple of 16, otherwise this panics.
fn encode_dxt5_row(source: &[u8]) -> Vec<u8> {
    assert!(source.len() % 64 == 0);
    let block_count = source.len() / 64;

    let mut dest = vec![0u8; block_count * 16];
    // contains the 16 decoded pixels per block
    let mut decoded_block = [0u8; 64];

    for (x, encoded_block) in dest.chunks_mut(16).enumerate() {
        // copy the values from the decoded block to linewise RGB layout
        for line in 0..4 {
            let offset = (block_count * line + x) * 16;
            decoded_block[line * 16..(line + 1) * 16].copy_from_slice(&source[offset..offset + 16]);
        }

        encode_dxt5_block(&decoded_block, encoded_block);
    }
    dest
}
