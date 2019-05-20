// Copyright 2018 Developers of the Rand project.
// Copyright 2013-2018 The Rust Project Developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! The ISAAC-64 random number generator.

use core::{fmt, slice};
use core::num::Wrapping as w;
use rand_core::{RngCore, SeedableRng, Error, le};
use rand_core::block::{BlockRngCore, BlockRng64};
use isaac_array::IsaacArray;

#[allow(non_camel_case_types)]
type w64 = w<u64>;

const RAND_SIZE_LEN: usize = 8;
const RAND_SIZE: usize = 1 << RAND_SIZE_LEN;

/// A random number generator that uses ISAAC-64, the 64-bit variant of the
/// ISAAC algorithm.
///
/// ISAAC stands for "Indirection, Shift, Accumulate, Add, and Count" which are
/// the principal bitwise operations employed. It is the most advanced of a
/// series of array based random number generator designed by Robert Jenkins
/// in 1996[^1].
///
/// ISAAC-64 is mostly similar to ISAAC. Because it operates on 64-bit integers
/// instead of 32-bit, it uses twice as much memory to hold its state and
/// results. Also it uses different constants for shifts and indirect indexing,
/// optimized to give good results for 64bit arithmetic.
///
/// ISAAC-64 is notably fast and produces excellent quality random numbers for
/// non-cryptographic applications.
///
/// In spite of being designed with cryptographic security in mind, ISAAC hasn't
/// been stringently cryptanalyzed and thus cryptographers do not not
/// consensually trust it to be secure. When looking for a secure RNG, prefer
/// [`Hc128Rng`] instead, which, like ISAAC, is an array-based RNG and one of
/// the stream-ciphers selected the by eSTREAM contest.
///
/// ## Overview of the ISAAC-64 algorithm:
/// (in pseudo-code)
///
/// ```text
/// Input: a, b, c, s[256] // state
/// Output: r[256] // results
///
/// mix(a,i) = !(a ^ a << 21)  if i = 0 mod 4
///              a ^ a >>  5   if i = 1 mod 4
///              a ^ a << 12   if i = 2 mod 4
///              a ^ a >> 33   if i = 3 mod 4
///
/// c = c + 1
/// b = b + c
///
/// for i in 0..256 {
///     x = s_[i]
///     a = mix(a,i) + s[i+128 mod 256]
///     y = a + b + s[x>>3 mod 256]
///     s[i] = y
///     b = x + s[y>>11 mod 256]
///     r[i] = b
/// }
/// ```
///
/// This implementation uses [`BlockRng64`] to implement the [`RngCore`] methods.
///
/// See for more information the documentation of [`IsaacRng`].
///
/// [^1]: Bob Jenkins, [*ISAAC and RC4*](
///       http://burtleburtle.net/bob/rand/isaac.html)
///
/// [`IsaacRng`]: ../isaac/struct.IsaacRng.html
/// [`Hc128Rng`]: ../../rand_hc/struct.Hc128Rng.html
/// [`BlockRng64`]: ../../rand_core/block/struct.BlockRng64.html
/// [`RngCore`]: ../../rand_core/trait.RngCore.html
#[derive(Clone, Debug)]
#[cfg_attr(feature="serde1", derive(Serialize, Deserialize))]
pub struct Isaac64Rng(BlockRng64<Isaac64Core>);

impl RngCore for Isaac64Rng {
    #[inline(always)]
    fn next_u32(&mut self) -> u32 {
        self.0.next_u32()
    }

    #[inline(always)]
    fn next_u64(&mut self) -> u64 {
        self.0.next_u64()
    }

    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.0.fill_bytes(dest)
    }

    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

impl SeedableRng for Isaac64Rng {
    type Seed = <Isaac64Core as SeedableRng>::Seed;

    fn from_seed(seed: Self::Seed) -> Self {
        Isaac64Rng(BlockRng64::<Isaac64Core>::from_seed(seed))
    }

    /// Create an ISAAC random number generator using an `u64` as seed.
    /// If `seed == 0` this will produce the same stream of random numbers as
    /// the reference implementation when used unseeded.
    fn seed_from_u64(seed: u64) -> Self {
        Isaac64Rng(BlockRng64::<Isaac64Core>::seed_from_u64(seed))
    }

