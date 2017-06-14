This document contains notes about various ideas that for one reason or another
are not being actively pursued.

## Next byte is non-ASCII after ASCII optimization

The current plan for a SIMD-accelerated inner loop for handling ASCII bytes
makes no use of the bit of information that if the buffers didn't end but the
ASCII loop exited, the next byte will not be an ASCII byte.

## The structure of handles.rs and bound checks

handles.rs is designed to make it possible to avoid bound checks when writing
to the slices. While it would be possible to omit the bound checks manually,
it probably makes more sense to carry out an investigation to make sure that
the compiler performs the omission. If not, it makes more sense to file a bug
on the compiler than to omit the checks manually.

## Handling ASCII with table lookups when decoding single-byte to UTF-16

Both uconv and ICU outperform encoding_rs when decoding single-byte to UTF-16.
unconv doesn't even do anything fancy to manually unroll the loop (see below).
Both handle even the ASCII range using table lookup. That is, there's no branch
for checking if we're in the lower or upper half of the encoding.

However, adding SIMD acceleration for the ASCII half will likely be a bigger
win than eliminating the branch to decide ASCII vs. non-ASCII.

## Manual loop unrolling for single-byte encodings

ICU currently outperforms encoding_rs (by over x2!) when decoding a single-byte
encoding to UTF-16. This appears to be thanks to manually unrolling the
conversion loop by 16. See [ucnv_MBCSSingleToBMPWithOffsets][1].

[1]: https://ssl.icu-project.org/repos/icu/icu/tags/release-55-1/source/common/ucnvmbcs.cpp

Notably, none of the single-byte encodings have bytes that'd decode to the
upper half of BMP. Therefore, if the unmappable marker has the highest bit set
instead of being zero, the check for unmappables within a 16-character stride
can be done either by ORing the BMP characters in the stride together and
checking the high bit or by loading the upper halves of the BMP charaters
in a `u8x8` register and checking the high bits using the `_mm_movemask_epi8`
/ `pmovmskb` SSE2 instruction.

## After non-ASCII, handle ASCII punctuation without SIMD

Since the failure mode of SIMD ASCII acceleration involves wasted aligment
checks and a wasted SIMD read when the next code unit is non-ASCII and non-Latin
scripts have runs of non-ASCII even if ASCII spaces and punctuation is used,
consider handling the next two or three bytes following non-ASCII as non-SIMD
before looping back to the SIMD mode. Maybe move back to SIMD ASCII faster if
there's ASCII that's not space or punctuation. Maybe with the "space or
punctuation" check in place, this code can be allowed to be in place even for
UTF-8 and Latin single-byte (i.e. not having different code for Latin and
non-Latin single-byte).

## Prefer maintaining aligment

Instead of returning to acceleration directly after non-ASCII, consider
continuing to the alignment boundary without acceleration.

## Read from SIMD lanes instead of RAM (cache) when ASCII check fails

When the SIMD ASCII check fails, the data has already been read from memory.
Test whether it's faster to read the data by lane from the SIMD register than
to read it again from RAM (cache).

## Use Level 2 Hanzi and Level 2 Kanji ordering

These two are ordered by radical and then by stroke count, so in principle,
they should be mostly Unicode-ordered, although at least Level 2 Hanzi isn't
fully Unicode-ordered. Is "mostly" good enough for encode accelelation?

## Create a `divmod_94()` function

Experiment with a function that computes `(i / 94, i % 94)` more efficiently
than generic code.

