/*!
Teddy is a simd accelerated multiple substring matching algorithm. The name
and the core ideas in the algorithm were learned from the [Hyperscan][1_u]
project.


Background
----------

The key idea of Teddy is to do *packed* substring matching. In the literature,
packed substring matching is the idea of examing multiple bytes in a haystack
at a time to detect matches. Implementations of, for example, memchr (which
detects matches of a single byte) have been doing this for years. Only
recently, with the introduction of various SIMD instructions, has this been
extended to substring matching. The PCMPESTRI instruction (and its relatives),
for example, implements substring matching in hardware. It is, however, limited
to substrings of length 16 bytes or fewer, but this restriction is fine in a
regex engine, since we rarely care about the performance difference between
searching for a 16 byte literal and a 16 + N literal; 16 is already long
enough. The key downside of the PCMPESTRI instruction, on current (2016) CPUs
at least, is its latency and throughput. As a result, it is often faster to do
substring search with a Boyer-Moore variant and a well placed memchr to quickly
skip through the haystack.

There are fewer results from the literature on packed substring matching,
and even fewer for packed multiple substring matching. Ben-Kiki et al. [2]
describes use of PCMPESTRI for substring matching, but is mostly theoretical
and hand-waves performance. There is other theoretical work done by Bille [3]
as well.

The rest of the work in the field, as far as I'm aware, is by Faro and Kulekci
and is generally focused on multiple pattern search. Their first paper [4a]
introduces the concept of a fingerprint, which is computed for every block of
N bytes in every pattern. The haystack is then scanned N bytes at a time and
a fingerprint is computed in the same way it was computed for blocks in the
patterns. If the fingerprint corresponds to one that was found in a pattern,
then a verification step follows to confirm that one of the substrings with the
corresponding fingerprint actually matches at the current location. Various
implementation tricks are employed to make sure the fingerprint lookup is fast;
typically by truncating the fingerprint. (This may, of course, provoke more
steps in the verification process, so a balance must be struck.)

The main downside of [4a] is that the minimum substring length is 32 bytes,
presumably because of how the algorithm uses certain SIMD instructions. This
essentially makes it useless for general purpose regex matching, where a small
number of short patterns is far more likely.

Faro and Kulekci published another paper [4b] that is conceptually very similar
to [4a]. The key difference is that it uses the CRC32 instruction (introduced
as part of SSE 4.2) to compute fingerprint values. This also enables the
algorithm to work effectively on substrings as short as 7 bytes with 4 byte
windows. 7 bytes is unfortunately still too long. The window could be
technically shrunk to 2 bytes, thereby reducing minimum length to 3, but the
small window size ends up negating most performance benefits—and it's likely
the common case in a general purpose regex engine.

Faro and Kulekci also published [4c] that appears to be intended as a
replacement to using PCMPESTRI. In particular, it is specifically motivated by
the high throughput/latency time of PCMPESTRI and therefore chooses other SIMD
instructions that are faster. While this approach works for short substrings,
I personally couldn't see a way to generalize it to multiple substring search.

Faro and Kulekci have another paper [4d] that I haven't been able to read
because it is behind a paywall.


Teddy
-----

Finally, we get to Teddy. If the above literature review is complete, then it
appears that Teddy is a novel algorithm. More than that, in my experience, it
completely blows away the competition for short substrings, which is exactly
what we want in a general purpose regex engine. Again, the algorithm appears
to be developed by the authors of [Hyperscan][1_u]. Hyperscan was open sourced
late 2015, and no earlier history could be found. Therefore, tracking the exact
provenance of the algorithm with respect to the published literature seems
difficult.

DISCLAIMER: My understanding of Teddy is limited to reading auto-generated C
code, its disassembly and observing its runtime behavior.

At a high level, Teddy works somewhat similarly to the fingerprint algorithms
published by Faro and Kulekci, but Teddy does it in a way that scales a bit
better. Namely:

1. Teddy's core algorithm scans the haystack in 16 byte chunks. 16 is
   significant because it corresponds to the number of bytes in a SIMD vector.
   If one used AVX2 instructions, then we could scan the haystack in 32 byte
   chunks. Similarly, if one used AVX512 instructions, we could scan the
   haystack in 64 byte chunks. Hyperscan implements SSE + AVX2, we only
   implement SSE for the moment.
2. Bitwise operations are performed on each chunk to discover if any region of
   it matches a set of precomputed fingerprints from the patterns. If there are
   matches, then a verification step is performed. In this implementation, our
   verification step is naive. This can be improved upon.

The details to make this work are quite clever. First, we must choose how to
pick our fingerprints. In Hyperscan's implementation, I *believe* they use the
last N bytes of each substring, where N must be at least the minimum length of
any substring in the set being searched. In this implementation, we use the
first N bytes of each substring. (The tradeoffs between these choices aren't
yet clear to me.) We then must figure out how to quickly test whether an
occurrence of any fingerprint from the set of patterns appears in a 16 byte
block from the haystack. To keep things simple, let's assume N = 1 and examine
some examples to motivate the approach. Here are our patterns:

```ignore
foo
bar
baz
```

The corresponding fingerprints, for N = 1, are `f`, `b` and `b`. Now let's set
our 16 byte block to:

```ignore
bat cat foo bump
xxxxxxxxxxxxxxxx
```

To cut to the chase, Teddy works by using bitsets. In particular, Teddy creates
a mask that allows us to quickly compute membership of a fingerprint in a 16
byte block that also tells which pattern the fingerprint corresponds to. In
this case, our fingerprint is a single byte, so an appropriate abstraction is
a map from a single byte to a list of patterns that contain that fingerprint:

```ignore
f |--> foo
b |--> bar, baz
```

Now, all we need to do is figure out how to represent this map in vector space
and use normal SIMD operations to perform a lookup. The first simplification
we can make is to represent our patterns as bit fields occupying a single
byte. This is important, because a single SIMD vector can store 16 bytes.

```ignore
f |--> 00000001
b |--> 00000010, 00000100
```

How do we perform lookup though? It turns out that SSSE3 introduced a very cool
instruction called PSHUFB. The instruction takes two SIMD vectors, `A` and `B`,
and returns a third vector `C`. All vectors are treated as 16 8-bit integers.
`C` is formed by `C[i] = A[B[i]]`. (This is a bit of a simplification, but true
for the purposes of this algorithm. For full details, see [Intel's Intrinsics
Guide][5_u].) This essentially lets us use the values in `B` to lookup values
in `A`.

If we could somehow cause `B` to contain our 16 byte block from the haystack,
and if `A` could contain our bitmasks, then we'd end up with something like
this for `A`:

```ignore
    0x00 0x01 ... 0x62      ... 0x66      ... 0xFF
A = 0    0        00000110      00000001      0
```

And if `B` contains our window from our haystack, we could use shuffle to take
the values from `B` and use them to look up our bitsets in `A`. But of course,
we can't do this because `A` in the above example contains 256 bytes, which
is much larger than the size of a SIMD vector.

Nybbles to the rescue! A nybble is 4 bits. Instead of one mask to hold all of
our bitsets, we can use two masks, where one mask corresponds to the lower four
bits of our fingerprint and the other mask corresponds to the upper four bits.
So our map now looks like:

```ignore
'f' & 0xF = 0x6 |--> 00000001
'f' >> 4  = 0x6 |--> 00000111
'b' & 0xF = 0x2 |--> 00000110
'b' >> 4  = 0x6 |--> 00000111
```

Notice that the bitsets for each nybble correspond to the union of all
fingerprints that contain that nybble. For example, both `f` and `b` have the
same upper 4 bits but differ on the lower 4 bits. Putting this together, we
have `A0`, `A1` and `B`, where `A0` is our mask for the lower nybble, `A1` is
our mask for the upper nybble and `B` is our 16 byte block from the haystack:

```ignore
      0x00 0x01 0x02      0x03 ... 0x06      ... 0xF
A0 =  0    0    00000110  0        00000001      0
A1 =  0    0    0         0        00000111      0
B  =  b    a    t         _        t             p
B  =  0x62 0x61 0x74      0x20     0x74          0x70
```

But of course, we can't use `B` with `PSHUFB` yet, since its values are 8 bits,
and we need indexes that are at most 4 bits (corresponding to one of 16
values). We can apply the same transformation to split `B` into lower and upper
nybbles as we did `A`. As before, `B0` corresponds to the lower nybbles and
`B1` corresponds to the upper nybbles:

```ignore
     b   a   t   _   c   a   t   _   f   o   o   _   b   u   m   p
B0 = 0x2 0x1 0x4 0x0 0x3 0x1 0x4 0x0 0x6 0xF 0xF 0x0 0x2 0x5 0xD 0x0
B1 = 0x6 0x6 0x7 0x2 0x6 0x6 0x7 0x2 0x6 0x6 0x6 0x2 0x6 0x7 0x6 0x7
```

And now we have a nice correspondence. `B0` can index `A0` and `B1` can index
`A1`. Here's what we get when we apply `C0 = PSHUFB(A0, B0)`:

```ignore
     b         a        ... f         o         ... p
     A0[0x2]   A0[0x1]      A0[0x6]   A0[0xF]       A0[0x0]
C0 = 00000110  0            00000001  0             0
```

And `C1 = PSHUFB(A1, B1)`:

```ignore
     b         a        ... f         o        ... p
     A1[0x6]   A1[0x6]      A1[0x6]   A1[0x6]      A1[0x7]
C1 = 00000111  00000111     00000111  00000111     0
```

Notice how neither one of `C0` or `C1` is guaranteed to report fully correct
results all on its own. For example, `C1` claims that `b` is a fingerprint for
the pattern `foo` (since `A1[0x6] = 00000111`), and that `o` is a fingerprint
for all of our patterns. But if we combined `C0` and `C1` with an `AND`
operation:

```ignore
     b         a        ... f         o        ... p
C  = 00000110  0            00000001  0            0
```

Then we now have that `C[i]` contains a bitset corresponding to the matching
fingerprints in a haystack's 16 byte block, where `i` is the `ith` byte in that
block.

Once we have that, we can look for the position of the least significant bit
in `C`. That position, modulo `8`, gives us the pattern that the fingerprint
matches. That position, integer divided by `8`, also gives us the byte offset
that the fingerprint occurs in inside the 16 byte haystack block. Using those
two pieces of information, we can run a verification procedure that tries
to match all substrings containing that fingerprint at that position in the
haystack.


Implementation notes
--------------------

The problem with the algorithm as described above is that it uses a single byte
for a fingerprint. This will work well if the fingerprints are rare in the
haystack (e.g., capital letters or special characters in normal English text),
but if the fingerprints are common, you'll wind up spending too much time in
the verification step, which effectively negate the performance benefits of
scanning 16 bytes at a time. Remember, the key to the performance of this
algorithm is to do as little work as possible per 16 bytes.

This algorithm can be extrapolated in a relatively straight-forward way to use
larger fingerprints. That is, instead of a single byte prefix, we might use a
three byte prefix. The implementation below implements N = {1, 2, 3} and always
picks the largest N possible. The rationale is that the bigger the fingerprint,
the fewer verification steps we'll do. Of course, if N is too large, then we'll
end up doing too much on each step.

The way to extend it is:

1. Add a mask for each byte in the fingerprint. (Remember that each mask is
   composed of two SIMD vectors.) This results in a value of `C` for each byte
   in the fingerprint while searching.
2. When testing each 16 byte block, each value of `C` must be shifted so that
   they are aligned. Once aligned, they should all be `AND`'d together. This
   will give you only the bitsets corresponding to the full match of the
   fingerprint.

The implementation below is commented to fill in the nitty gritty details.

References
----------

- **[1]** [Hyperscan on GitHub](https://github.com/01org/hyperscan),
    [webpage](https://01.org/hyperscan)
- **[2a]** Ben-Kiki, O., Bille, P., Breslauer, D., Gasieniec, L., Grossi, R.,
    & Weimann, O. (2011).
    _Optimal packed string matching_.
    In LIPIcs-Leibniz International Proceedings in Informatics (Vol. 13).
    Schloss Dagstuhl-Leibniz-Zentrum fuer Informatik.
    DOI: 10.4230/LIPIcs.FSTTCS.2011.423.
    [PDF](http://drops.dagstuhl.de/opus/volltexte/2011/3355/pdf/37.pdf).
- **[2b]** Ben-Kiki, O., Bille, P., Breslauer, D., Ga̧sieniec, L., Grossi, R.,
    & Weimann, O. (2014).
    _Towards optimal packed string matching_.
    Theoretical Computer Science, 525, 111-129.
    DOI: 10.1016/j.tcs.2013.06.013.
    [PDF](http://www.cs.haifa.ac.il/~oren/Publications/bpsm.pdf).
- **[3]** Bille, P. (2011).
    _Fast searching in packed strings_.
    Journal of Discrete Algorithms, 9(1), 49-56.
    DOI: 10.1016/j.jda.2010.09.003.
    [PDF](http://www.sciencedirect.com/science/article/pii/S1570866710000353).
- **[4a]** Faro, S., & Külekci, M. O. (2012, October).
    _Fast multiple string matching using streaming SIMD extensions technology_.
    In String Processing and Information Retrieval (pp. 217-228).
    Springer Berlin Heidelberg.
    DOI: 10.1007/978-3-642-34109-0_23.
    [PDF](http://www.dmi.unict.it/~faro/papers/conference/faro32.pdf).
- **[4b]** Faro, S., & Külekci, M. O. (2013, September).
    _Towards a Very Fast Multiple String Matching Algorithm for Short Patterns_.
    In Stringology (pp. 78-91).
    [PDF](http://www.dmi.unict.it/~faro/papers/conference/faro36.pdf).
- **[4c]** Faro, S., & Külekci, M. O. (2013, January).
    _Fast packed string matching for short patterns_.
    In Proceedings of the Meeting on Algorithm Engineering & Expermiments
    (pp. 113-121).
    Society for Industrial and Applied Mathematics.
    [PDF](http://arxiv.org/pdf/1209.6449.pdf).
- **[4d]** Faro, S., & Külekci, M. O. (2014).
    _Fast and flexible packed string matching_.
    Journal of Discrete Algorithms, 28, 61-72.
    DOI: 10.1016/j.jda.2014.07.003.

[1_u]: https://github.com/01org/hyperscan
[5_u]: https://software.intel.com/sites/landingpage/IntrinsicsGuide
*/

