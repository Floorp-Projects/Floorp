use super::scoped_threadpool::Pool;
use num_traits::cast::NumCast;
use num_traits::identities::Zero;
use std::mem;
#[cfg(test)]
use std::borrow::Cow;
use std::error::Error;
use std::io::{self, BufRead, Cursor, Seek};
use std::iter::Iterator;
use std::path::Path;
use Primitive;

use color::{ColorType, Rgb};
use image::{self, ImageDecoder, ImageDecoderExt, ImageError, ImageResult, Progress};

/// Adapter to conform to ```ImageDecoder``` trait
#[derive(Debug)]
pub struct HDRAdapter<R: BufRead> {
    inner: Option<HDRDecoder<R>>,
    data: Option<Vec<u8>>,
    meta: HDRMetadata,
}

impl<R: BufRead> HDRAdapter<R> {
    /// Creates adapter
    pub fn new(r: R) -> ImageResult<HDRAdapter<R>> {
        let decoder = try!(HDRDecoder::new(r));
        let meta = decoder.metadata();
        Ok(HDRAdapter {
            inner: Some(decoder),
            data: None,
            meta,
        })
    }

    /// Allows reading old Radiance HDR images
    pub fn new_nonstrict(r: R) -> ImageResult<HDRAdapter<R>> {
        let decoder = try!(HDRDecoder::with_strictness(r, false));
        let meta = decoder.metadata();
        Ok(HDRAdapter {
            inner: Some(decoder),
            data: None,
            meta,
        })
    }

    /// Read the actual data of the image, and store it in Self::data.
    fn read_image_data(&mut self) -> ImageResult<()> {
        match self.inner.take() {
            Some(decoder) => {
                let img: Vec<Rgb<u8>> = decoder.read_image_ldr()?;

                let len = img.len() * mem::size_of::<Rgb<u8>>(); // length in bytes
                let target = self.data.get_or_insert_with(|| Vec::with_capacity(len));
                target.clear();

                for Rgb { data } in img {
                    target.extend_from_slice(&data);
                }

                Ok(())
            }
            None => Err(ImageError::ImageEnd),
        }
    }

}

impl<R: BufRead> ImageDecoder for HDRAdapter<R> {
    type Reader = Cursor<Vec<u8>>;

    fn dimensions(&self) -> (u64, u64) {
        (self.meta.width as u64, self.meta.height as u64)
    }

    fn colortype(&self) -> ColorType {
        ColorType::RGB(8)
    }

    fn into_reader(self) -> ImageResult<Self::Reader> {
        Ok(Cursor::new(self.read_image()?))
    }

    fn read_image(mut self) -> ImageResult<Vec<u8>> {
        if let Some(data) = self.data {
            return Ok(data);
        }

        self.read_image_data()?;
        Ok(self.data.unwrap())
    }
}

impl<R: BufRead + Seek> ImageDecoderExt for HDRAdapter<R> {
    fn read_rect_with_progress<F: Fn(Progress)>(
        &mut self,
        x: u64,
        y: u64,
        width: u64,
        height: u64,
        buf: &mut [u8],
        progress_callback: F,
    ) -> ImageResult<()> {
        if self.data.is_none() {
            self.read_image_data()?;
        }

        image::load_rect(x, y, width, height, buf, progress_callback, self, |_, _| unreachable!(),
                         |s, buf| {
                             buf.copy_from_slice(&*s.data.as_ref().unwrap());
                             Ok(buf.len())
                         })
    }
}

/// Radiance HDR file signature
pub const SIGNATURE: &[u8] = b"#?RADIANCE";
const SIGNATURE_LENGTH: usize = 10;

/// An Radiance HDR decoder
#[derive(Debug)]
pub struct HDRDecoder<R> {
    r: R,
    width: u32,
    height: u32,
    meta: HDRMetadata,
}

/// Refer to [wikipedia](https://en.wikipedia.org/wiki/RGBE_image_format)
#[repr(C)]
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct RGBE8Pixel {
    /// Color components
    pub c: [u8; 3],
    /// Exponent
    pub e: u8,
}

