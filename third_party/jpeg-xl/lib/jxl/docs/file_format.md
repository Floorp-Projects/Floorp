# File format overview

[TOC]

<!--*
# Document freshness: For more information, see go/fresh-source.
freshness: { owner: 'user' reviewed: '2018-12-17' }

!!!!!!WARNING: copied into working draft, update that instead!!!!!!
*-->

This document describes high level PIK format and metadata.

## FileHeader

Topmost structure that contains most general image related metadata and a file
signature used to distinguish PIK files.

Structure:

-   "signature" 32 bits; must be equal to little-endian representation of
    \\(\texttt{0x0A4CD74A}\\) value; \\(\texttt{0x0A}\\) causes files opened in
    text mode to be rejected, and \\(\texttt{0xD7}\\) detects 7-bit transfers
-   "xsize_minus_1" [field](entropy_coding_basic.md#uint32-field) /
    L = [9 bits, 11 bits, 13 bits, 32 bits]; 13-bit values support most existing
    cameras (up to 8K x 8K images), 32-bit values cover provide support for up
    to 4G x 4G images; "xsize" is derived from this field by adding \\(1\\),
    thus zero-width images are not supported
-   "ysize_minus_1" [field](entropy_coding_basic.md#uint32-field) /
    L = [9 bits, 11 bits, 13 bits, 32 bits]; similar to "xsize_minus_1"
-   orientation indicates 8 possible orientations, as defined in EXIF.
-   nested ["metadata"](#metadata) structure
-   nested ["preview"](#preview) structure
-   nested ["animation"](#animation) structure
-   ["extensions"](#extensions) stub that allows future format extension

## Metadata

[Optional structure](#optional-structures) that contains meta-information about
image and other supportive information.

Structure:
-   "all_default" 1 bit; see ["optional structures"](#optional-structures)
-   nested ["transcoded"](#transcoded) structure
-   "target_nits_div50" [field](entropy_coding_basic.md#uint32-field) /
    L = [direct 2, direct 5, direct 80, 8 bits];
    most common values (100, 250, 4000) are encoded directly; maximal
    expressible intensity is 12750
-   "exif" compressed [byte array](entropy_coding_basic.md#byte-array-field);
    original image metadata
-   "iptc" compressed [byte array](entropy_coding_basic.md#byte-array-field);
    original image metadata
-   "xmp" compressed [byte array](entropy_coding_basic.md#byte-array-field);
    original image metadata

## Transcoded

[Optional structure](#optional-structures) that contains information original
image.

Structure:

-   "all_default" 1 bit; see ["optional structures"](#optional-structures)
-   "original_bit_depth" [field](entropy_coding_basic.md#uint32-field) /
    L = [direct 8, direct 16, direct 32, 5 bits]
-   "original_color_encoding" nested [ColorEncoding](#colorencoding) structure
-   "original_bytes_per_alpha" [field](entropy_coding_basic.md#uint32-field) /
    L = [direct 0, direct 1, direct 2, direct 4]


## Preview

[Optional structure](#optional-structures) that contains information about
"discardable preview" image. Unlike early "progressive" passes, this image is
completely independent and optimized for better "preview" experience, i.e.
appropriately preprocessed and non-power-of-two scaled.

Preview images have different size constraints than main image and currently
limited to 8K x 8K; size limit is set to be 128MiB.

Structure:

-   "all_default" 1 bit; see ["optional structures"](#optional-structures)
-   "size_bits" [field](entropy_coding_basic.md#uint32-field) /
    L = [12 bits, 16 bits, 20 bits, 28 bits]
-   "xsize" [field](entropy_coding_basic.md#uint32-field) /
    L = [7 bits, 9 bits, 11 bits, 13 bits]
-   "ysize" [field](entropy_coding_basic.md#uint32-field) /
    L = [7 bits, 9 bits, 11 bits, 13 bits]

## Animation

[Optional structure](#optional-structures) that contains meta-information about
animation (image sequence).

Structure:

-   "all_default" 1 bit; see ["optional structures"](#optional-structures)
-   "num_loops" [field](entropy_coding_basic.md#uint32-field) /
    L = [direct 0, 3 bits, 16 bits, 32 bits]; \\(0\\) means to repeat infinitely
-   "ticks_numerator" [field](entropy_coding_basic.md#uint32-field) /
    L = [direct 1, 9 bits, 20 bits, 32 bits]
-   "ticks_denominator" [field](entropy_coding_basic.md#uint32-field) /
    L = [direct 1, 9 bits, 20 bits, 32 bits]

## Extensions

This "structure" is usually put at the end of other structures which would be
extended in future. It allows earlier versions of decoders to skip the newer
fields.

Structure:

-   "extensions" [field](entropy_coding_basic.md#uint64-field)
-   if "extensions" is \\(0\\), then no extra information follows
-   otherwise:
    -   "extension_bits" [field](entropy_coding_basic.md#uint64-field) - number
        of bits to be skipped by decoder that does not expect extensions here

## Optional structures

Some structures are "optional". In case all the field values are equal to their
defaults, encoder is eligible to represent the structure with a single bit.

Structures that contain non-empty [extension](#extensions) tail are ineligible
for 1-bit encoding.

Technically, this bit is represented as "all_default" field that comes first; if
the value of this field is \\(1\\), then the rest of structure is not decoded.
