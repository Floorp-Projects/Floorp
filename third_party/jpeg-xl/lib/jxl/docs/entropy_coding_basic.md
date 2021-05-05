# Basic entropy encoders

[TOC]

<!--*
# Document freshness: For more information, see go/fresh-source.
freshness: { owner: 'user' reviewed: '2018-12-13' }
*-->

This document describes low level encodings used in JXL.

## Uint32 field

Field is a variable length encoding for values with predefined distribution.

Distribution is described with \\(\text{distribution}\\) value.

If \\(\text{distribution} > \texttt{0xFFFFFFDF} = 2^{32} - 32\\) then value is
always encoded as \\(\text{distribution} - \texttt{0xFFFFFFDF}\\) bits.
This mode is called "raw" encoding. Raw encoding is typically used for
edge cases, e.g. when exactly 1 or 32 bit value needs to be encoded.

In "regular" (non-raw) mode \\(\text{distribution}\\) is interpreted as array
\\(\text{L}\\) containing 4 uint8 values.

To decode regular field, first 2 bits are read;
those bits represent "selector" value \\(\text{S}\\).

Depending on mode \\(\text{M} = \text{L}[\text{S}]\\):

-   if \\(\text{M} \ge \texttt{0x80}\\),
    then \\(\text{M} - \texttt{0x80}\\) is the "direct" encoded value
-   else if \\(\text{M} \ge \texttt{0x40}\\),
    let \\(\text{V}\\) be the \\((\text{M} \& \texttt{0x7}) + 1\\)
    following bits, "shifted" encoded value is
    \\(\text{V} + ((\text{M} \gg 3) \& \texttt{0x7}) + 1\\)
-   otherwise \\(\text{M}\\) following bits represent the encoded value

Source code: cs/jxl/fields.h

## Uint64 field

This field supports bigger values than [Uint32](#uint32-field), but has single
fixed distribution.

Value is decoded as following:

-   "selector" \\(\text{S}\\) 2 bits
-   if \\(\text{S} = 0\\) then value is 0
-   if \\(\text{S} = 1\\) then next 4 bits represent \\(\text{value} - 1\\)
-   if \\(\text{S} = 2\\) then next 8 bits represent \\(\text{value} - 17\\)
-   if \\(\text{S} = 3\\) then:
    -   12 bits represent the lowest 12 bits of value
    -   while next bit is 1:
        -   if less than 60 value bits are already read,
            then 8 bits represent higher 8 bits of value
        -   otherwise 4 bits represent the highest 4 bits of value and 'while'
            loop is finished

Source code: cs/jxl/fields.h

## Byte array field

Byte array field holds (optionally compressed) array of bytes. Byte array is
encoded as:

-   \\(\text{type}\\) [field](#uint32-field) /
    L = [direct 0, direct 1, direct 2, 3 bits + 3]
-   if \\(\text{type}\\) is \\(\text{None} = 0\\), then byte array is empty
-   if \\(\text{type}\\) is \\(\text{Raw} = 1\\) or \\(\text{Brotli} = 2\\),
    then:
    -   \\(\text{size}\\) [field](#uint32-field) /
        L = [8 bits, 16 bits, 24 bits, 32 bits]
    -   uint8 repeated \\(\text{size}\\) times;
    -   payload is compressed if \\(\text{type}\\) is \\(\text{Brotli}\\)

Source code: cs/jxl/fields.h

## VarSignedMantissa

VarMantissa is a mapping from \\(\text{L}\\) bits to signed values.
The principle is that \\(\text{L} + 1\\) bits may only represent values whose
absolute values are bigger than absolute values that could be represented with
\\(\text{L}\\) bits.

-   0-bit values represent \\(\{0\}\\)
-   1-bit values represent \\(\{-1, 1\}\\)
-   2-bit values represent \\(\{-3, -2, 2, 3\}\\)
-   L-bit values represent \\(\{\pm 2^{L - 1} .. \pm(2^L - 1)\}\\)

## VarUnsignedMantissa

Analogous to [VarSignedMantissa](#varsignedmantissa), but for unsigned values.

TODO: provide examples

## VarLenUint8

-   \\(\text{zero}\\) 1 bit
-   if \\(\text{zero}\\) is \\(0\\), then value is \\(0\\), otherwise:
    -   \\(\text{L}\\) 3 bits
    -   \\(\text{value} - 1\\) encoded as
        \\(\text{L}\\)-bit [VarUnsignedMantissa](#vaunrsignedmantissa)

## VarLenUint16

-   \\(\text{zero}\\) 1 bit
-   if \\(\text{zero}\\) is \\(0\\), then value is \\(0\\), otherwise:
    -   \\(\text{L}\\) 4 bits
    -   \\(\text{value} - 1\\) encoded as
        \\(\text{L}\\)-bit [VarUnsignedMantissa](#varunsignedmantissa)
