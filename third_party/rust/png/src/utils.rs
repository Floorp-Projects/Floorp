//! Utility functions
use std::iter::repeat;
use num_iter::range_step;

#[inline(always)]
pub fn unpack_bits<F>(buf: &mut [u8], channels: usize, bit_depth: u8, func: F)
where F: Fn(u8, &mut[u8]) {
    let bits = buf.len()/channels*bit_depth as usize;
    let extra_bits = bits % 8;
    let entries = bits / 8 + match extra_bits {
        0 => 0,
        _ => 1
    };
    let skip = match extra_bits {
        0 => 0,
        n => (8-n) / bit_depth as usize
    };
    let mask = ((1u16 << bit_depth) - 1) as u8;
    let i =
        (0..entries)
        .rev() // reverse iterator
        .flat_map(|idx|
            // this has to be reversed too
            range_step(0, 8, bit_depth)
            .zip(repeat(idx))
        )
        .skip(skip);
    let channels = channels as isize;
    let j = range_step(buf.len() as isize - channels, -channels, -channels);
    //let j = range_step(0, buf.len(), channels).rev(); // ideal solution;
    for ((shift, i), j) in i.zip(j) {
        let pixel = (buf[i] & (mask << shift)) >> shift;
        func(pixel, &mut buf[j as usize..(j + channels) as usize])
    }
}

pub fn expand_trns_line(buf: &mut[u8], trns: &[u8], channels: usize) {
    let channels = channels as isize;
    let i = range_step(buf.len() as isize / (channels+1) * channels - channels, -channels, -channels);
    let j = range_step(buf.len() as isize - (channels+1), -(channels+1), -(channels+1));
    let channels = channels as usize;
    for (i, j) in i.zip(j) {
        let i_pixel = i as usize;
        let j_chunk = j as usize;
        if &buf[i_pixel..i_pixel+channels] == trns {
            buf[j_chunk+channels] = 0
        } else {
            buf[j_chunk+channels] = 0xFF
        }
        for k in (0..channels).rev() {
            buf[j_chunk+k] = buf[i_pixel+k];
        }
    }
}

pub fn expand_trns_line16(buf: &mut[u8], trns: &[u8], channels: usize) {
    let channels = channels as isize;
    let c2 = 2 * channels;
    let i = range_step(buf.len() as isize / (c2+2) * c2 - c2, -c2, -c2);
    let j = range_step(buf.len() as isize - (c2+2), -(c2+2), -(c2+2));
    let c2 = c2 as usize;
    for (i, j) in i.zip(j) {
        let i_pixel = i as usize;
        let j_chunk = j as usize;
        if &buf[i_pixel..i_pixel+c2] == trns {
            buf[j_chunk+c2] = 0;
            buf[j_chunk+c2 + 1] = 0
        } else {
            buf[j_chunk+c2] = 0xFF;
            buf[j_chunk+c2 + 1] = 0xFF
        }
        for k in (0..c2).rev() {
            buf[j_chunk+k] = buf[i_pixel+k];
        }
    }
}


/// This iterator iterates over the different passes of an image Adam7 encoded
/// PNG image
/// The pattern is:
///     16462646
///     77777777
///     56565656
///     77777777
///     36463646
///     77777777
///     56565656
///     77777777
///
#[derive(Clone)]
pub struct Adam7Iterator {
    line: u32,
    lines: u32,
    line_width: u32,
    current_pass: u8,
    width: u32,
    height: u32,
}

impl Adam7Iterator {
    pub fn new(width: u32, height: u32) -> Adam7Iterator {
        let mut this = Adam7Iterator {
            line: 0,
            lines: 0,
            line_width: 0,
            current_pass: 1,
            width: width,
            height: height
        };
        this.init_pass();
        this
    }

    /// Calculates the bounds of the current pass
    fn init_pass(&mut self) {
        let w = self.width as f64;
        let h = self.height as f64;
        let (line_width, lines) = match self.current_pass {
            1 => (w/8.0, h/8.0),
            2 => ((w-4.0)/8.0, h/8.0),
            3 => (w/4.0, (h-4.0)/8.0),
            4 => ((w-2.0)/4.0, h/4.0),
            5 => (w/2.0, (h-2.0)/4.0),
            6 => ((w-1.0)/2.0, h/2.0),
            7 => (w, (h-1.0)/2.0),
            _ => unreachable!()
        };
        self.line_width = line_width.ceil() as u32;
        self.lines = lines.ceil() as u32;
        self.line = 0;
    }
    
    /// The current pass#.
    pub fn current_pass(&self) -> u8 {
        self.current_pass
    }
}

/// Iterates over the (passes, lines, widths)
impl Iterator for Adam7Iterator {
    type Item = (u8, u32, u32);
    fn next(&mut self) -> Option<(u8, u32, u32)> {
        if self.line < self.lines && self.line_width > 0 {
            let this_line = self.line;
            self.line += 1;
            Some((self.current_pass, this_line, self.line_width))
        } else if self.current_pass < 7 {
            self.current_pass += 1;
            self.init_pass();
            self.next()
        } else {
            None
        }
    }
}

macro_rules! expand_pass(
    ($img:expr, $scanline:expr, $j:ident, $pos:expr, $bytes_pp:expr) => {
        for ($j, pixel) in $scanline.chunks($bytes_pp).enumerate() {
            for (offset, val) in pixel.iter().enumerate() {
                $img[$pos + offset] = *val
            }
        }
    }
);

/// Expands an Adam 7 pass
pub fn expand_pass(
    img: &mut [u8], width: u32, scanline: &[u8],
    pass: u8, line_no: u32, bytes_pp: u8) {
    let line_no = line_no as usize;
    let width = width as usize;
    let bytes_pp = bytes_pp as usize;
    match pass {
        1 => expand_pass!(img, scanline, j,  8*line_no    * width + bytes_pp * j*8     , bytes_pp),
        2 => expand_pass!(img, scanline, j,  8*line_no    * width + bytes_pp *(j*8 + 4), bytes_pp),
        3 => expand_pass!(img, scanline, j, (8*line_no+4) * width + bytes_pp * j*4     , bytes_pp),
        4 => expand_pass!(img, scanline, j,  4*line_no    * width + bytes_pp *(j*4 + 2), bytes_pp),
        5 => expand_pass!(img, scanline, j, (4*line_no+2) * width + bytes_pp * j*2     , bytes_pp),
        6 => expand_pass!(img, scanline, j,  2*line_no    * width + bytes_pp *(j*2+1)  , bytes_pp),
        7 => expand_pass!(img, scanline, j, (2*line_no+1) * width + bytes_pp * j       , bytes_pp),
        _ => {}
    }
}

#[test]
fn test_adam7() {
    /*
        1646
        7777
        5656
        7777
    */
    let it = Adam7Iterator::new(4, 4);
    let passes: Vec<_> = it.collect();
    assert_eq!(&*passes, &[(1, 0, 1), (4, 0, 1), (5, 0, 2), (6, 0, 2), (6, 1, 2), (7, 0, 4), (7, 1, 4)]);
}