/// Creates ```RGBE8Pixel``` from components
pub fn rgbe8(r: u8, g: u8, b: u8, e: u8) -> RGBE8Pixel {
    RGBE8Pixel { c: [r, g, b], e }
}

impl RGBE8Pixel {
    /// Converts ```RGBE8Pixel``` into ```Rgb<f32>``` linearly
    #[inline]
    pub fn to_hdr(self) -> Rgb<f32> {
        if self.e == 0 {
            Rgb([0.0, 0.0, 0.0])
        } else {
            //            let exp = f32::ldexp(1., self.e as isize - (128 + 8)); // unstable
            let exp = f32::exp2(<f32 as From<_>>::from(self.e) - (128.0 + 8.0));
            Rgb([
                exp * <f32 as From<_>>::from(self.c[0]),
                exp * <f32 as From<_>>::from(self.c[1]),
                exp * <f32 as From<_>>::from(self.c[2]),
            ])
        }
    }

    /// Converts ```RGBE8Pixel``` into ```Rgb<T>``` with scale=1 and gamma=2.2
    ///
    /// color_ldr = (color_hdr*scale)<sup>gamma</sup>
    ///
    /// # Panic
    ///
    /// Panics when ```T::max_value()``` cannot be represented as f32.
    #[inline]
    pub fn to_ldr<T: Primitive + Zero>(self) -> Rgb<T> {
        self.to_ldr_scale_gamma(1.0, 2.2)
    }

    /// Converts RGBE8Pixel into Rgb<T> using provided scale and gamma
    ///
    /// color_ldr = (color_hdr*scale)<sup>gamma</sup>
    ///
    /// # Panic
    ///
    /// Panics when T::max_value() cannot be represented as f32.
    /// Panics when scale or gamma is NaN
    #[inline]
    pub fn to_ldr_scale_gamma<T: Primitive + Zero>(self, scale: f32, gamma: f32) -> Rgb<T> {
        let Rgb { data } = self.to_hdr();
        let (r, g, b) = (data[0], data[1], data[2]);
        #[inline]
        fn sg<T: Primitive + Zero>(v: f32, scale: f32, gamma: f32) -> T {
            let t_max = T::max_value();
            // Disassembly shows that t_max_f32 is compiled into constant
            let t_max_f32: f32 = NumCast::from(t_max)
                .expect("to_ldr_scale_gamma: maximum value of type is not representable as f32");
            let fv = f32::powf(v * scale, gamma) * t_max_f32 + 0.5;
            if fv < 0.0 {
                T::zero()
            } else if fv > t_max_f32 {
                t_max
            } else {
                NumCast::from(fv)
                    .expect("to_ldr_scale_gamma: cannot convert f32 to target type. NaN?")
            }
        }
        Rgb([
            sg(r, scale, gamma),
            sg(g, scale, gamma),
            sg(b, scale, gamma),
        ])
    }
}

impl<R: BufRead> HDRDecoder<R> {
    /// Reads Radiance HDR image header from stream ```r```
    /// if the header is valid, creates HDRDecoder
    /// strict mode is enabled
    pub fn new(reader: R) -> ImageResult<HDRDecoder<R>> {
        HDRDecoder::with_strictness(reader, true)
    }

