extern crate blake2_rfc;
extern crate scoped_threadpool;

use std::{fmt, mem};
use std::error::Error;
use self::blake2_rfc::blake2b::Blake2b;
use octword::u64x2;
use block::{ARGON2_BLOCK_BYTES, Block, Matrix};
use block;

#[derive(Eq, PartialEq, Copy, Clone, Debug)]
pub enum Variant {
    Argon2d = 0,
    Argon2i = 1,
}

const ARGON2_VERSION: u32 = 0x10;
const DEF_B2HASH_LEN: usize = 64;
const SLICES_PER_LANE: u32 = 4;

pub mod defaults {
    // from run.c
    pub const PASSES: u32 = 3;
    pub const KIB: u32 = 4096;
    pub const LANES: u32 = 1;
    pub const LENGTH: usize = 32;
}

fn split_u64(n: u64) -> (u32, u32) {
    ((n & 0xffffffff) as u32, (n >> 32) as u32)
}

fn xor_all(blocks: &Vec<&Block>) -> Block {
    if blocks.len() == 0 {
        block::zero()
    } else {
        let mut rv = blocks[0].clone();
        for &block in blocks.iter().skip(1) {
            rv ^= block;
        }
        rv
    }
}

fn as32le(k: u32) -> [u8; 4] { unsafe { mem::transmute(k.to_le()) } }

fn len32(t: &[u8]) -> [u8; 4] { as32le(t.len() as u32) }

macro_rules! b2hash {
    ($($bytes: expr),*) => {
        {
            let mut out: [u8; DEF_B2HASH_LEN] = unsafe { mem::uninitialized() };
            b2hash!(&mut out; $($bytes),*);
            out
        }
    };
    ($out: expr; $($bytes: expr),*) => {
        {
            let mut b = Blake2b::new($out.len());
            $(b.update($bytes));*;
            $out.clone_from_slice(b.finalize().as_bytes());
        }
    };
}

#[cfg_attr(rustfmt, rustfmt_skip)]
fn h0(lanes: u32, hash_length: u32, memory_kib: u32, passes: u32, version: u32,
      variant: Variant, p: &[u8], s: &[u8], k: &[u8], x: &[u8])
      -> [u8; 72] {
    let mut rv = [0 as u8; 72];
    b2hash!(&mut rv[0..DEF_B2HASH_LEN];
            &as32le(lanes), &as32le(hash_length), &as32le(memory_kib),
            &as32le(passes), &as32le(version), &as32le(variant as u32),
            &len32(p), p,
            &len32(s), s,
            &len32(k), k,
            &len32(x), x);
    rv
}

/// Main entry point for running Argon2 on customized parameters (cf. note for
/// `Argon2::new`).
pub struct Argon2 {
    passes: u32,
    lanes: u32,
    lanelen: u32,
    kib: u32,
    variant: Variant,
}

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum ParamErr {
    TooFewPasses,
    TooFewLanes,
    TooManyLanes,
    MinKiB(u64),
}

impl fmt::Display for ParamErr {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        use ParamErr::*;
        match *self {
            TooFewPasses | TooFewLanes | TooManyLanes => {
                write!(f, "{}", self.description())
            }
            MinKiB(k) => write!(f, "Memory parameter must be >= {} KiB.", k),
        }
    }
}

impl Error for ParamErr {
    fn description(&self) -> &str {
        use ParamErr::*;
        match *self {
            TooFewPasses => "Argon2 requires one or more passes to be run.",
            TooFewLanes | TooManyLanes => {
                "The number of lanes must be between one and 2^24 - 1."
            }
            MinKiB(_) => "Specified size of block matrix was too small.",
        }
    }
}

impl Argon2 {
    /// Returns an `Argon2` set to default input parameters. See below for a
    /// description of these parameters.
    pub fn default(v: Variant) -> Argon2 {
        Argon2::new(defaults::PASSES, defaults::LANES, defaults::KIB, v)
            .ok()
            .unwrap()
    }

