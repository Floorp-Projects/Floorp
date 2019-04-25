# Advisory about potential memory unsafety issues

[While reviewing][i885] some `unsafe Vec::from_raw_parts` operations within the
library, trying to justify their existence with stronger reasoning, we noticed
that they instead did not meet the required conditions set by the standard
library. This unsoundness was quickly removed, but we noted that the same
unjustified reasoning had been applied by a dependency introduced in `0.21`.

For efficiency reasons, we had tried to reuse the allocations made by decoders
for the buffer of the final image. However, that process is error prone. Most
image decoding algorithms change the representation type of color samples to
some degree. Notably, the output pixel type may have a different size and
alignment than the type used in the temporary decoding buffer. In this specific
instance, the `ImageBuffer` of the output expects a linear arrangement of `u8`
samples while the implementation of the `hdr` decoder uses a pixel
representation of `Rgb<u8>`, which has three times the size. One of the
requirements of `Vec::from_raw_parts` reads:

> ptr's T needs to have the same size and alignment as it was allocated with.

This requirement is not present on slices `[T]`, as it is motivated by the
allocator interface. The validity invariant of a reference and slice only
requires the correct alignment here, which was considered in the design of
`Rgb<_>` by giving it a well-defined representation, `#[repr(C)]`. But
critically, this does not guarantee that we can reuse the existing allocation
through effectively transmuting a `Vec<_>`!

The actual impact of this issue is, in real world implementations, limited to
allocators which handle allocations for types of size `1` and `3`/`4`
differently. To the best of my knowledge, this does not apply to `jemalloc` and
the `libc` allocator. However, we decided to proceed with caution here.

## Lessons for the future

New library dependencies will be under a stricter policy. Not only would they
need to be justified by functionality but also require at least some level of
reasoning how they solve that problem better than alternatives. Some appearance
of maintenance, or the existence of `#[deny(unsafe)]`, will help. We'll
additionally look into existing dependencies trying to identify similar issues
and minimizing the potential surface for implementation risks.

## Sound and safe buffer reuse

It seems that the `Vec` representation is entirely unfit for buffer reuse in
the style which an image library requires. In particular, using pixel types of
different sizes is likely common to handle either whole (encoded) pixels or
individual samples. Thus, we started a new sub-project to address this use
case, [image-canvas][image-canvas]. Contributions and review of its safety are
very welcome, we ask for the communities help here. The release of `v0.1` will
not occur until at least one such review has occurred.


[i885]: https://github.com/image-rs/image/pull/885
[image-canvas]: https://github.com/image-rs/canvas