    /// Reads Radiance HDR image header from stream ```reader```,
    /// if the header is valid, creates ```HDRDecoder```.
    ///
    /// strict enables strict mode
    ///
    /// Warning! Reading wrong file in non-strict mode
    ///   could consume file size worth of memory in the process.
    pub fn with_strictness(mut reader: R, strict: bool) -> ImageResult<HDRDecoder<R>> {
        let mut attributes = HDRMetadata::new();

        {
            // scope to make borrowck happy
            let r = &mut reader;
            if strict {
                let mut signature = [0; SIGNATURE_LENGTH];
                try!(r.read_exact(&mut signature));
                if signature != SIGNATURE {
                    return Err(ImageError::FormatError(
                        "Radiance HDR signature not found".to_string(),
                    ));
                } // no else
                  // skip signature line ending
                try!(read_line_u8(r));
            } else {
                // Old Radiance HDR files (*.pic) don't use signature
                // Let them be parsed in non-strict mode
            }
            // read header data until empty line
            loop {
                match try!(read_line_u8(r)) {
                    None => {
                        // EOF before end of header
                        return Err(ImageError::FormatError("EOF in header".into()));
                    }
                    Some(line) => {
                        if line.is_empty() {
                            // end of header
                            break;
                        } else if line[0] == b'#' {
                            // line[0] will not panic, line.len() == 0 is false here
                            // skip comments
                            continue;
                        } // no else
                          // process attribute line
                        let line = String::from_utf8_lossy(&line[..]);
                        try!(attributes.update_header_info(&line, strict));
                    } // <= Some(line)
                } // match read_line_u8()
            } // loop
        } // scope to end borrow of reader
          // parse dimensions
        let (width, height) = match try!(read_line_u8(&mut reader)) {
            None => {
                // EOF instead of image dimensions
                return Err(ImageError::FormatError("EOF in dimensions line".into()));
            }
            Some(dimensions) => {
                let dimensions = String::from_utf8_lossy(&dimensions[..]);
                try!(parse_dimensions_line(&dimensions, strict))
            }
        };

        Ok(HDRDecoder {
            r: reader,

            width,
            height,
            meta: HDRMetadata {
                width,
                height,
                ..attributes
            },
        })
    } // end with_strictness

    /// Returns file metadata. Refer to ```HDRMetadata``` for details.
    pub fn metadata(&self) -> HDRMetadata {
        self.meta.clone()
    }

    /// Consumes decoder and returns a vector of RGBE8 pixels
    pub fn read_image_native(mut self) -> ImageResult<Vec<RGBE8Pixel>> {
        // Don't read anything if image is empty
        if self.width == 0 || self.height == 0 {
            return Ok(vec![]);
        }
        // expression self.width > 0 && self.height > 0 is true from now to the end of this method
        let pixel_count = self.width as usize * self.height as usize;
        let mut ret = Vec::<RGBE8Pixel>::with_capacity(pixel_count);
        unsafe {
            // RGBE8Pixel doesn't implement Drop, so it's Ok to drop half-initialized ret
            ret.set_len(pixel_count);
        } // ret contains uninitialized data, so now it's my responsibility to return fully initialized ret
        for chunk in ret.chunks_mut(self.width as usize) {
            try!(read_scanline(&mut self.r, chunk));
        }
        Ok(ret)
    }

    /// Consumes decoder and returns a vector of transformed pixels
    pub fn read_image_transform<T: Send, F: Send + Sync + Fn(RGBE8Pixel) -> T>(
        mut self,
        f: F,
    ) -> ImageResult<Vec<T>> {
        // Don't read anything if image is empty
        if self.width == 0 || self.height == 0 {
            return Ok(vec![]);
        }
        // expression self.width > 0 && self.height > 0 is true from now to the end of this method
        // scanline buffer
        let uszwidth = self.width as usize;

        let pixel_count = self.width as usize * self.height as usize;
        let mut ret = Vec::with_capacity(pixel_count);
        unsafe {
            // RGBE8Pixel doesn't implement Drop, so it's Ok to drop half-initialized ret
            ret.set_len(pixel_count);
        } // ret contains uninitialized data, so now it's my responsibility to return fully initialized ret

        {
            let chunks_iter = ret.chunks_mut(uszwidth);
            let mut pool = Pool::new(8); //

            try!(pool.scoped(|scope| {
                for chunk in chunks_iter {
                    let mut buf = Vec::<RGBE8Pixel>::with_capacity(uszwidth);
                    unsafe {
                        buf.set_len(uszwidth);
                    }
                    try!(read_scanline(&mut self.r, &mut buf[..]));
                    let f = &f;
                    scope.execute(move || {
                        for (dst, &pix) in chunk.iter_mut().zip(buf.iter()) {
                            *dst = f(pix);
                        }
                    });
                }
                Ok(())
            }) as Result<(), ImageError>);
        }

        Ok(ret)
    }

