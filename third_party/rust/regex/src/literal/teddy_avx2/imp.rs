/*!
This is the Teddy searcher, but ported to AVX2.

See the module comments in the SSSE3 Teddy searcher for a more in depth
explanation of how this algorithm works. For the most part, this port is
basically the same as the SSSE3 version, but using 256-bit vectors instead of
128-bit vectors, which increases throughput.
*/

use std::cmp;

use aho_corasick::{Automaton, AcAutomaton, FullAcAutomaton};
use syntax::hir::literal::Literals;

use vector::avx2::{AVX2VectorBuilder, u8x32};

/// Corresponds to the number of bytes read at a time in the haystack.
const BLOCK_SIZE: usize = 32;

/// Match reports match information.
#[derive(Debug, Clone)]
pub struct Match {
    /// The index of the pattern that matched. The index is in correspondence
    /// with the order of the patterns given at construction.
    pub pat: usize,
    /// The start byte offset of the match.
    pub start: usize,
    /// The end byte offset of the match. This is always `start + pat.len()`.
    pub end: usize,
}

/// A SIMD accelerated multi substring searcher.
#[derive(Debug, Clone)]
pub struct Teddy {
    /// A builder for AVX2 empowered vectors.
    vb: AVX2VectorBuilder,
    /// A list of substrings to match.
    pats: Vec<Vec<u8>>,
    /// An Aho-Corasick automaton of the patterns. We use this when we need to
    /// search pieces smaller than the Teddy block size.
    ac: FullAcAutomaton<Vec<u8>>,
    /// A set of 8 buckets. Each bucket corresponds to a single member of a
    /// bitset. A bucket contains zero or more substrings. This is useful
    /// when the number of substrings exceeds 8, since our bitsets cannot have
    /// more than 8 members.
    buckets: Vec<Vec<usize>>,
    /// Our set of masks. There's one mask for each byte in the fingerprint.
    masks: Masks,
}

impl Teddy {
    /// Returns true if and only if Teddy is supported on this platform.
    ///
    /// If this returns `false`, then `Teddy::new(...)` is guaranteed to
    /// return `None`.
    pub fn available() -> bool {
        AVX2VectorBuilder::new().is_some()
    }

    /// Create a new `Teddy` multi substring matcher.
    ///
    /// If a `Teddy` matcher could not be created (e.g., `pats` is empty or has
    /// an empty substring), then `None` is returned.
    pub fn new(pats: &Literals) -> Option<Teddy> {
        let vb = match AVX2VectorBuilder::new() {
            None => return None,
            Some(vb) => vb,
        };
        if !Teddy::available() {
            return None;
        }

        let pats: Vec<_> = pats.literals().iter().map(|p|p.to_vec()).collect();
        let min_len = pats.iter().map(|p| p.len()).min().unwrap_or(0);
        // Don't allow any empty patterns and require that we have at
        // least one pattern.
        if min_len < 1 {
            return None;
        }
        // Pick the largest mask possible, but no larger than 3.
        let nmasks = cmp::min(3, min_len);
        let mut masks = Masks::new(vb, nmasks);
        let mut buckets = vec![vec![]; 8];
        // Assign a substring to each bucket, and add the bucket's bitfield to
        // the appropriate position in the mask.
        for (pati, pat) in pats.iter().enumerate() {
            let bucket = pati % 8;
            buckets[bucket].push(pati);
            masks.add(bucket as u8, pat);
        }
        Some(Teddy {
            vb: vb,
            pats: pats.to_vec(),
            ac: AcAutomaton::new(pats.to_vec()).into_full(),
            buckets: buckets,
            masks: masks,
        })
    }

    /// Returns all of the substrings matched by this `Teddy`.
    pub fn patterns(&self) -> &[Vec<u8>] {
        &self.pats
    }

    /// Returns the number of substrings in this matcher.
    pub fn len(&self) -> usize {
        self.pats.len()
    }

    /// Returns the approximate size on the heap used by this matcher.
    pub fn approximate_size(&self) -> usize {
        self.pats.iter().fold(0, |a, b| a + b.len())
    }