    fn from_rng<S: RngCore>(rng: S) -> Result<Self, Error> {
        BlockRng64::<Isaac64Core>::from_rng(rng).map(|rng| Isaac64Rng(rng))
    }
}

impl Isaac64Rng {
    /// Create an ISAAC-64 random number generator using an `u64` as seed.
    /// If `seed == 0` this will produce the same stream of random numbers as
    /// the reference implementation when used unseeded.
    #[deprecated(since="0.6.0", note="use SeedableRng::seed_from_u64 instead")]
    pub fn new_from_u64(seed: u64) -> Self {
        Self::seed_from_u64(seed)
    }
}

/// The core of `Isaac64Rng`, used with `BlockRng`.
#[derive(Clone)]
#[cfg_attr(feature="serde1", derive(Serialize, Deserialize))]
pub struct Isaac64Core {
    #[cfg_attr(feature="serde1",serde(with="super::isaac_array::isaac_array_serde"))]
    mem: [w64; RAND_SIZE],
    a: w64,
    b: w64,
    c: w64,
}

// Custom Debug implementation that does not expose the internal state
impl fmt::Debug for Isaac64Core {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Isaac64Core {{}}")
    }
}

impl BlockRngCore for Isaac64Core {
    type Item = u64;
    type Results = IsaacArray<Self::Item>;

    /// Refills the output buffer, `results`. See also the pseudocode desciption
    /// of the algorithm in the [`Isaac64Rng`] documentation.
    ///
    /// Optimisations used (similar to the reference implementation):
    /// 
    /// - The loop is unrolled 4 times, once for every constant of mix().
    /// - The contents of the main loop are moved to a function `rngstep`, to
    ///   reduce code duplication.
    /// - We use local variables for a and b, which helps with optimisations.
    /// - We split the main loop in two, one that operates over 0..128 and one
    ///   over 128..256. This way we can optimise out the addition and modulus
    ///   from `s[i+128 mod 256]`.
    /// - We maintain one index `i` and add `m` or `m2` as base (m2 for the
    ///   `s[i+128 mod 256]`), relying on the optimizer to turn it into pointer
    ///   arithmetic.
    /// - We fill `results` backwards. The reference implementation reads values
    ///   from `results` in reverse. We read them in the normal direction, to
    ///   make `fill_bytes` a memcopy. To maintain compatibility we fill in
    ///   reverse.
    /// 
    /// [`Isaac64Rng`]: struct.Isaac64Rng.html
    fn generate(&mut self, results: &mut IsaacArray<Self::Item>) {
        self.c += w(1);
        // abbreviations
        let mut a = self.a;
        let mut b = self.b + self.c;
        const MIDPOINT: usize = RAND_SIZE / 2;

        #[inline]
        fn ind(mem:&[w64; RAND_SIZE], v: w64, amount: usize) -> w64 {
            let index = (v >> amount).0 as usize % RAND_SIZE;
            mem[index]
        }

        #[inline]
        fn rngstep(mem: &mut [w64; RAND_SIZE],
                   results: &mut [u64; RAND_SIZE],
                   mix: w64,
                   a: &mut w64,
                   b: &mut w64,
                   base: usize,
                   m: usize,
                   m2: usize) {
            let x = mem[base + m];
            *a = mix + mem[base + m2];
            let y = *a + *b + ind(&mem, x, 3);
            mem[base + m] = y;
            *b = x + ind(&mem, y, 3 + RAND_SIZE_LEN);
            results[RAND_SIZE - 1 - base - m] = (*b).0;
        }

        let mut m = 0;
        let mut m2 = MIDPOINT;
        for i in (0..MIDPOINT/4).map(|i| i * 4) {
            rngstep(&mut self.mem, results, !(a ^ (a << 21)), &mut a, &mut b, i + 0, m, m2);
            rngstep(&mut self.mem, results,   a ^ (a >> 5 ),  &mut a, &mut b, i + 1, m, m2);
            rngstep(&mut self.mem, results,   a ^ (a << 12),  &mut a, &mut b, i + 2, m, m2);
            rngstep(&mut self.mem, results,   a ^ (a >> 33),  &mut a, &mut b, i + 3, m, m2);
        }

        m = MIDPOINT;
        m2 = 0;
        for i in (0..MIDPOINT/4).map(|i| i * 4) {
            rngstep(&mut self.mem, results, !(a ^ (a << 21)), &mut a, &mut b, i + 0, m, m2);
            rngstep(&mut self.mem, results,   a ^ (a >> 5 ),  &mut a, &mut b, i + 1, m, m2);
            rngstep(&mut self.mem, results,   a ^ (a << 12),  &mut a, &mut b, i + 2, m, m2);
            rngstep(&mut self.mem, results,   a ^ (a >> 33),  &mut a, &mut b, i + 3, m, m2);
        }

        self.a = a;
        self.b = b;
    }
}