    /// Consumes decoder and returns a vector of Rgb<u8> pixels.
    /// scale = 1, gamma = 2.2
    pub fn read_image_ldr(self) -> ImageResult<Vec<Rgb<u8>>> {
        self.read_image_transform(|pix| pix.to_ldr())
    }

    /// Consumes decoder and returns a vector of Rgb<f32> pixels.
    ///
    pub fn read_image_hdr(self) -> ImageResult<Vec<Rgb<f32>>> {
        self.read_image_transform(|pix| pix.to_hdr())
    }
}

impl<R: BufRead> IntoIterator for HDRDecoder<R> {
    type Item = ImageResult<RGBE8Pixel>;
    type IntoIter = HDRImageDecoderIterator<R>;

    fn into_iter(self) -> Self::IntoIter {
        // scanline buffer
        let mut buf = Vec::with_capacity(self.width as usize);
        unsafe {
            // dropping half-initialized vector of RGBE8Pixel is safe
            // and I took care to hide half-initialized vector from a user
            buf.set_len(self.width as usize);
        }
        HDRImageDecoderIterator {
            r: self.r,
            scanline_cnt: self.height as usize,
            buf,
            col: 0,
            scanline: 0,
            trouble: true, // make first call to `next()` read scanline
            error_encountered: false,
        }
    }
}

/// Scanline buffered pixel by pixel iterator
pub struct HDRImageDecoderIterator<R: BufRead> {
    r: R,
    scanline_cnt: usize,
    buf: Vec<RGBE8Pixel>, // scanline buffer
    col: usize,           // current position in scanline
    scanline: usize,      // current scanline
    trouble: bool,        // optimization, true indicates that we need to check something
    error_encountered: bool,
}

impl<R: BufRead> HDRImageDecoderIterator<R> {
    // Advances counter to the next pixel
    #[inline]
    fn advance(&mut self) {
        self.col += 1;
        if self.col == self.buf.len() {
            self.col = 0;
            self.scanline += 1;
            self.trouble = true;
        }
    }
}

impl<R: BufRead> Iterator for HDRImageDecoderIterator<R> {
    type Item = ImageResult<RGBE8Pixel>;

    fn next(&mut self) -> Option<Self::Item> {
        if !self.trouble {
            let ret = self.buf[self.col];
            self.advance();
            Some(Ok(ret))
        } else {
            // some condition is pending
            if self.buf.is_empty() || self.scanline == self.scanline_cnt {
                // No more pixels
                return None;
            } // no else
            if self.error_encountered {
                self.advance();
                // Error was encountered. Keep producing errors.
                // ImageError can't implement Clone, so just dump some error
                return Some(Err(ImageError::ImageEnd));
            } // no else
            if self.col == 0 {
                // fill scanline buffer
                match read_scanline(&mut self.r, &mut self.buf[..]) {
                    Ok(_) => {
                        // no action required
                    }
                    Err(err) => {
                        self.advance();
                        self.error_encountered = true;
                        self.trouble = true;
                        return Some(Err(err));
                    }
                }
            } // no else
            self.trouble = false;
            let ret = self.buf[0];
            self.advance();
            Some(Ok(ret))
        }
    }

    fn size_hint(&self) -> (usize, Option<usize>) {
        let total_cnt = self.buf.len() * self.scanline_cnt;
        let cur_cnt = self.buf.len() * self.scanline + self.col;
        let remaining = total_cnt - cur_cnt;
        (remaining, Some(remaining))
    }
}

impl<R: BufRead> ExactSizeIterator for HDRImageDecoderIterator<R> {}

// Precondition: buf.len() > 0
fn read_scanline<R: BufRead>(r: &mut R, buf: &mut [RGBE8Pixel]) -> ImageResult<()> {
    assert!(!buf.is_empty());
    let width = buf.len();
    // first 4 bytes in scanline allow to determine compression method
    let fb = try!(read_rgbe(r));
    if fb.c[0] == 2 && fb.c[1] == 2 && fb.c[2] < 128 {
        // denormalized pixel value (2,2,<128,_) indicates new per component RLE method
        // decode_component guarantees that offset is within 0 .. width
        // therefore we can skip bounds checking here, but we will not
        try!(decode_component(r, width, |offset, value| buf[offset].c[0] = value));
        try!(decode_component(r, width, |offset, value| buf[offset].c[1] = value));
        try!(decode_component(r, width, |offset, value| buf[offset].c[2] = value));
        try!(decode_component(r, width, |offset, value| buf[offset].e = value));
    } else {
        // old RLE method (it was considered old around 1991, should it be here?)
        try!(decode_old_rle(r, fb, buf));
    }
    Ok(())
}