    /// Use this to customize Argon2's time and memory cost parameters.
    /// Adjusting any of these will affect the value of the final hash result.
    ///
    /// `passes`: The number of block matrix iterations to perform. Increasing
    /// this forces hashing to take longer. Must be between 1 and 2^32 - 1.
    ///
    /// `lanes`: The degree of parallelism by which memory is filled during hash
    /// computation. Setting this to N instructs argon2rs to partition the block
    /// matrix into N lanes, simultaneously filling each lane in parallel with N
    /// threads. Must be between 1 and 2^24 - 1.
    ///
    /// `kib`: Desired total size of block matrix, in kibbibytes (1 KiB = 1024
    /// bytes). Increasing this forces hashing to use more memory in order to
    /// thwart ASIC-based attacks. Must be >= 8 * lanes.
    ///
    /// `variant`: Set this to `Variant::Argon2i` when hashing passwords.
    pub fn new(passes: u32, lanes: u32, kib: u32, variant: Variant)
               -> Result<Argon2, ParamErr> {
        if passes < 1 {
            Result::Err(ParamErr::TooFewPasses)
        } else if lanes < 1 {
            Result::Err(ParamErr::TooFewLanes)
        } else if 0x00ffffff < lanes {
            Result::Err(ParamErr::TooManyLanes)
        } else if (kib as u64) < 8 * lanes as u64 {
            Result::Err(ParamErr::MinKiB(8 * lanes as u64))
        } else {
            Result::Ok(Argon2 {
                passes: passes,
                lanes: lanes,
                lanelen: kib / (4 * lanes) * 4,
                kib: kib,
                variant: variant,
            })
        }
    }

    /// Runs the selected Argon2 variant over provided inputs, writing the final
    /// hash to the byte slice `out`. Note that the output length is assumed to
    /// be `out.len()` and must be between 4 and 2^32 - 1. The inputs are:
    ///
    /// `p`, the byte slice containing, typically, the plaintext (length 0 to
    /// 2^32-1);
    ///
    /// a salt `s` of length 8 to 2^32-1 bytes;
    ///
    /// `k`, an optional (length 0 to 32 bytes) secret value; and
    ///
    /// `x`, optional associated data length 0 to 2^32 - 1.
    pub fn hash(&self, out: &mut [u8], p: &[u8], s: &[u8], k: &[u8], x: &[u8]) {
        self.hash_impl(out, p, s, k, x, |_| {}, |_, _| {});
    }

    #[cfg_attr(rustfmt, rustfmt_skip)]
    fn hash_impl<F, G>(&self, out: &mut [u8], p: &[u8], s: &[u8], k: &[u8],
                       x: &[u8], mut h0_fn: F, mut pass_fn: G)
        where F: FnMut(&[u8]) -> (),
              G: FnMut(u32, &Matrix) -> ()
    {
        assert!(4 <= out.len() && out.len() <= 0xffffffff);
        assert!(p.len() <= 0xffffffff);
        assert!(8 <= s.len()  && s.len() <= 0xffffffff);
        assert!(k.len() <= 32);
        assert!(x.len() <= 0xffffffff);

        let mut blocks = Matrix::new(self.lanes, self.lanelen);
        let h0 = h0(self.lanes, out.len() as u32, self.kib, self.passes,
                    ARGON2_VERSION, self.variant, p, s, k, x);
        h0_fn(&h0);  // kats

        let mut pool = scoped_threadpool::Pool::new(self.lanes);
        if self.lanes > 1 {
            pool.scoped(|sc| {
                for (l, bref) in (0..self.lanes).zip(blocks.lanes_as_mut()) {
                    sc.execute(move || self.fill_first_slice(bref, h0, l));
                }
            });
        } else {
            self.fill_first_slice(&mut blocks, h0, 0);
        }

        // finish first pass. slices have to be filled in sync.
        self.fill_segment(0, 1, &mut blocks, &mut pool);
        pass_fn(0, &blocks);  // kats

        for p in 1..self.passes {
            self.fill_segment(p, 0, &mut blocks, &mut pool);
            pass_fn(p, &blocks);  // kats
        }

        h_prime(out, block::as_u8(&xor_all(&blocks.col(self.lanelen - 1))));
    }

