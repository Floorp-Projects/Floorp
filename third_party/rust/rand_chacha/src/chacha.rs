// Copyright 2018 Developers of the Rand project.
// Copyright 2014 The Rust Project Developers.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! The ChaCha random number generator.

use core::fmt;
use rand_core::{CryptoRng, RngCore, SeedableRng, Error, le};
use rand_core::block::{BlockRngCore, BlockRng};

const SEED_WORDS: usize = 8; // 8 words for the 256-bit key
const STATE_WORDS: usize = 16;

/// A cryptographically secure random number generator that uses the ChaCha
/// algorithm.
///
/// ChaCha is a stream cipher designed by Daniel J. Bernstein[^1], that we use
/// as an RNG. It is an improved variant of the Salsa20 cipher family, which was
/// selected as one of the "stream ciphers suitable for widespread adoption" by
/// eSTREAM[^2].
///
/// ChaCha uses add-rotate-xor (ARX) operations as its basis. These are safe
/// against timing attacks, although that is mostly a concern for ciphers and
/// not for RNGs. Also it is very suitable for SIMD implementation.
/// Here we do not provide a SIMD implementation yet, except for what is
/// provided by auto-vectorisation.
///
/// With the ChaCha algorithm it is possible to choose the number of rounds the
/// core algorithm should run. The number of rounds is a tradeoff between
/// performance and security, where 8 rounds is the minimum potentially
/// secure configuration, and 20 rounds is widely used as a conservative choice.
/// We use 20 rounds in this implementation, but hope to allow type-level
/// configuration in the future.
///
/// We use a 64-bit counter and 64-bit stream identifier as in Bernstein's
/// implementation[^1] except that we use a stream identifier in place of a
/// nonce. A 64-bit counter over 64-byte (16 word) blocks allows 1 ZiB of output
/// before cycling, and the stream identifier allows 2<sup>64</sup> unique
/// streams of output per seed. Both counter and stream are initialized to zero
/// but may be set via [`set_word_pos`] and [`set_stream`].
///
/// The word layout is:
///
/// ```text
/// constant  constant  constant  constant
/// seed      seed      seed      seed
/// seed      seed      seed      seed
/// counter   counter   stream_id stream_id
/// ```
///
/// This implementation uses an output buffer of sixteen `u32` words, and uses
/// [`BlockRng`] to implement the [`RngCore`] methods.
///
/// [^1]: D. J. Bernstein, [*ChaCha, a variant of Salsa20*](
///       https://cr.yp.to/chacha.html)
///
/// [^2]: [eSTREAM: the ECRYPT Stream Cipher Project](
///       http://www.ecrypt.eu.org/stream/)
///
/// [`set_word_pos`]: #method.set_word_pos
/// [`set_stream`]: #method.set_stream
/// [`BlockRng`]: ../rand_core/block/struct.BlockRng.html
/// [`RngCore`]: ../rand_core/trait.RngCore.html
#[derive(Clone, Debug)]
pub struct ChaChaRng(BlockRng<ChaChaCore>);

impl RngCore for ChaChaRng {
    #[inline]
    fn next_u32(&mut self) -> u32 {
        self.0.next_u32()
    }

    #[inline]
    fn next_u64(&mut self) -> u64 {
        self.0.next_u64()
    }

    #[inline]
    fn fill_bytes(&mut self, dest: &mut [u8]) {
        self.0.fill_bytes(dest)
    }

    #[inline]
    fn try_fill_bytes(&mut self, dest: &mut [u8]) -> Result<(), Error> {
        self.0.try_fill_bytes(dest)
    }
}

impl SeedableRng for ChaChaRng {
    type Seed = <ChaChaCore as SeedableRng>::Seed;

    fn from_seed(seed: Self::Seed) -> Self {
        ChaChaRng(BlockRng::<ChaChaCore>::from_seed(seed))
    }

    fn from_rng<R: RngCore>(rng: R) -> Result<Self, Error> {
        BlockRng::<ChaChaCore>::from_rng(rng).map(ChaChaRng)
    }
}

impl CryptoRng for ChaChaRng {}

impl ChaChaRng {
    /// Get the offset from the start of the stream, in 32-bit words.
    ///
    /// Since the generated blocks are 16 words (2<sup>4</sup>) long and the
    /// counter is 64-bits, the offset is a 68-bit number. Sub-word offsets are
    /// not supported, hence the result can simply be multiplied by 4 to get a
    /// byte-offset.
    ///
    /// Note: this function is currently only available with Rust 1.26 or later.
    #[cfg(all(rustc_1_26, not(target_os = "emscripten")))]
    pub fn get_word_pos(&self) -> u128 {
        let mut c = (self.0.core.state[13] as u64) << 32
                  | (self.0.core.state[12] as u64);
        let mut index = self.0.index();
        // c is the end of the last block generated, unless index is at end
        if index >= STATE_WORDS {
            index = 0;
        } else {
            c = c.wrapping_sub(1);
        }
        ((c as u128) << 4) | (index as u128)
    }