use std::cmp;

use aho_corasick::{Automaton, AcAutomaton, FullAcAutomaton};
use syntax::hir::literal::Literals;

use vector::ssse3::{SSSE3VectorBuilder, u8x16};

/// Corresponds to the number of bytes read at a time in the haystack.
const BLOCK_SIZE: usize = 16;

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
    /// A builder for SSSE3 empowered vectors.
    vb: SSSE3VectorBuilder,
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
        SSSE3VectorBuilder::new().is_some()
    }

    /// Create a new `Teddy` multi substring matcher.
    ///
    /// If a `Teddy` matcher could not be created (e.g., `pats` is empty or has
    /// an empty substring), then `None` is returned.
    pub fn new(pats: &Literals) -> Option<Teddy> {
        let vb = match SSSE3VectorBuilder::new() {
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
        // is if SSSE3 is available.
        unsafe { self.find_impl(haystack) }
    }

    #[allow(unused_attributes)]
    #[target_feature(enable = "ssse3")]
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
        let zero = self.vb.u8x16_splat(0);
        let len = haystack.len();
        debug_assert!(len >= BLOCK_SIZE);
        while pos <= len - BLOCK_SIZE {
            let h = unsafe {
                // I tried and failed to eliminate bounds checks in safe code.
                // This is safe because of our loop invariant: pos is always
                // <= len-16.
                let p = haystack.get_unchecked(pos..);
                self.vb.u8x16_load_unchecked_unaligned(p)
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
        let zero = self.vb.u8x16_splat(0);
        let len = haystack.len();
        // The previous value of `C` (from the module documentation) for the
        // *first* byte in the fingerprint. On subsequent iterations, we take
        // the last bitset from the previous `C` and insert it into the first
        // position of the current `C`, shifting all other bitsets to the right
        // one lane. This causes `C` for the first byte to line up with `C` for
        // the second byte, so that they can be `AND`'d together.
        let mut prev0 = self.vb.u8x16_splat(0xFF);
        let mut pos = 1;
        debug_assert!(len >= BLOCK_SIZE);
        while pos <= len - BLOCK_SIZE {
            let h = unsafe {
                // I tried and failed to eliminate bounds checks in safe code.
                // This is safe because of our loop invariant: pos is always
                // <= len-16.
                let p = haystack.get_unchecked(pos..);
                self.vb.u8x16_load_unchecked_unaligned(p)
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
        let zero = self.vb.u8x16_splat(0);
        let len = haystack.len();
        let mut prev0 = self.vb.u8x16_splat(0xFF);
        let mut prev1 = self.vb.u8x16_splat(0xFF);
        let mut pos = 2;
        while pos <= len - BLOCK_SIZE {
            let h = unsafe {
                // I tried and failed to eliminate bounds checks in safe code.
                // This is safe because of our loop invariant: pos is always
                // <= len-16.
                let p = haystack.get_unchecked(pos..);
                self.vb.u8x16_load_unchecked_unaligned(p)
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
        res: u8x16,
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
    vb: SSSE3VectorBuilder,
    masks: [Mask; 3],
    size: usize,
}

impl Masks {
    /// Create a new set of masks of size `n`, where `n` corresponds to the
    /// number of bytes in a fingerprint.
    fn new(vb: SSSE3VectorBuilder, n: usize) -> Masks {
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
    fn members1(&self, haystack_block: u8x16) -> u8x16 {
        let masklo = self.vb.u8x16_splat(0xF);
        let hlo = haystack_block.and(masklo);
        let hhi = haystack_block.bit_shift_right_4().and(masklo);

        self.masks[0].lo.shuffle(hlo).and(self.masks[0].hi.shuffle(hhi))
    }

    /// Like members1, but computes C for the first and second bytes in the
    /// fingerprint.
    #[inline(always)]
    fn members2(&self, haystack_block: u8x16) -> (u8x16, u8x16) {
        let masklo = self.vb.u8x16_splat(0xF);
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
    fn members3(&self, haystack_block: u8x16) -> (u8x16, u8x16, u8x16) {
        let masklo = self.vb.u8x16_splat(0xF);
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
    lo: u8x16,
    /// Bitsets for the high nybbles in a fingerprint.
    hi: u8x16,
}

impl Mask {
    /// Create a new mask with no members.
    fn new(vb: SSSE3VectorBuilder) -> Mask {
        Mask {
            lo: vb.u8x16_splat(0),
            hi: vb.u8x16_splat(0),
        }
    }

    /// Adds the given byte to the given bucket.
    fn add(&mut self, bucket: u8, byte: u8) {
        // Split our byte into two nybbles, and add each nybble to our
        // mask.
        let byte_lo = (byte & 0xF) as usize;
        let byte_hi = (byte >> 4) as usize;

        let lo = self.lo.extract(byte_lo);
        self.lo.replace(byte_lo, ((1 << bucket) as u8) | lo);

        let hi = self.hi.extract(byte_hi);
        self.hi.replace(byte_hi, ((1 << bucket) as u8) | hi);
    }
}