    // `Matrix` is an array of 1-KiB blocks and organized as follows:
    //
    //     +------------------------ `lanelen` columns ------------------------+
    //     |                                 +-----------------+               |
    //     +--- slice ---+   +--- slice ---+ | +--- slice ---+ | +--- slice ---+
    //     |             |   |             | | |             | | |             |
    //  +- b b b b ...   b   b b b b ...   b | b b b b ...   b | b b b b ...   b
    //  |  b                                 |                 |
    //  |  ...                               |                 |
    //  +- b b b b ...   b   b b b b ...   b | b b b b ...   b | b b b b ...   b
    //  |                                    |                 |
    //  +--`lane` rows                       +---- segment ----+
    //
    //  where each `b` represents a 1-KiB block.
    //
    //  Some invariants:
    //  - There are always four slices.
    //  - `lanelen * lane = self.kib`.
    //  - Filling is done segment-by-segment.
    #[inline(always)]
    fn fill_segment(&self, pass: u32, slice_begin: u32, blocks: &mut Matrix,
                    pool: &mut scoped_threadpool::Pool) {
        if self.lanes == 1 {
            for slice in slice_begin..SLICES_PER_LANE {
                self.fill_slice(blocks, pass, 0, slice, 0);
            }
            return;
        }

        for slice in slice_begin..SLICES_PER_LANE {
            pool.scoped(|sc| {
                for (l, bref) in (0..self.lanes).zip(blocks.lanes_as_mut()) {
                    sc.execute(move || self.fill_slice(bref, pass, l, slice, 0));
                }
            });
        }
    }

    fn fill_first_slice(&self, blks: &mut Matrix, mut h0: [u8; 72], lane: u32) {
        // fill the first (of four) slice
        h0[68..72].clone_from_slice(&as32le(lane));

        h0[64..68].clone_from_slice(&as32le(0));
        h_prime(block::as_u8_mut(&mut blks[(lane, 0)]), &h0);

        h0[64..68].clone_from_slice(&as32le(1));
        h_prime(block::as_u8_mut(&mut blks[(lane, 1)]), &h0);

        // finish rest of first slice
        self.fill_slice(blks, 0, lane, 0, 2);
    }

    #[cfg_attr(rustfmt, rustfmt_skip)]
    fn fill_slice(&self, blks: &mut Matrix, pass: u32, lane: u32, slice: u32,
                  offset: u32) {
        let mut jgen = Gen2i::new(offset as usize, pass, lane, slice,
                                  self.lanes * self.lanelen, self.passes);
        let slicelen = self.lanelen / SLICES_PER_LANE;

        for idx in offset..slicelen {
            let (j1, j2) = if self.variant == Variant::Argon2i {
                jgen.nextj()
            } else {
                let col = self.prev(slice * slicelen + idx);
                split_u64((blks[(lane, col)])[0].0)
            };
            self.fill_block(blks, pass, lane, slice, idx, j1, j2);
        }
    }

    fn fill_block(&self, blks: &mut Matrix, pass: u32, lane: u32, slice: u32,
                  idx: u32, j1: u32, j2: u32) {
        let slicelen = self.lanelen / SLICES_PER_LANE;
        let ls = self.lanes;
        let z = index_alpha(pass, lane, slice, ls, idx, slicelen, j1, j2);

        let zth = match (pass, slice) {
            (0, 0) => (lane, z),
            _ => (j2 % self.lanes, z),
        };

        let cur = (lane, slice * slicelen + idx);
        let pre = (lane, self.prev(cur.1));
        let (wr, rd, refblk) = blks.get3(cur, pre, zth);
        g(wr, rd, refblk);
    }