#[inline(always)]
fn read_byte<R: BufRead>(r: &mut R) -> io::Result<u8> {
    let mut buf = [0u8];
    try!(r.read_exact(&mut buf[..]));
    Ok(buf[0])
}

// Guarantees that first parameter of set_component will be within pos .. pos+width
#[inline]
fn decode_component<R: BufRead, S: FnMut(usize, u8)>(
    r: &mut R,
    width: usize,
    mut set_component: S,
) -> ImageResult<()> {
    let mut buf = [0; 128];
    let mut pos = 0;
    while pos < width {
        // increment position by a number of decompressed values
        pos += {
            let rl = try!(read_byte(r));
            if rl <= 128 {
                // sanity check
                if pos + rl as usize > width {
                    return Err(ImageError::FormatError(
                        "Wrong length of decoded scanline".into(),
                    ));
                }
                // read values
                try!(r.read_exact(&mut buf[0..rl as usize]));
                for (offset, &value) in buf[0..rl as usize].iter().enumerate() {
                    set_component(pos + offset, value);
                }
                rl as usize
            } else {
                // run
                let rl = rl - 128;
                // sanity check
                if pos + rl as usize > width {
                    return Err(ImageError::FormatError(
                        "Wrong length of decoded scanline".into(),
                    ));
                }
                // fill with same value
                let value = try!(read_byte(r));
                for offset in 0..rl as usize {
                    set_component(pos + offset, value);
                }
                rl as usize
            }
        };
    }
    if pos != width {
        return Err(ImageError::FormatError(
            "Wrong length of decoded scanline".into(),
        ));
    }
    Ok(())
}

// Decodes scanline, places it into buf
// Precondition: buf.len() > 0
// fb - first 4 bytes of scanline
fn decode_old_rle<R: BufRead>(
    r: &mut R,
    fb: RGBE8Pixel,
    buf: &mut [RGBE8Pixel],
) -> ImageResult<()> {
    assert!(!buf.is_empty());
    let width = buf.len();
    // convenience function.
    // returns run length if pixel is a run length marker
    #[inline]
    fn rl_marker(pix: RGBE8Pixel) -> Option<usize> {
        if pix.c == [1, 1, 1] {
            Some(pix.e as usize)
        } else {
            None
        }
    }
    // first pixel in scanline should not be run length marker
    // it is error if it is
    if rl_marker(fb).is_some() {
        return Err(ImageError::FormatError(
            "First pixel of a scanline shouldn't be run length marker".into(),
        ));
    }
    buf[0] = fb; // set first pixel of scanline

    let mut x_off = 1; // current offset from beginning of a scanline
    let mut rl_mult = 1; // current run length multiplier
    let mut prev_pixel = fb;
    while x_off < width {
        let pix = try!(read_rgbe(r));
        // it's harder to forget to increase x_off if I write this this way.
        x_off += {
            if let Some(rl) = rl_marker(pix) {
                // rl_mult takes care of consecutive RL markers
                let rl = rl * rl_mult;
                rl_mult *= 256;
                if x_off + rl <= width {
                    // do run
                    for b in &mut buf[x_off..x_off + rl] {
                        *b = prev_pixel;
                    }
                } else {
                    return Err(ImageError::FormatError(
                        "Wrong length of decoded scanline".into(),
                    ));
                };
                rl // value to increase x_off by
            } else {
                rl_mult = 1; // chain of consecutive RL markers is broken
                prev_pixel = pix;
                buf[x_off] = pix;
                1 // value to increase x_off by
            }
        };
    }
    if x_off != width {
        return Err(ImageError::FormatError(
            "Wrong length of decoded scanline".into(),
        ));
    }
    Ok(())
}