impl Isaac64Core {
    /// Create a new ISAAC-64 random number generator.
    fn init(mut mem: [w64; RAND_SIZE], rounds: u32) -> Self {
        fn mix(a: &mut w64, b: &mut w64, c: &mut w64, d: &mut w64,
               e: &mut w64, f: &mut w64, g: &mut w64, h: &mut w64) {
            *a -= *e; *f ^= *h >> 9;  *h += *a;
            *b -= *f; *g ^= *a << 9;  *a += *b;
            *c -= *g; *h ^= *b >> 23; *b += *c;
            *d -= *h; *a ^= *c << 15; *c += *d;
            *e -= *a; *b ^= *d >> 14; *d += *e;
            *f -= *b; *c ^= *e << 20; *e += *f;
            *g -= *c; *d ^= *f >> 17; *f += *g;
            *h -= *d; *e ^= *g << 14; *g += *h;
        }

        // These numbers are the result of initializing a...h with the
        // fractional part of the golden ratio in binary (0x9e3779b97f4a7c13)
        // and applying mix() 4 times.
        let mut a = w(0x647c4677a2884b7c);
        let mut b = w(0xb9f8b322c73ac862);
        let mut c = w(0x8c0ea5053d4712a0);
        let mut d = w(0xb29b2e824a595524);
        let mut e = w(0x82f053db8355e0ce);
        let mut f = w(0x48fe4a0fa5a09315);
        let mut g = w(0xae985bf2cbfc89ed);
        let mut h = w(0x98f5704f6c44c0ab);

        // Normally this should do two passes, to make all of the seed effect
        // all of `mem`
        for _ in 0..rounds {
            for i in (0..RAND_SIZE/8).map(|i| i * 8) {
                a += mem[i  ]; b += mem[i+1];
                c += mem[i+2]; d += mem[i+3];
                e += mem[i+4]; f += mem[i+5];
                g += mem[i+6]; h += mem[i+7];
                mix(&mut a, &mut b, &mut c, &mut d,
                    &mut e, &mut f, &mut g, &mut h);
                mem[i  ] = a; mem[i+1] = b;
                mem[i+2] = c; mem[i+3] = d;
                mem[i+4] = e; mem[i+5] = f;
                mem[i+6] = g; mem[i+7] = h;
            }
        }

        Self { mem, a: w(0), b: w(0), c: w(0) }
    }

    /// Create an ISAAC-64 random number generator using an `u64` as seed.
    /// If `seed == 0` this will produce the same stream of random numbers as
    /// the reference implementation when used unseeded.
    #[deprecated(since="0.6.0", note="use SeedableRng::seed_from_u64 instead")]
    pub fn new_from_u64(seed: u64) -> Self {
        Self::seed_from_u64(seed)
    }
}

impl SeedableRng for Isaac64Core {
    type Seed = [u8; 32];

    fn from_seed(seed: Self::Seed) -> Self {
        let mut seed_u64 = [0u64; 4];
        le::read_u64_into(&seed, &mut seed_u64);
        // Convert the seed to `Wrapping<u64>` and zero-extend to `RAND_SIZE`.
        let mut seed_extended = [w(0); RAND_SIZE];
        for (x, y) in seed_extended.iter_mut().zip(seed_u64.iter()) {
            *x = w(*y);
        }
        Self::init(seed_extended, 2)
    }
    