    fn prev(&self, n: u32) -> u32 {
        if n > 0 { n - 1 } else { self.lanelen - 1 }
    }

    /// Provides read-only access to `(variant, kibbibytes, passes, lanes)`.
    pub fn params(&self) -> (Variant, u32, u32, u32) {
        (self.variant, self.kib, self.passes, self.lanes)
    }
}

/// Convenience wrapper around Argon2i for the majority of use cases where only
/// a password and salt are supplied. Note that a salt between 8 and 2^32-1
/// bytes must be provided.
pub fn argon2i_simple(password: &str, salt: &str) -> [u8; defaults::LENGTH] {
    let mut out = [0; defaults::LENGTH];
    let a2 = Argon2::default(Variant::Argon2i);
    a2.hash(&mut out, password.as_bytes(), salt.as_bytes(), &[], &[]);
    out
}

/// Convenience wrapper around Argon2d for the majority of use cases where only
/// a password and salt are supplied. Note that a salt between 8 and 2^32-1
/// bytes must be provided.
pub fn argon2d_simple(password: &str, salt: &str) -> [u8; defaults::LENGTH] {
    let mut out = [0; defaults::LENGTH];
    let a2 = Argon2::default(Variant::Argon2d);
    a2.hash(&mut out, password.as_bytes(), salt.as_bytes(), &[], &[]);
    out
}

fn h_prime(out: &mut [u8], input: &[u8]) {
    if out.len() <= DEF_B2HASH_LEN {
        b2hash!(out; &len32(out), input);
    } else {
        let mut tmp = b2hash!(&len32(out), input);
        out[0..DEF_B2HASH_LEN].clone_from_slice(&tmp);
        let mut wr_at: usize = 32;

        while out.len() - wr_at > DEF_B2HASH_LEN {
            b2hash!(&mut tmp; &tmp);
            out[wr_at..wr_at + DEF_B2HASH_LEN].clone_from_slice(&tmp);
            wr_at += DEF_B2HASH_LEN / 2;
        }

        let len = out.len() - wr_at;
        b2hash!(&mut out[wr_at..wr_at + len]; &tmp);
    }
}

// from opt.c
fn index_alpha(pass: u32, lane: u32, slice: u32, lanes: u32, sliceidx: u32,
               slicelen: u32, j1: u32, j2: u32)
               -> u32 {
    let lanelen = slicelen * 4;
    let r: u32 = match (pass, slice, j2 % lanes == lane) {
        (0, 0, _) => sliceidx - 1,
        (0, _, false) => slice * slicelen - if sliceidx == 0 { 1 } else { 0 },
        (0, _, true) => slice * slicelen + sliceidx - 1,
        (_, _, false) => lanelen - slicelen - if sliceidx == 0 { 1 } else { 0 },
        (_, _, true) => lanelen - slicelen + sliceidx - 1,
    };

    let (r_, j1_) = (r as u64, j1 as u64);
    let relpos: u32 = (r_ - 1 - (r_ * (j1_ * j1_ >> 32) >> 32)) as u32;

    match (pass, slice) {
        (0, _) | (_, 3) => relpos % lanelen,
        _ => (slicelen * (slice + 1) + relpos) % lanelen,
    }
}

struct Gen2i {
    arg: Block,
    pseudos: Block,
    idx: usize,
}

impl Gen2i {
    #[cfg_attr(rustfmt, rustfmt_skip)]
    fn new(start_at: usize, pass: u32, lane: u32, slice: u32, totblocks: u32,
           totpasses: u32)
           -> Gen2i {
        use block::zero;

        let mut rv = Gen2i { arg: zero(), pseudos: zero(), idx: start_at };
        let args = [(pass, lane), (slice, totblocks),
                    (totpasses, Variant::Argon2i as u32)];
        for (k, &(lo, hi)) in rv.arg.iter_mut().zip(args.into_iter()) {
            *k = u64x2(lo as u64, hi as u64);
        }
        rv.more();
        rv
    }

