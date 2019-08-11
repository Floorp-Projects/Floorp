use color::Rgb;
use hdr::{rgbe8, RGBE8Pixel, SIGNATURE};
use std::io::{Result, Write};

/// Radiance HDR encoder
pub struct HDREncoder<W: Write> {
    w: W,
}

impl<W: Write> HDREncoder<W> {
    /// Creates encoder
    pub fn new(w: W) -> HDREncoder<W> {
        HDREncoder { w }
    }

    /// Encodes the image ```data```
    /// that has dimensions ```width``` and ```height```
    pub fn encode(mut self, data: &[Rgb<f32>], width: usize, height: usize) -> Result<()> {
        assert!(data.len() >= width * height);
        let w = &mut self.w;
        w.write_all(SIGNATURE)?;
        w.write_all(b"\n")?;
        w.write_all(b"# Rust HDR encoder\n")?;
        w.write_all(b"FORMAT=32-bit_rle_rgbe\n\n")?;
        w.write_all(format!("-Y {} +X {}\n", height, width).as_bytes())?;

        if width < 8 || width > 32_768 {
            for &pix in data {
                write_rgbe8(w, to_rgbe8(pix))?;
            }
        } else {
            // new RLE marker contains scanline width
            let marker = rgbe8(2, 2, (width / 256) as u8, (width % 256) as u8);
            // buffers for encoded pixels
            let mut bufr = Vec::with_capacity(width);
            bufr.resize(width, 0);
            let mut bufg = Vec::with_capacity(width);
            bufg.resize(width, 0);
            let mut bufb = Vec::with_capacity(width);
            bufb.resize(width, 0);
            let mut bufe = Vec::with_capacity(width);
            bufe.resize(width, 0);
            let mut rle_buf = Vec::with_capacity(width);
            for scanline in data.chunks(width) {
                for ((((r, g), b), e), &pix) in bufr.iter_mut()
                    .zip(bufg.iter_mut())
                    .zip(bufb.iter_mut())
                    .zip(bufe.iter_mut())
                    .zip(scanline.iter())
                {
                    let cp = to_rgbe8(pix);
                    *r = cp.c[0];
                    *g = cp.c[1];
                    *b = cp.c[2];
                    *e = cp.e;
                }
                write_rgbe8(w, marker)?; // New RLE encoding marker
                rle_buf.clear();
                rle_compress(&bufr[..], &mut rle_buf);
                w.write_all(&rle_buf[..])?;
                rle_buf.clear();
                rle_compress(&bufg[..], &mut rle_buf);
                w.write_all(&rle_buf[..])?;
                rle_buf.clear();
                rle_compress(&bufb[..], &mut rle_buf);
                w.write_all(&rle_buf[..])?;
                rle_buf.clear();
                rle_compress(&bufe[..], &mut rle_buf);
                w.write_all(&rle_buf[..])?;
            }
        }
        Ok(())
    }
}

#[derive(Debug, PartialEq, Eq)]
enum RunOrNot {
    Run(u8, usize),
    Norun(usize, usize),
}
use self::RunOrNot::{Norun, Run};

const RUN_MAX_LEN: usize = 127;
const NORUN_MAX_LEN: usize = 128;

struct RunIterator<'a> {
    data: &'a [u8],
    curidx: usize,
}

impl<'a> RunIterator<'a> {
    fn new(data: &'a [u8]) -> RunIterator<'a> {
        RunIterator { data, curidx: 0 }
    }
}

impl<'a> Iterator for RunIterator<'a> {
    type Item = RunOrNot;

    fn next(&mut self) -> Option<Self::Item> {
        if self.curidx == self.data.len() {
            None
        } else {
            let cv = self.data[self.curidx];
            let crun = self.data[self.curidx..]
                .iter()
                .take_while(|&&v| v == cv)
                .take(RUN_MAX_LEN)
                .count();
            let ret = if crun > 2 {
                Run(cv, crun)
            } else {
                Norun(self.curidx, crun)
            };
            self.curidx += crun;
            Some(ret)
        }
    }
}

struct NorunCombineIterator<'a> {
    runiter: RunIterator<'a>,
    prev: Option<RunOrNot>,
}