    /// Searches `haystack` for the substrings in this `Teddy`. If a match was
    /// found, then it is returned. Otherwise, `None` is returned.
    pub fn find(&self, haystack: &[u8]) -> Option<Match> {
        // This is safe because the only way we can construct a Teddy type
        // is if AVX2 is available.
        unsafe { self.find_impl(haystack) }
    }

    #[allow(unused_attributes)]
    #[target_feature(enable = "avx2")]
    unsafe fn find_impl(&self, haystack: &[u8]) -> Option<Match> {
        // If our haystack is smaller than the block size, then fall back to
        // a naive brute force search.
        if haystack.is_empty() || haystack.len() < (BLOCK_SIZE + 2) {
            return self.slow(haystack, 0);
        }
        match self.masks.len() {
            0 => None,
            1 => self.find1(haystack),
            2 => self.find2(haystack),
            3 => self.find3(haystack),
            _ => unreachable!(),
        }
    }

    /// `find1` is used when there is only 1 mask. This is the easy case and is
    /// pretty much as described in the module documentation.
    #[inline(always)]
    fn find1(&self, haystack: &[u8]) -> Option<Match> {
        let mut pos = 0;
        let zero = self.vb.u8x32_splat(0);
        let len = haystack.len();
        debug_assert!(len >= BLOCK_SIZE);
        while pos <= len - BLOCK_SIZE {
            let h = unsafe {
                // I tried and failed to eliminate bounds checks in safe code.
                // This is safe because of our loop invariant: pos is always
                // <= len-32.
                let p = haystack.get_unchecked(pos..);
                self.vb.u8x32_load_unchecked_unaligned(p)
            };
            // N.B. `res0` is our `C` in the module documentation.
            let res0 = self.masks.members1(h);
            // Only do expensive verification if there are any non-zero bits.
            let bitfield = res0.ne(zero).movemask();
            if bitfield != 0 {
                if let Some(m) = self.verify(haystack, pos, res0, bitfield) {
                    return Some(m);
                }
            }
            pos += BLOCK_SIZE;
        }
        self.slow(haystack, pos)
    }

    /// `find2` is used when there are 2 masks, e.g., the fingerprint is 2 bytes
    /// long.
    #[inline(always)]
    fn find2(&self, haystack: &[u8]) -> Option<Match> {
        // This is an exotic way to right shift a SIMD vector across lanes.
        // See below at use for more details.
        let zero = self.vb.u8x32_splat(0);
        let len = haystack.len();
        // The previous value of `C` (from the module documentation) for the
        // *first* byte in the fingerprint. On subsequent iterations, we take
        // the last bitset from the previous `C` and insert it into the first
        // position of the current `C`, shifting all other bitsets to the right
        // one lane. This causes `C` for the first byte to line up with `C` for
        // the second byte, so that they can be `AND`'d together.
        let mut prev0 = self.vb.u8x32_splat(0xFF);
        let mut pos = 1;
        debug_assert!(len >= BLOCK_SIZE);
        while pos <= len - BLOCK_SIZE {
            let h = unsafe {
                // I tried and failed to eliminate bounds checks in safe code.
                // This is safe because of our loop invariant: pos is always
                // <= len-32.
                let p = haystack.get_unchecked(pos..);
                self.vb.u8x32_load_unchecked_unaligned(p)
            };
            let (res0, res1) = self.masks.members2(h);

            // Do this:
            //
            //     (prev0 << 15) | (res0 >> 1)
            //
            // This lets us line up our C values for each byte.
            let res0prev0 = res0.alignr_15(prev0);

            // `AND`'s our `C` values together.
            let res = res0prev0.and(res1);
            prev0 = res0;

            let bitfield = res.ne(zero).movemask();
            if bitfield != 0 {
                let pos = pos.checked_sub(1).unwrap();
                if let Some(m) = self.verify(haystack, pos, res, bitfield) {
                    return Some(m);
                }
            }
            pos += BLOCK_SIZE;
        }
        // The windowing above doesn't check the last byte in the last
        // window, so start the slow search at the last byte of the last
        // window.
        self.slow(haystack, pos.checked_sub(1).unwrap())
    }