    fn more(&mut self) {
        self.arg[3].0 += 1;
        g_two(&mut self.pseudos, &self.arg);
    }

    fn nextj(&mut self) -> (u32, u32) {
        let rv = split_u64(block::as_u64(&self.pseudos)[self.idx]);
        self.idx = (self.idx + 1) % per_kib!(u64);
        if self.idx == 0 {
            self.more();
        }
        rv
    }
}

// g x y = let r = x `xor` y in p_col (p_row r) `xor` r,
fn g(dest: &mut Block, lhs: &Block, rhs: &Block) {
    for (d, (l, r)) in dest.iter_mut().zip(lhs.iter().zip(rhs.iter())) {
        *d = *l ^ *r;
    }

    for row in 0..8 {
        p_row(row, dest);
    }
    // column-wise, 2x u64 groups
    for col in 0..8 {
        p_col(col, dest);
    }

    *dest ^= (lhs, rhs);
}

/// ``` g2 y = let g' y = g 0 y in g' . g' ```
/// Used for data-independent index generation.
fn g_two(dest: &mut Block, src: &Block) {
    *dest = src.clone();

    for row in 0..8 {
        p_row(row, dest);
    }
    for col in 0..8 {
        p_col(col, dest);
    }

    *dest ^= src;

    let tmp: Block = dest.clone();

    for row in 0..8 {
        p_row(row, dest);
    }
    for col in 0..8 {
        p_col(col, dest);
    }

    *dest ^= &tmp;
}

macro_rules! p {
    ($v0v1: expr, $v2v3: expr, $v4v5: expr, $v6v7: expr,
     $v8v9: expr, $v10v11: expr, $v12v13: expr, $v14v15: expr) => {
        {
            g_blake2b!($v0v1, $v4v5, $v8v9, $v12v13);
            g_blake2b!($v2v3, $v6v7, $v10v11, $v14v15);

            let (mut v7v4, mut v5v6) = $v4v5.cross_swap($v6v7);
            let (mut v15v12, mut v13v14) = $v12v13.cross_swap($v14v15);

            g_blake2b!($v0v1, v5v6, $v10v11, v15v12);
            g_blake2b!($v2v3, v7v4, $v8v9, v13v14);

            let (v4v5, v6v7) = v5v6.cross_swap(v7v4);
            let (v12v13, v14v15) = v13v14.cross_swap(v15v12);
            $v4v5 = v4v5;
            $v6v7 = v6v7;
            $v12v13 = v12v13;
            $v14v15 = v14v15;
        }
    };
}

macro_rules! g_blake2b {
    ($a: expr, $b: expr, $c: expr, $d: expr) => {
        $a = $a + $b + $a.lower_mult($b) * u64x2(2, 2);
        $d = ($d ^ $a).rotate_right(32);
        $c = $c + $d + $c.lower_mult($d) * u64x2(2, 2);
        $b = ($b ^ $c).rotate_right(24);
        $a = $a + $b + $a.lower_mult($b) * u64x2(2, 2);
        $d = ($d ^ $a).rotate_right(16);
        $c = $c + $d + $c.lower_mult($d) * u64x2(2, 2);
        $b = ($b ^ $c).rotate_right(63);
    };
}


#[cfg_attr(rustfmt, rustfmt_skip)]
fn p_row(row: usize, b: &mut Block) {
    p!(b[8 * row + 0], b[8 * row + 1], b[8 * row + 2], b[8 * row + 3],
       b[8 * row + 4], b[8 * row + 5], b[8 * row + 6], b[8 * row + 7]);
}

#[cfg_attr(rustfmt, rustfmt_skip)]
fn p_col(col: usize, b: &mut Block) {
    p!(b[8 * 0 + col], b[8 * 1 + col], b[8 * 2 + col], b[8 * 3 + col],
       b[8 * 4 + col], b[8 * 5 + col], b[8 * 6 + col], b[8 * 7 + col]);
}