impl<'a> NorunCombineIterator<'a> {
    fn new(data: &'a [u8]) -> NorunCombineIterator<'a> {
        NorunCombineIterator {
            runiter: RunIterator::new(data),
            prev: None,
        }
    }
}

// Combines sequential noruns produced by RunIterator
impl<'a> Iterator for NorunCombineIterator<'a> {
    type Item = RunOrNot;
    fn next(&mut self) -> Option<Self::Item> {
        loop {
            match self.prev.take() {
                Some(Run(c, len)) => {
                    // Just return stored run
                    return Some(Run(c, len));
                }
                Some(Norun(idx, len)) => {
                    // Let's see if we need to continue norun
                    match self.runiter.next() {
                        Some(Norun(_, len1)) => {
                            // norun continues
                            let clen = len + len1; // combined length
                            if clen == NORUN_MAX_LEN {
                                return Some(Norun(idx, clen));
                            } else if clen > NORUN_MAX_LEN {
                                // combined norun exceeds maximum length. store extra part of norun
                                self.prev = Some(Norun(idx + NORUN_MAX_LEN, clen - NORUN_MAX_LEN));
                                // then return maximal norun
                                return Some(Norun(idx, NORUN_MAX_LEN));
                            } else {
                                // len + len1 < NORUN_MAX_LEN
                                self.prev = Some(Norun(idx, len + len1));
                                // combine and continue loop
                            }
                        }
                        Some(Run(c, len1)) => {
                            // Run encountered. Store it
                            self.prev = Some(Run(c, len1));
                            return Some(Norun(idx, len)); // and return combined norun
                        }
                        None => {
                            // End of sequence
                            return Some(Norun(idx, len)); // return combined norun
                        }
                    }
                } // End match self.prev.take() == Some(NoRun())
                None => {
                    // No norun to combine
                    match self.runiter.next() {
                        Some(Norun(idx, len)) => {
                            self.prev = Some(Norun(idx, len));
                            // store for combine and continue the loop
                        }
                        Some(Run(c, len)) => {
                            // Some run. Just return it
                            return Some(Run(c, len));
                        }
                        None => {
                            // That's all, folks
                            return None;
                        }
                    }
                } // End match self.prev.take() == None
            } // End match
        } // End loop
    }
}

// Appends RLE compressed ```data``` to ```rle```
fn rle_compress(data: &[u8], rle: &mut Vec<u8>) {
    rle.clear();
    if data.is_empty() {
        rle.push(0); // Technically correct. It means read next 0 bytes.
        return;
    }
    // Task: split data into chunks of repeating (max 127) and non-repeating bytes (max 128)
    // Prepend non-repeating chunk with its length
    // Replace repeating byte with (run length + 128) and the byte
    for rnr in NorunCombineIterator::new(data) {
        match rnr {
            Run(c, len) => {
                assert!(len <= 127);
                rle.push(128u8 + len as u8);
                rle.push(c);
            }
            Norun(idx, len) => {
                assert!(len <= 128);
                rle.push(len as u8);
                rle.extend_from_slice(&data[idx..idx + len]);
            }
        }
    }
}

fn write_rgbe8<W: Write>(w: &mut W, v: RGBE8Pixel) -> Result<()> {
    w.write_all(&[v.c[0], v.c[1], v.c[2], v.e])
}

/// Converts ```Rgb<f32>``` into ```RGBE8Pixel```
pub fn to_rgbe8(pix: Rgb<f32>) -> RGBE8Pixel {
    let pix = pix.0;
    let mx = f32::max(pix[0], f32::max(pix[1], pix[2]));
    if mx <= 0.0 {
        RGBE8Pixel { c: [0, 0, 0], e: 0 }
    } else {
        // let (frac, exp) = mx.frexp(); // unstable yet
        let exp = mx.log2().floor() as i32 + 1;
        let mul = f32::powi(2.0, exp);
        let mut conv = [0u8; 3];
        for (cv, &sv) in conv.iter_mut().zip(pix.iter()) {
            *cv = f32::trunc(sv / mul * 256.0) as u8;
        }
        RGBE8Pixel {
            c: conv,
            e: (exp + 128) as u8,
        }
    }
}

#[test]
fn to_rgbe8_test() {
    use hdr::rgbe8;
    let test_cases = vec![rgbe8(0, 0, 0, 0), rgbe8(1, 1, 128, 128)];
    for &pix in &test_cases {
        assert_eq!(pix, to_rgbe8(pix.to_hdr()));
    }
    for mc in 128..255 {
        // TODO: use inclusive range when stable
        let pix = rgbe8(mc, mc, mc, 100);
        assert_eq!(pix, to_rgbe8(pix.to_hdr()));
        let pix = rgbe8(mc, 0, mc, 130);
        assert_eq!(pix, to_rgbe8(pix.to_hdr()));
        let pix = rgbe8(0, 0, mc, 140);
        assert_eq!(pix, to_rgbe8(pix.to_hdr()));
        let pix = rgbe8(1, 0, mc, 150);
        assert_eq!(pix, to_rgbe8(pix.to_hdr()));
        let pix = rgbe8(1, mc, 10, 128);
        assert_eq!(pix, to_rgbe8(pix.to_hdr()));
        for c in 0..255 {
            // Radiance HDR seems to be pre IEEE 754.
            // exponent can be -128 (represented as 0u8), so some colors cannot be represented in normalized f32
            // Let's exclude exponent value of -128 (0u8) from testing
            let pix = rgbe8(1, mc, c, if c == 0 { 1 } else { c });
            assert_eq!(pix, to_rgbe8(pix.to_hdr()));
        }
    }
    fn relative_dist(a: Rgb<f32>, b: Rgb<f32>) -> f32 {
        // maximal difference divided by maximal value
        let max_diff = a.0
            .iter()
            .zip(b.0.iter())
            .fold(0.0, |diff, (&a, &b)| f32::max(diff, (a - b).abs()));
        let max_val = a.0
            .iter()
            .chain(b.0.iter())
            .fold(0.0, |maxv, &a| f32::max(maxv, a));
        if max_val == 0.0 {
            0.0
        } else {
            max_diff / max_val
        }
    }
    let test_values = vec![
        0.000_001, 0.000_02, 0.000_3, 0.004, 0.05, 0.6, 7.0, 80.0, 900.0, 1_000.0, 20_000.0,
        300_000.0,
    ];
    for &r in &test_values {
        for &g in &test_values {
            for &b in &test_values {
                let c1 = Rgb([r, g, b]);
                let c2 = to_rgbe8(c1).to_hdr();
                let rel_dist = relative_dist(c1, c2);
                // Maximal value is normalized to the range 128..256, thus we have 1/128 precision
                assert!(
                    rel_dist <= 1.0 / 128.0,
                    "Relative distance ({}) exceeds 1/128 for {:?} and {:?}",
                    rel_dist,
                    c1,
                    c2
                );
            }
        }
    }
}

#[test]
fn runiterator_test() {
    let data = [];
    let mut run_iter = RunIterator::new(&data[..]);
    assert_eq!(run_iter.next(), None);
    let data = [5];
    let mut run_iter = RunIterator::new(&data[..]);
    assert_eq!(run_iter.next(), Some(Norun(0, 1)));
    assert_eq!(run_iter.next(), None);
    let data = [1, 1];
    let mut run_iter = RunIterator::new(&data[..]);
    assert_eq!(run_iter.next(), Some(Norun(0, 2)));
    assert_eq!(run_iter.next(), None);
    let data = [0, 0, 0];
    let mut run_iter = RunIterator::new(&data[..]);
    assert_eq!(run_iter.next(), Some(Run(0u8, 3)));
    assert_eq!(run_iter.next(), None);
    let data = [0, 0, 1, 1];
    let mut run_iter = RunIterator::new(&data[..]);
    assert_eq!(run_iter.next(), Some(Norun(0, 2)));
    assert_eq!(run_iter.next(), Some(Norun(2, 2)));
    assert_eq!(run_iter.next(), None);
    let data = [0, 0, 0, 1, 1];
    let mut run_iter = RunIterator::new(&data[..]);
    assert_eq!(run_iter.next(), Some(Run(0u8, 3)));
    assert_eq!(run_iter.next(), Some(Norun(3, 2)));
    assert_eq!(run_iter.next(), None);
    let data = [1, 2, 2, 2];
    let mut run_iter = RunIterator::new(&data[..]);
    assert_eq!(run_iter.next(), Some(Norun(0, 1)));
    assert_eq!(run_iter.next(), Some(Run(2u8, 3)));
    assert_eq!(run_iter.next(), None);
    let data = [1, 1, 2, 2, 2];
    let mut run_iter = RunIterator::new(&data[..]);
    assert_eq!(run_iter.next(), Some(Norun(0, 2)));
    assert_eq!(run_iter.next(), Some(Run(2u8, 3)));
    assert_eq!(run_iter.next(), None);
    let data = [2; 128];
    let mut run_iter = RunIterator::new(&data[..]);
    assert_eq!(run_iter.next(), Some(Run(2u8, 127)));
    assert_eq!(run_iter.next(), Some(Norun(127, 1)));
    assert_eq!(run_iter.next(), None);
    let data = [2; 129];
    let mut run_iter = RunIterator::new(&data[..]);
    assert_eq!(run_iter.next(), Some(Run(2u8, 127)));
    assert_eq!(run_iter.next(), Some(Norun(127, 2)));
    assert_eq!(run_iter.next(), None);
    let data = [2; 130];
    let mut run_iter = RunIterator::new(&data[..]);
    assert_eq!(run_iter.next(), Some(Run(2u8, 127)));
    assert_eq!(run_iter.next(), Some(Run(2u8, 3)));
    assert_eq!(run_iter.next(), None);
}

#[test]
fn noruncombine_test() {
    fn a<T>(mut v: Vec<T>, mut other: Vec<T>) -> Vec<T> {
        v.append(&mut other);
        v
    }

    let v = vec![];
    let mut rsi = NorunCombineIterator::new(&v[..]);
    assert_eq!(rsi.next(), None);

    let v = vec![1];
    let mut rsi = NorunCombineIterator::new(&v[..]);
    assert_eq!(rsi.next(), Some(Norun(0, 1)));
    assert_eq!(rsi.next(), None);

    let v = vec![2, 2];
    let mut rsi = NorunCombineIterator::new(&v[..]);
    assert_eq!(rsi.next(), Some(Norun(0, 2)));
    assert_eq!(rsi.next(), None);

    let v = vec![3, 3, 3];
    let mut rsi = NorunCombineIterator::new(&v[..]);
    assert_eq!(rsi.next(), Some(Run(3, 3)));
    assert_eq!(rsi.next(), None);

    let v = vec![4, 4, 3, 3, 3];
    let mut rsi = NorunCombineIterator::new(&v[..]);
    assert_eq!(rsi.next(), Some(Norun(0, 2)));
    assert_eq!(rsi.next(), Some(Run(3, 3)));
    assert_eq!(rsi.next(), None);

    let v = vec![40; 400];
    let mut rsi = NorunCombineIterator::new(&v[..]);
    assert_eq!(rsi.next(), Some(Run(40, 127)));
    assert_eq!(rsi.next(), Some(Run(40, 127)));
    assert_eq!(rsi.next(), Some(Run(40, 127)));
    assert_eq!(rsi.next(), Some(Run(40, 19)));
    assert_eq!(rsi.next(), None);

    let v = a(a(vec![5; 3], vec![6; 129]), vec![7, 3, 7, 10, 255]);
    let mut rsi = NorunCombineIterator::new(&v[..]);
    assert_eq!(rsi.next(), Some(Run(5, 3)));
    assert_eq!(rsi.next(), Some(Run(6, 127)));
    assert_eq!(rsi.next(), Some(Norun(130, 7)));
    assert_eq!(rsi.next(), None);

    let v = a(a(vec![5; 2], vec![6; 129]), vec![7, 3, 7, 7, 255]);
    let mut rsi = NorunCombineIterator::new(&v[..]);
    assert_eq!(rsi.next(), Some(Norun(0, 2)));
    assert_eq!(rsi.next(), Some(Run(6, 127)));
    assert_eq!(rsi.next(), Some(Norun(129, 7)));
    assert_eq!(rsi.next(), None);

    let v: Vec<_> = ::std::iter::repeat(())
        .flat_map(|_| (0..2))
        .take(257)
        .collect();
    let mut rsi = NorunCombineIterator::new(&v[..]);
    assert_eq!(rsi.next(), Some(Norun(0, 128)));
    assert_eq!(rsi.next(), Some(Norun(128, 128)));
    assert_eq!(rsi.next(), Some(Norun(256, 1)));
    assert_eq!(rsi.next(), None);
}