fn read_rgbe<R: BufRead>(r: &mut R) -> io::Result<RGBE8Pixel> {
    let mut buf = [0u8; 4];
    try!(r.read_exact(&mut buf[..]));
    Ok(RGBE8Pixel {c: [buf[0], buf[1], buf[2]], e: buf[3] })
}

/// Metadata for Radiance HDR image
#[derive(Debug, Clone)]
pub struct HDRMetadata {
    /// Width of decoded image. It could be either scanline length,
    /// or scanline count, depending on image orientation.
    pub width: u32,
    /// Height of decoded image. It depends on orientation too.
    pub height: u32,
    /// Orientation matrix. For standard orientation it is ((1,0),(0,1)) - left to right, top to bottom.
    /// First pair tells how resulting pixel coordinates change along a scanline.
    /// Second pair tells how they change from one scanline to the next.
    pub orientation: ((i8, i8), (i8, i8)),
    /// Divide color values by exposure to get to get physical radiance in
    /// watts/steradian/m<sup>2</sup>
    ///
    /// Image may not contain physical data, even if this field is set.
    pub exposure: Option<f32>,
    /// Divide color values by corresponding tuple member (r, g, b) to get to get physical radiance
    /// in watts/steradian/m<sup>2</sup>
    ///
    /// Image may not contain physical data, even if this field is set.
    pub color_correction: Option<(f32, f32, f32)>,
    /// Pixel height divided by pixel width
    pub pixel_aspect_ratio: Option<f32>,
    /// All lines contained in image header are put here. Ordering of lines is preserved.
    /// Lines in the form "key=value" are represented as ("key", "value").
    /// All other lines are ("", "line")
    pub custom_attributes: Vec<(String, String)>,
}

impl HDRMetadata {
    fn new() -> HDRMetadata {
        HDRMetadata {
            width: 0,
            height: 0,
            orientation: ((1, 0), (0, 1)),
            exposure: None,
            color_correction: None,
            pixel_aspect_ratio: None,
            custom_attributes: vec![],
        }
    }

    // Updates header info, in strict mode returns error for malformed lines (no '=' separator)
    // unknown attributes are skipped
    fn update_header_info(&mut self, line: &str, strict: bool) -> ImageResult<()> {
        // split line at first '='
        // old Radiance HDR files (*.pic) feature tabs in key, so                vvv trim
        let maybe_key_value = split_at_first(line, "=").map(|(key, value)| (key.trim(), value));
        // save all header lines in custom_attributes
        match maybe_key_value {
            Some((key, val)) => self.custom_attributes
                .push((key.to_owned(), val.to_owned())),
            None => self.custom_attributes.push(("".into(), line.to_owned())),
        }
        // parse known attributes
        match maybe_key_value {
            Some(("FORMAT", val)) => {
                if val.trim() != "32-bit_rle_rgbe" {
                    // XYZE isn't supported yet
                    return Err(ImageError::UnsupportedError(limit_string_len(val, 20)));
                }
            }
            Some(("EXPOSURE", val)) => {
                match val.trim().parse::<f32>() {
                    Ok(v) => {
                        self.exposure = Some(self.exposure.unwrap_or(1.0) * v); // all encountered exposure values should be multiplied
                    }
                    Err(parse_error) => {
                        if strict {
                            return Err(ImageError::FormatError(format!(
                                "Cannot parse EXPOSURE value: {}",
                                parse_error.description()
                            )));
                        } // no else, skip this line in non-strict mode
                    }
                };
            }
            Some(("PIXASPECT", val)) => {
                match val.trim().parse::<f32>() {
                    Ok(v) => {
                        self.pixel_aspect_ratio = Some(self.pixel_aspect_ratio.unwrap_or(1.0) * v); // all encountered exposure values should be multiplied
                    }
                    Err(parse_error) => {
                        if strict {
                            return Err(ImageError::FormatError(format!(
                                "Cannot parse PIXASPECT value: {}",
                                parse_error.description()
                            )));
                        } // no else, skip this line in non-strict mode
                    }
                };
            }
            Some(("COLORCORR", val)) => {
                let mut rgbcorr = [1.0, 1.0, 1.0];
                match parse_space_separated_f32(val, &mut rgbcorr, "COLORCORR") {
                    Ok(extra_numbers) => {
                        if strict && extra_numbers {
                            return Err(ImageError::FormatError(
                                "Extra numbers in COLORCORR".into(),
                            ));
                        } // no else, just ignore extra numbers
                        let (rc, gc, bc) = self.color_correction.unwrap_or((1.0, 1.0, 1.0));
                        self.color_correction =
                            Some((rc * rgbcorr[0], gc * rgbcorr[1], bc * rgbcorr[2]));
                    }
                    Err(err) => {
                        if strict {
                            return Err(err);
                        } // no else, skip malformed line in non-strict mode
                    }
                }
            }
            None => {
                // old Radiance HDR files (*.pic) contain commands in a header
                // just skip them
            }
            _ => {
                // skip unknown attribute
            }
        } // match attributes
        Ok(())
    }
}