    /// Set the offset from the start of the stream, in 32-bit words.
    ///
    /// As with `get_word_pos`, we use a 68-bit number. Since the generator
    /// simply cycles at the end of its period (1 ZiB), we ignore the upper
    /// 60 bits.
    ///
    /// Note: this function is currently only available with Rust 1.26 or later.
    #[cfg(all(rustc_1_26, not(target_os = "emscripten")))]
    pub fn set_word_pos(&mut self, word_offset: u128) {
        let index = (word_offset as usize) & 0xF;
        let counter = (word_offset >> 4) as u64;
        self.0.core.state[12] = counter as u32;
        self.0.core.state[13] = (counter >> 32) as u32;
        if index != 0 {
            self.0.generate_and_set(index); // also increments counter
        } else {
            self.0.reset();
        }
    }

    /// Set the stream number.
    ///
    /// This is initialized to zero; 2<sup>64</sup> unique streams of output
    /// are available per seed/key.
    ///
    /// Note that in order to reproduce ChaCha output with a specific 64-bit
    /// nonce, one can convert that nonce to a `u64` in little-endian fashion
    /// and pass to this function. In theory a 96-bit nonce can be used by
    /// passing the last 64-bits to this function and using the first 32-bits as
    /// the most significant half of the 64-bit counter (which may be set
    /// indirectly via `set_word_pos`), but this is not directly supported.
    pub fn set_stream(&mut self, stream: u64) {
        let index = self.0.index();
        self.0.core.state[14] = stream as u32;
        self.0.core.state[15] = (stream >> 32) as u32;
        if index < STATE_WORDS {
            // we need to regenerate a partial result buffer
            {
                // reverse of counter adjustment in generate()
                if self.0.core.state[12] == 0 {
                    self.0.core.state[13] = self.0.core.state[13].wrapping_sub(1);
                }
                self.0.core.state[12] = self.0.core.state[12].wrapping_sub(1);
            }
            self.0.generate_and_set(index);
        }
    }
}

/// The core of `ChaChaRng`, used with `BlockRng`.
#[derive(Clone)]
pub struct ChaChaCore {
    state: [u32; STATE_WORDS],
}

// Custom Debug implementation that does not expose the internal state
impl fmt::Debug for ChaChaCore {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "ChaChaCore {{}}")
    }
}

macro_rules! quarter_round{
    ($a: expr, $b: expr, $c: expr, $d: expr) => {{
        $a = $a.wrapping_add($b); $d ^= $a; $d = $d.rotate_left(16);
        $c = $c.wrapping_add($d); $b ^= $c; $b = $b.rotate_left(12);
        $a = $a.wrapping_add($b); $d ^= $a; $d = $d.rotate_left( 8);
        $c = $c.wrapping_add($d); $b ^= $c; $b = $b.rotate_left( 7);
    }}
}

macro_rules! double_round{
    ($x: expr) => {{
        // Column round
        quarter_round!($x[ 0], $x[ 4], $x[ 8], $x[12]);
        quarter_round!($x[ 1], $x[ 5], $x[ 9], $x[13]);
        quarter_round!($x[ 2], $x[ 6], $x[10], $x[14]);
        quarter_round!($x[ 3], $x[ 7], $x[11], $x[15]);
        // Diagonal round
        quarter_round!($x[ 0], $x[ 5], $x[10], $x[15]);
        quarter_round!($x[ 1], $x[ 6], $x[11], $x[12]);
        quarter_round!($x[ 2], $x[ 7], $x[ 8], $x[13]);
        quarter_round!($x[ 3], $x[ 4], $x[ 9], $x[14]);
    }}
}

impl BlockRngCore for ChaChaCore {
    type Item = u32;
    type Results = [u32; STATE_WORDS];

    fn generate(&mut self, results: &mut Self::Results) {
        // For some reason extracting this part into a separate function
        // improves performance by 50%.
        fn core(results: &mut [u32; STATE_WORDS],
                state: &[u32; STATE_WORDS])
        {
            let mut tmp = *state;
            let rounds = 20;
            for _ in 0..rounds / 2 {
                double_round!(tmp);
            }
            for i in 0..STATE_WORDS {
                results[i] = tmp[i].wrapping_add(state[i]);
            }
        }

        core(results, &self.state);

        // update 64-bit counter
        self.state[12] = self.state[12].wrapping_add(1);
        if self.state[12] != 0 { return; };
        self.state[13] = self.state[13].wrapping_add(1);
    }
}