    fn seed_from_u64(seed: u64) -> Self {
        let mut key = [w(0); RAND_SIZE];
        key[0] = w(seed);
        // Initialize with only one pass.
        // A second pass does not improve the quality here, because all of the
        // seed was already available in the first round.
        // Not doing the second pass has the small advantage that if
        // `seed == 0` this method produces exactly the same state as the
        // reference implementation when used unseeded.
        Self::init(key, 1)
    }

    fn from_rng<R: RngCore>(mut rng: R) -> Result<Self, Error> {
        // Custom `from_rng` implementation that fills a seed with the same size
        // as the entire state.
        let mut seed = [w(0u64); RAND_SIZE];
        unsafe {
            let ptr = seed.as_mut_ptr() as *mut u8;
            let slice = slice::from_raw_parts_mut(ptr, RAND_SIZE * 8);
            rng.try_fill_bytes(slice)?;
        }
        for i in seed.iter_mut() {
            *i = w(i.0.to_le());
        }

        Ok(Self::init(seed, 2))
    }
}

#[cfg(test)]
mod test {
    use rand_core::{RngCore, SeedableRng};
    use super::Isaac64Rng;

    #[test]
    fn test_isaac64_construction() {
        // Test that various construction techniques produce a working RNG.
        let seed = [1,0,0,0, 23,0,0,0, 200,1,0,0, 210,30,0,0,
                    0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0];
        let mut rng1 = Isaac64Rng::from_seed(seed);
        assert_eq!(rng1.next_u64(), 14964555543728284049);

        let mut rng2 = Isaac64Rng::from_rng(rng1).unwrap();
        assert_eq!(rng2.next_u64(), 919595328260451758);
    }

    #[test]
    fn test_isaac64_true_values_64() {
        let seed = [1,0,0,0, 0,0,0,0, 23,0,0,0, 0,0,0,0,
                    200,1,0,0, 0,0,0,0, 210,30,0,0, 0,0,0,0];
        let mut rng1 = Isaac64Rng::from_seed(seed);
        let mut results = [0u64; 10];
        for i in results.iter_mut() { *i = rng1.next_u64(); }
        let expected = [
                   15071495833797886820, 7720185633435529318,
                   10836773366498097981, 5414053799617603544,
                   12890513357046278984, 17001051845652595546,
                   9240803642279356310, 12558996012687158051,
                   14673053937227185542, 1677046725350116783];
        assert_eq!(results, expected);

        let seed = [57,48,0,0, 0,0,0,0, 50,9,1,0, 0,0,0,0,
                    49,212,0,0, 0,0,0,0, 148,38,0,0, 0,0,0,0];
        let mut rng2 = Isaac64Rng::from_seed(seed);
        // skip forward to the 10000th number
        for _ in 0..10000 { rng2.next_u64(); }

        for i in results.iter_mut() { *i = rng2.next_u64(); }
        let expected = [
            18143823860592706164, 8491801882678285927, 2699425367717515619,
            17196852593171130876, 2606123525235546165, 15790932315217671084,
            596345674630742204, 9947027391921273664, 11788097613744130851,
            10391409374914919106];
        assert_eq!(results, expected);
    }

    #[test]
    fn test_isaac64_true_values_32() {
        let seed = [1,0,0,0, 0,0,0,0, 23,0,0,0, 0,0,0,0,
                    200,1,0,0, 0,0,0,0, 210,30,0,0, 0,0,0,0];
        let mut rng = Isaac64Rng::from_seed(seed);
        let mut results = [0u32; 12];
        for i in results.iter_mut() { *i = rng.next_u32(); }
        // Subset of above values, as an LE u32 sequence
        let expected = [
                    3477963620, 3509106075,
                    687845478, 1797495790,
                    227048253, 2523132918,
                    4044335064, 1260557630,
                    4079741768, 3001306521,
                    69157722, 3958365844];
        assert_eq!(results, expected);
    }