fn parse_space_separated_f32(line: &str, vals: &mut [f32], name: &str) -> ImageResult<bool> {
    let mut nums = line.split_whitespace();
    for val in vals.iter_mut() {
        if let Some(num) = nums.next() {
            match num.parse::<f32>() {
                Ok(v) => *val = v,
                Err(err) => {
                    return Err(ImageError::FormatError(format!(
                        "f32 parse error in {}: {}",
                        name,
                        err.description()
                    )));
                }
            }
        } else {
            // not enough numbers in line
            return Err(ImageError::FormatError(format!(
                "Not enough numbers in {}",
                name
            )));
        }
    }
    Ok(nums.next().is_some())
}

// Parses dimension line "-Y height +X width"
// returns (width, height) or error
fn parse_dimensions_line(line: &str, strict: bool) -> ImageResult<(u32, u32)> {
    let mut dim_parts = line.split_whitespace();
    let err = "Malformed dimensions line";
    let c1_tag = try!(
        dim_parts
            .next()
            .ok_or_else(|| ImageError::FormatError(err.into()))
    );
    let c1_str = try!(
        dim_parts
            .next()
            .ok_or_else(|| ImageError::FormatError(err.into()))
    );
    let c2_tag = try!(
        dim_parts
            .next()
            .ok_or_else(|| ImageError::FormatError(err.into()))
    );
    let c2_str = try!(
        dim_parts
            .next()
            .ok_or_else(|| ImageError::FormatError(err.into()))
    );
    if strict && dim_parts.next().is_some() {
        // extra data in dimensions line
        return Err(ImageError::FormatError(err.into()));
    } // no else
      // dimensions line is in the form "-Y 10 +X 20"
      // There are 8 possible orientations: +Y +X, +X -Y and so on
    match (c1_tag, c2_tag) {
        ("-Y", "+X") => {
            // Common orientation (left-right, top-down)
            // c1_str is height, c2_str is width
            let height = try!(c1_str.parse::<u32>().into_image_error(err));
            let width = try!(c2_str.parse::<u32>().into_image_error(err));
            Ok((width, height))
        }
        _ => Err(ImageError::FormatError(format!(
            "Unsupported orientation {} {}",
            limit_string_len(c1_tag, 4),
            limit_string_len(c2_tag, 4)
        ))),
    } // final expression. Returns value
}

trait IntoImageError<T> {
    fn into_image_error(self, description: &str) -> ImageResult<T>;
}

impl<T> IntoImageError<T> for ::std::result::Result<T, ::std::num::ParseFloatError> {
    fn into_image_error(self, description: &str) -> ImageResult<T> {
        self.map_err(|err| {
            ImageError::FormatError(format!("{} {}", description, err.description()))
        })
    }
}

impl<T> IntoImageError<T> for ::std::result::Result<T, ::std::num::ParseIntError> {
    fn into_image_error(self, description: &str) -> ImageResult<T> {
        self.map_err(|err| {
            ImageError::FormatError(format!("{} {}", description, err.description()))
        })
    }
}