impl SeedableRng for ChaChaCore {
    type Seed = [u8; SEED_WORDS*4];

    fn from_seed(seed: Self::Seed) -> Self {
        let mut seed_le = [0u32; SEED_WORDS];
        le::read_u32_into(&seed, &mut seed_le);
        Self {
            state: [0x61707865, 0x3320646E, 0x79622D32, 0x6B206574, // constants
                    seed_le[0], seed_le[1], seed_le[2], seed_le[3], // seed
                    seed_le[4], seed_le[5], seed_le[6], seed_le[7], // seed
                    0, 0, 0, 0], // counter
         }
    }
}

impl CryptoRng for ChaChaCore {}

impl From<ChaChaCore> for ChaChaRng {
    fn from(core: ChaChaCore) -> Self {
        ChaChaRng(BlockRng::new(core))
    }
}

#[cfg(test)]
mod test {
    use ::rand_core::{RngCore, SeedableRng};
    use super::ChaChaRng;

    #[test]
    fn test_chacha_construction() {
        let seed = [0,0,0,0,0,0,0,0,
            1,0,0,0,0,0,0,0,
            2,0,0,0,0,0,0,0,
            3,0,0,0,0,0,0,0];
        let mut rng1 = ChaChaRng::from_seed(seed);
        assert_eq!(rng1.next_u32(), 137206642);

        let mut rng2 = ChaChaRng::from_rng(rng1).unwrap();
        assert_eq!(rng2.next_u32(), 1325750369);
    }

    #[test]
    fn test_chacha_true_values_a() {
        // Test vectors 1 and 2 from
        // https://tools.ietf.org/html/draft-nir-cfrg-chacha20-poly1305-04
        let seed = [0u8; 32];
        let mut rng = ChaChaRng::from_seed(seed);

        let mut results = [0u32; 16];
        for i in results.iter_mut() { *i = rng.next_u32(); }
        let expected = [0xade0b876, 0x903df1a0, 0xe56a5d40, 0x28bd8653,
                        0xb819d2bd, 0x1aed8da0, 0xccef36a8, 0xc70d778b,
                        0x7c5941da, 0x8d485751, 0x3fe02477, 0x374ad8b8,
                        0xf4b8436a, 0x1ca11815, 0x69b687c3, 0x8665eeb2];
        assert_eq!(results, expected);

        for i in results.iter_mut() { *i = rng.next_u32(); }
        let expected = [0xbee7079f, 0x7a385155, 0x7c97ba98, 0x0d082d73,
                        0xa0290fcb, 0x6965e348, 0x3e53c612, 0xed7aee32,
                        0x7621b729, 0x434ee69c, 0xb03371d5, 0xd539d874,
                        0x281fed31, 0x45fb0a51, 0x1f0ae1ac, 0x6f4d794b];
        assert_eq!(results, expected);
    }

    #[test]
    fn test_chacha_true_values_b() {
        // Test vector 3 from
        // https://tools.ietf.org/html/draft-nir-cfrg-chacha20-poly1305-04
        let seed = [0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 1];
        let mut rng = ChaChaRng::from_seed(seed);

        // Skip block 0
        for _ in 0..16 { rng.next_u32(); }

        let mut results = [0u32; 16];
        for i in results.iter_mut() { *i = rng.next_u32(); }
        let expected = [0x2452eb3a, 0x9249f8ec, 0x8d829d9b, 0xddd4ceb1,
                        0xe8252083, 0x60818b01, 0xf38422b8, 0x5aaa49c9,
                        0xbb00ca8e, 0xda3ba7b4, 0xc4b592d1, 0xfdf2732f,
                        0x4436274e, 0x2561b3c8, 0xebdd4aa6, 0xa0136c00];
        assert_eq!(results, expected);
    }