    /// `find3` is used when there are 3 masks, e.g., the fingerprint is 3 bytes
    /// long.
    ///
    /// N.B. This is a straight-forward extrapolation of `find2`. The only
    /// difference is that we need to keep track of two previous values of `C`,
    /// since we now need to align for three bytes.
    #[inline(always)]
    fn find3(&self, haystack: &[u8]) -> Option<Match> {
        let zero = self.vb.u8x32_splat(0);
        let len = haystack.len();
        let mut prev0 = self.vb.u8x32_splat(0xFF);
        let mut prev1 = self.vb.u8x32_splat(0xFF);
        let mut pos = 2;

        while pos <= len - BLOCK_SIZE {
            let h = unsafe {
                // I tried and failed to eliminate bounds checks in safe code.
                // This is safe because of our loop invariant: pos is always
                // <= len-32.
                let p = haystack.get_unchecked(pos..);
                self.vb.u8x32_load_unchecked_unaligned(p)
            };
            let (res0, res1, res2) = self.masks.members3(h);

            let res0prev0 = res0.alignr_14(prev0);
            let res1prev1 = res1.alignr_15(prev1);
            let res = res0prev0.and(res1prev1).and(res2);

            prev0 = res0;
            prev1 = res1;

            let bitfield = res.ne(zero).movemask();
            if bitfield != 0 {
                let pos = pos.checked_sub(2).unwrap();
                if let Some(m) = self.verify(haystack, pos, res, bitfield) {
                    return Some(m);
                }
            }
            pos += BLOCK_SIZE;
        }
        // The windowing above doesn't check the last two bytes in the last
        // window, so start the slow search at the penultimate byte of the
        // last window.
        // self.slow(haystack, pos.saturating_sub(2))
        self.slow(haystack, pos.checked_sub(2).unwrap())
    }

    /// Runs the verification procedure on `res` (i.e., `C` from the module
    /// documentation), where the haystack block starts at `pos` in
    /// `haystack`. `bitfield` has ones in the bit positions that `res` has
    /// non-zero bytes.
    ///
    /// If a match exists, it returns the first one.
    #[inline(always)]
    fn verify(
        &self,
        haystack: &[u8],
        pos: usize,
        res: u8x32,
        mut bitfield: u32,
    ) -> Option<Match> {
        while bitfield != 0 {
            // The next offset, relative to pos, where some fingerprint
            // matched.
            let byte_pos = bitfield.trailing_zeros() as usize;
            bitfield &= !(1 << byte_pos);

            // Offset relative to the beginning of the haystack.
            let start = pos + byte_pos;

            // The bitfield telling us which patterns had fingerprints that
            // match at this starting position.
            let mut patterns = res.extract(byte_pos);
            while patterns != 0 {
                let bucket = patterns.trailing_zeros() as usize;
                patterns &= !(1 << bucket);

                // Actual substring search verification.
                if let Some(m) = self.verify_bucket(haystack, bucket, start) {
                    return Some(m);
                }
            }
        }

        None
    }

    /// Verifies whether any substring in the given bucket matches in haystack
    /// at the given starting position.
    #[inline(always)]
    fn verify_bucket(
        &self,
        haystack: &[u8],
        bucket: usize,
        start: usize,
    ) -> Option<Match> {
        // This cycles through the patterns in the bucket in the order that
        // the patterns were given. Therefore, we guarantee leftmost-first
        // semantics.
        for &pati in &self.buckets[bucket] {
            let pat = &*self.pats[pati];
            if start + pat.len() > haystack.len() {
                continue;
            }
            if pat == &haystack[start..start + pat.len()] {
                return Some(Match {
                    pat: pati,
                    start: start,
                    end: start + pat.len(),
                });
            }
        }
        None
    }

    /// Slow substring search through all patterns in this matcher.
    ///
    /// This is used when we don't have enough bytes in the haystack for our
    /// block based approach.
    #[inline(never)]
    fn slow(&self, haystack: &[u8], pos: usize) -> Option<Match> {
        self.ac.find(&haystack[pos..]).next().map(|m| {
            Match {
                pat: m.pati,
                start: pos + m.start,
                end: pos + m.end,
            }
        })
    }
}

/// A list of masks. This has length equal to the length of the fingerprint.
/// The length of the fingerprint is always `min(3, len(smallest_substring))`.
#[derive(Debug, Clone)]
struct Masks {
    vb: AVX2VectorBuilder,
    masks: [Mask; 3],
    size: usize,
}