// Returns string with no more than len+3 characters
fn limit_string_len(s: &str, len: usize) -> String {
    let s_char_len = s.chars().count();
    if s_char_len > len {
        s.chars().take(len).chain("...".chars()).collect()
    } else {
        s.into()
    }
}

// Splits string into (before separator, after separator) tuple
// or None if separator isn't found
fn split_at_first<'a>(s: &'a str, separator: &str) -> Option<(&'a str, &'a str)> {
    match s.find(separator) {
        None | Some(0) => None,
        Some(p) if p >= s.len() - separator.len() => None,
        Some(p) => Some((&s[..p], &s[(p + separator.len())..])),
    }
}

#[test]
fn split_at_first_test() {
    assert_eq!(split_at_first(&Cow::Owned("".into()), "="), None);
    assert_eq!(split_at_first(&Cow::Owned("=".into()), "="), None);
    assert_eq!(split_at_first(&Cow::Owned("= ".into()), "="), None);
    assert_eq!(
        split_at_first(&Cow::Owned(" = ".into()), "="),
        Some((" ", " "))
    );
    assert_eq!(
        split_at_first(&Cow::Owned("EXPOSURE= ".into()), "="),
        Some(("EXPOSURE", " "))
    );
    assert_eq!(
        split_at_first(&Cow::Owned("EXPOSURE= =".into()), "="),
        Some(("EXPOSURE", " ="))
    );
    assert_eq!(
        split_at_first(&Cow::Owned("EXPOSURE== =".into()), "=="),
        Some(("EXPOSURE", " ="))
    );
    assert_eq!(split_at_first(&Cow::Owned("EXPOSURE".into()), ""), None);
}

// Reads input until b"\n" or EOF
// Returns vector of read bytes NOT including end of line characters
//   or return None to indicate end of file
fn read_line_u8<R: BufRead>(r: &mut R) -> ::std::io::Result<Option<Vec<u8>>> {
    let mut ret = Vec::with_capacity(16);
    match r.read_until(b'\n', &mut ret) {
        Ok(0) => Ok(None),
        Ok(_) => {
            if let Some(&b'\n') = ret[..].last() {
                let _ = ret.pop();
            }
            Ok(Some(ret))
        }
        Err(err) => Err(err),
    }
}

#[test]
fn read_line_u8_test() {
    let buf: Vec<_> = (&b"One\nTwo\nThree\nFour\n\n\n"[..]).into();
    let input = &mut ::std::io::Cursor::new(buf);
    assert_eq!(&read_line_u8(input).unwrap().unwrap()[..], &b"One"[..]);
    assert_eq!(&read_line_u8(input).unwrap().unwrap()[..], &b"Two"[..]);
    assert_eq!(&read_line_u8(input).unwrap().unwrap()[..], &b"Three"[..]);
    assert_eq!(&read_line_u8(input).unwrap().unwrap()[..], &b"Four"[..]);
    assert_eq!(&read_line_u8(input).unwrap().unwrap()[..], &b""[..]);
    assert_eq!(&read_line_u8(input).unwrap().unwrap()[..], &b""[..]);
    assert_eq!(read_line_u8(input).unwrap(), None);
}

/// Helper function for reading raw 3-channel f32 images
pub fn read_raw_file<P: AsRef<Path>>(path: P) -> ::std::io::Result<Vec<Rgb<f32>>> {
    use byteorder::{LittleEndian as LE, ReadBytesExt};
    use std::fs::File;
    use std::io::BufReader;

    let mut r = BufReader::new(try!(File::open(path)));
    let w = try!(r.read_u32::<LE>()) as usize;
    let h = try!(r.read_u32::<LE>()) as usize;
    let c = try!(r.read_u32::<LE>()) as usize;
    assert_eq!(c, 3);
    let cnt = w * h;
    let mut ret = Vec::with_capacity(cnt);
    for _ in 0..cnt {
        let cr = try!(r.read_f32::<LE>());
        let cg = try!(r.read_f32::<LE>());
        let cb = try!(r.read_f32::<LE>());
        ret.push(Rgb([cr, cg, cb]));
    }
    Ok(ret)
}