    #[test]
    #[cfg(all(rustc_1_26, not(target_os = "emscripten")))]
    fn test_chacha_true_values_c() {
        // Test vector 4 from
        // https://tools.ietf.org/html/draft-nir-cfrg-chacha20-poly1305-04
        let seed = [0, 0xff, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0,
                    0, 0, 0, 0, 0, 0, 0, 0];
        let expected = [0xfb4dd572, 0x4bc42ef1, 0xdf922636, 0x327f1394,
                        0xa78dea8f, 0x5e269039, 0xa1bebbc1, 0xcaf09aae,
                        0xa25ab213, 0x48a6b46c, 0x1b9d9bcb, 0x092c5be6,
                        0x546ca624, 0x1bec45d5, 0x87f47473, 0x96f0992e];
        let expected_end = 3 * 16;
        let mut results = [0u32; 16];

        // Test block 2 by skipping block 0 and 1
        let mut rng1 = ChaChaRng::from_seed(seed);
        for _ in 0..32 { rng1.next_u32(); }
        for i in results.iter_mut() { *i = rng1.next_u32(); }
        assert_eq!(results, expected);
        assert_eq!(rng1.get_word_pos(), expected_end);

        // Test block 2 by using `set_word_pos`
        let mut rng2 = ChaChaRng::from_seed(seed);
        rng2.set_word_pos(2 * 16);
        for i in results.iter_mut() { *i = rng2.next_u32(); }
        assert_eq!(results, expected);
        assert_eq!(rng2.get_word_pos(), expected_end);

        // Test skipping behaviour with other types
        let mut buf = [0u8; 32];
        rng2.fill_bytes(&mut buf[..]);
        assert_eq!(rng2.get_word_pos(), expected_end + 8);
        rng2.fill_bytes(&mut buf[0..25]);
        assert_eq!(rng2.get_word_pos(), expected_end + 15);
        rng2.next_u64();
        assert_eq!(rng2.get_word_pos(), expected_end + 17);
        rng2.next_u32();
        rng2.next_u64();
        assert_eq!(rng2.get_word_pos(), expected_end + 20);
        rng2.fill_bytes(&mut buf[0..1]);
        assert_eq!(rng2.get_word_pos(), expected_end + 21);
    }

    #[test]
    fn test_chacha_multiple_blocks() {
        let seed = [0,0,0,0, 1,0,0,0, 2,0,0,0, 3,0,0,0, 4,0,0,0, 5,0,0,0, 6,0,0,0, 7,0,0,0];
        let mut rng = ChaChaRng::from_seed(seed);

        // Store the 17*i-th 32-bit word,
        // i.e., the i-th word of the i-th 16-word block
        let mut results = [0u32; 16];
        for i in results.iter_mut() {
            *i = rng.next_u32();
            for _ in 0..16 {
                rng.next_u32();
            }
        }
        let expected = [0xf225c81a, 0x6ab1be57, 0x04d42951, 0x70858036,
                        0x49884684, 0x64efec72, 0x4be2d186, 0x3615b384,
                        0x11cfa18e, 0xd3c50049, 0x75c775f6, 0x434c6530,
                        0x2c5bad8f, 0x898881dc, 0x5f1c86d9, 0xc1f8e7f4];
        assert_eq!(results, expected);
    }

    #[test]
    fn test_chacha_true_bytes() {
        let seed = [0u8; 32];
        let mut rng = ChaChaRng::from_seed(seed);
        let mut results = [0u8; 32];
        rng.fill_bytes(&mut results);
        let expected = [118, 184, 224, 173, 160, 241, 61, 144,
                        64, 93, 106, 229, 83, 134, 189, 40,
                        189, 210, 25, 184, 160, 141, 237, 26,
                        168, 54, 239, 204, 139, 119, 13, 199];
        assert_eq!(results, expected);
    }

    #[test]
    fn test_chacha_nonce() {
        // Test vector 5 from
        // https://tools.ietf.org/html/draft-nir-cfrg-chacha20-poly1305-04
        // Although we do not support setting a nonce, we try it here anyway so
        // we can use this test vector.
        let seed = [0u8; 32];
        let mut rng = ChaChaRng::from_seed(seed);
        // 96-bit nonce in LE order is: 0,0,0,0, 0,0,0,0, 0,0,0,2
        rng.set_stream(2u64 << (24 + 32));

        let mut results = [0u32; 16];
        for i in results.iter_mut() { *i = rng.next_u32(); }
        let expected = [0x374dc6c2, 0x3736d58c, 0xb904e24a, 0xcd3f93ef,
                        0x88228b1a, 0x96a4dfb3, 0x5b76ab72, 0xc727ee54,
                        0x0e0e978a, 0xf3145c95, 0x1b748ea8, 0xf786c297,
                        0x99c28f5f, 0x628314e8, 0x398a19fa, 0x6ded1b53];
        assert_eq!(results, expected);
    }

    #[test]
    fn test_chacha_clone_streams() {
        let seed = [0,0,0,0, 1,0,0,0, 2,0,0,0, 3,0,0,0, 4,0,0,0, 5,0,0,0, 6,0,0,0, 7,0,0,0];
        let mut rng = ChaChaRng::from_seed(seed);
        let mut clone = rng.clone();
        for _ in 0..16 {
            assert_eq!(rng.next_u64(), clone.next_u64());
        }

        rng.set_stream(51);
        for _ in 0..7 {
            assert!(rng.next_u32() != clone.next_u32());
        }
        clone.set_stream(51);   // switch part way through block
        for _ in 7..16 {
            assert_eq!(rng.next_u32(), clone.next_u32());
        }
    }
}