#[cfg(test)]
mod tests {
    use std::fs::File;
    use std::io::Read;
    use super::Argon2;
    use block;
    use std::fmt::Write;

    // from genkat.c
    const TEST_OUTLEN: usize = 32;
    const TEST_PWDLEN: usize = 32;
    const TEST_SALTLEN: usize = 16;
    const TEST_SECRETLEN: usize = 8;
    const TEST_ADLEN: usize = 12;

    macro_rules! w { ($($args: expr),*) => { let _ = write!($($args),*); }; }
    macro_rules! wl { ($($args: expr),*) => { let _ = writeln!($($args),*); }; }

    fn u8info(prefix: &str, bytes: &[u8], print_length: bool) -> String {
        let bs = bytes.iter()
                      .fold(String::new(), |xs, b| xs + &format!("{:02x} ", b));
        let len = match print_length {
            false => ": ".to_string(),
            true => format!("[{}]: ", bytes.len()),
        };
        prefix.to_string() + &len + &bs

    }

    fn block_info(i: usize, b: &block::Block) -> String {
        let blk = block::as_u64(b);
        blk.iter().enumerate().fold(String::new(), |xs, (j, octword)| {
            xs + "Block " + &format!("{:004} ", i) + &format!("[{:>3}]: ", j) +
            &format!("{:0016x}", octword) + "\n"
        })
    }

    fn run_and_collect(arg: &Argon2, out: &mut [u8], p: &[u8], s: &[u8],
                       k: &[u8], x: &[u8])
                       -> (String, String) {
        let (mut h0output, mut blockoutput) = (String::new(), String::new());

        {
            let h0fn = |h0: &[u8]| {
                wl!(&mut h0output,
                    "{}",
                    u8info("Pre-hashing digest",
                           &h0[..super::DEF_B2HASH_LEN],
                           false));
            };

            let passfn = |p: u32, matrix: &block::Matrix| {
                wl!(&mut blockoutput, "\n After pass {}:", p);
                for (i, block) in matrix.iter().flat_map(|ls| ls).enumerate() {
                    w!(&mut blockoutput, "{}", block_info(i, block));
                }
            };

            arg.hash_impl(out, p, s, k, x, h0fn, passfn);
        }

        (h0output, blockoutput)
    }

    fn compare_kats(fexpected: &str, variant: super::Variant) {
        let mut f = File::open(fexpected).unwrap();
        let mut expected = String::new();
        f.read_to_string(&mut expected).unwrap();

        let (p, s) = (&[1; TEST_PWDLEN], &[2; TEST_SALTLEN]);
        let (k, x) = (&[3; TEST_SECRETLEN], &[4; TEST_ADLEN]);
        let mut out = [0 as u8; TEST_OUTLEN];
        let a2 = Argon2::new(3, 4, 32, variant).ok().unwrap();
        let (h0, blocks) = run_and_collect(&a2, &mut out, p, s, k, x);

        let mut rv = String::new();
        wl!(rv, "======================================={:?}", a2.variant);
        w!(rv, "Memory: {} KiB, Iterations: {}, ", a2.kib, a2.passes);
        w!(rv, "Parallelism: {} lanes, ", a2.lanes);
        wl!(rv, "Tag length: {} bytes", out.len());
        wl!(rv, "{}", u8info("Password", p, true));
        wl!(rv, "{}", u8info("Salt", s, true));
        wl!(rv, "{}", u8info("Secret", k, true));
        wl!(rv, "{}", u8info("Associated data", x, true));
        w!(rv, "{}", h0 + &blocks);
        wl!(rv, "{}", u8info("Tag", &out, false));

        if expected.trim() != rv.trim() {
            println!("{}", rv);
            assert!(false);
        }
    }

    #[test]
    fn argon2i_kat() { compare_kats("kats/argon2i", super::Variant::Argon2i); }

    #[test]
    fn argon2d_kat() { compare_kats("kats/argon2d", super::Variant::Argon2d); }
}