    #[test]
    fn test_isaac64_true_values_mixed() {
        let seed = [1,0,0,0, 0,0,0,0, 23,0,0,0, 0,0,0,0,
                    200,1,0,0, 0,0,0,0, 210,30,0,0, 0,0,0,0];
        let mut rng = Isaac64Rng::from_seed(seed);
        // Test alternating between `next_u64` and `next_u32` works as expected.
        // Values are the same as `test_isaac64_true_values` and
        // `test_isaac64_true_values_32`.
        assert_eq!(rng.next_u64(), 15071495833797886820);
        assert_eq!(rng.next_u32(), 687845478);
        assert_eq!(rng.next_u32(), 1797495790);
        assert_eq!(rng.next_u64(), 10836773366498097981);
        assert_eq!(rng.next_u32(), 4044335064);
        // Skip one u32
        assert_eq!(rng.next_u64(), 12890513357046278984);
        assert_eq!(rng.next_u32(), 69157722);
    }

    #[test]
    fn test_isaac64_true_bytes() {
        let seed = [1,0,0,0, 0,0,0,0, 23,0,0,0, 0,0,0,0,
                    200,1,0,0, 0,0,0,0, 210,30,0,0, 0,0,0,0];
        let mut rng = Isaac64Rng::from_seed(seed);
        let mut results = [0u8; 32];
        rng.fill_bytes(&mut results);
        // Same as first values in test_isaac64_true_values as bytes in LE order
        let expected = [100, 131, 77, 207, 155, 181, 40, 209,
                        102, 176, 255, 40, 238, 155, 35, 107,
                        61, 123, 136, 13, 246, 243, 99, 150,
                        216, 167, 15, 241, 62, 149, 34, 75];
        assert_eq!(results, expected);
    }

    #[test]
    fn test_isaac64_new_uninitialized() {
        // Compare the results from initializing `IsaacRng` with
        // `seed_from_u64(0)`, to make sure it is the same as the reference
        // implementation when used uninitialized.
        // Note: We only test the first 16 integers, not the full 256 of the
        // first block.
        let mut rng = Isaac64Rng::seed_from_u64(0);
        let mut results = [0u64; 16];
        for i in results.iter_mut() { *i = rng.next_u64(); }
        let expected: [u64; 16] = [
            0xF67DFBA498E4937C, 0x84A5066A9204F380, 0xFEE34BD5F5514DBB,
            0x4D1664739B8F80D6, 0x8607459AB52A14AA, 0x0E78BC5A98529E49,
            0xFE5332822AD13777, 0x556C27525E33D01A, 0x08643CA615F3149F,
            0xD0771FAF3CB04714, 0x30E86F68A37B008D, 0x3074EBC0488A3ADF,
            0x270645EA7A2790BC, 0x5601A0A8D3763C6A, 0x2F83071F53F325DD,
            0xB9090F3D42D2D2EA];
        assert_eq!(results, expected);
    }

    #[test]
    fn test_isaac64_clone() {
        let seed = [1,0,0,0, 0,0,0,0, 23,0,0,0, 0,0,0,0,
                    200,1,0,0, 0,0,0,0, 210,30,0,0, 0,0,0,0];
        let mut rng1 = Isaac64Rng::from_seed(seed);
        let mut rng2 = rng1.clone();
        for _ in 0..16 {
            assert_eq!(rng1.next_u64(), rng2.next_u64());
        }
    }

    #[test]
    #[cfg(feature="serde1")]
    fn test_isaac64_serde() {
        use bincode;
        use std::io::{BufWriter, BufReader};

        let seed = [1,0,0,0, 23,0,0,0, 200,1,0,0, 210,30,0,0,
                     57,48,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0];
        let mut rng = Isaac64Rng::from_seed(seed);

        let buf: Vec<u8> = Vec::new();
        let mut buf = BufWriter::new(buf);
        bincode::serialize_into(&mut buf, &rng).expect("Could not serialize");

        let buf = buf.into_inner().unwrap();
        let mut read = BufReader::new(&buf[..]);
        let mut deserialized: Isaac64Rng = bincode::deserialize_from(&mut read).expect("Could not deserialize");

        for _ in 0..300 { // more than the 256 buffered results
            assert_eq!(rng.next_u64(), deserialized.next_u64());
        }
    }
}