impl Masks {
    /// Create a new set of masks of size `n`, where `n` corresponds to the
    /// number of bytes in a fingerprint.
    fn new(vb: AVX2VectorBuilder, n: usize) -> Masks {
        Masks {
            vb: vb,
            masks: [Mask::new(vb), Mask::new(vb), Mask::new(vb)],
            size: n,
        }
    }

    /// Returns the number of masks.
    fn len(&self) -> usize {
        self.size
    }

    /// Adds the given pattern to the given bucket. The bucket should be a
    /// power of `2 <= 2^7`.
    fn add(&mut self, bucket: u8, pat: &[u8]) {
        for i in 0..self.len() {
            self.masks[i].add(bucket, pat[i]);
        }
    }

    /// Finds the fingerprints that are in the given haystack block. i.e., this
    /// returns `C` as described in the module documentation.
    ///
    /// More specifically, `for i in 0..16` and `j in 0..8, C[i][j] == 1` if and
    /// only if `haystack_block[i]` corresponds to a fingerprint that is part
    /// of a pattern in bucket `j`.
    #[inline(always)]
    fn members1(&self, haystack_block: u8x32) -> u8x32 {
        let masklo = self.vb.u8x32_splat(0xF);
        let hlo = haystack_block.and(masklo);
        let hhi = haystack_block.bit_shift_right_4().and(masklo);

        self.masks[0].lo.shuffle(hlo).and(self.masks[0].hi.shuffle(hhi))
    }

    /// Like members1, but computes C for the first and second bytes in the
    /// fingerprint.
    #[inline(always)]
    fn members2(&self, haystack_block: u8x32) -> (u8x32, u8x32) {
        let masklo = self.vb.u8x32_splat(0xF);
        let hlo = haystack_block.and(masklo);
        let hhi = haystack_block.bit_shift_right_4().and(masklo);

        let res0 =
            self.masks[0].lo.shuffle(hlo).and(self.masks[0].hi.shuffle(hhi));
        let res1 =
            self.masks[1].lo.shuffle(hlo).and(self.masks[1].hi.shuffle(hhi));
        (res0, res1)
    }

    /// Like `members1`, but computes `C` for the first, second and third bytes
    /// in the fingerprint.
    #[inline(always)]
    fn members3(&self, haystack_block: u8x32) -> (u8x32, u8x32, u8x32) {
        let masklo = self.vb.u8x32_splat(0xF);
        let hlo = haystack_block.and(masklo);
        let hhi = haystack_block.bit_shift_right_4().and(masklo);

        let res0 =
            self.masks[0].lo.shuffle(hlo).and(self.masks[0].hi.shuffle(hhi));
        let res1 =
            self.masks[1].lo.shuffle(hlo).and(self.masks[1].hi.shuffle(hhi));
        let res2 =
            self.masks[2].lo.shuffle(hlo).and(self.masks[2].hi.shuffle(hhi));
        (res0, res1, res2)
    }
}

/// A single mask.
#[derive(Debug, Clone, Copy)]
struct Mask {
    /// Bitsets for the low nybbles in a fingerprint.
    lo: u8x32,
    /// Bitsets for the high nybbles in a fingerprint.
    hi: u8x32,
}

impl Mask {
    /// Create a new mask with no members.
    fn new(vb: AVX2VectorBuilder) -> Mask {
        Mask {
            lo: vb.u8x32_splat(0),
            hi: vb.u8x32_splat(0),
        }
    }

    /// Adds the given byte to the given bucket.
    fn add(&mut self, bucket: u8, byte: u8) {
        // Split our byte into two nybbles, and add each nybble to our
        // mask.
        let byte_lo = (byte & 0xF) as usize;
        let byte_hi = (byte >> 4) as usize;

        let lo = self.lo.extract(byte_lo) | ((1 << bucket) as u8);
        self.lo.replace(byte_lo, lo);
        self.lo.replace(byte_lo + 16, lo);

        let hi = self.hi.extract(byte_hi) | ((1 << bucket) as u8);
        self.hi.replace(byte_hi, hi);
        self.hi.replace(byte_hi + 16, hi);
    }
}
