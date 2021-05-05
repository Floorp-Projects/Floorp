# Upsampling

[TOC]

<!--*
# Document freshness: For more information, see go/fresh-source.
freshness: { owner: 'janwas' reviewed: '2019-02-22' }
*-->

jxl/resample.h provides `Upsampler8` for fast and high-quality 8x8 upsampling by
4x4 (separable/non-separable) or 6x6 (non-separable) floating-point kernels.
This was previously used for the "smooth predictor", which has been removed. It
would still be useful for a progressive mode that upsamples DC for a preview,
though this code is not yet used for that purpose.

See 'Separability' section below for the surprising result that non-separable
can be faster than separable and possibly better.

## Performance evaluation

### 4x4 separable/non-separable

__Single-core__: 5.1 GB/s (single-channel, floating-point, 160x96 input)

*   40x speedup vs. an unoptimized Nehab/Hoppe bicubic upsampler
*   25x or 6x speedup vs. [Pillow 4.3](http://python-pillow.org/pillow-perf/)
    bicubic (AVX2: 66M RGB 8-bit per second) in terms of bytes or samples
*   2.6x AVX2 speedup vs. our SSE4 version (similar algorithm)

__12 independent instances on 12 cores__: same speed for each instance (linear
scalability: not limited by memory bandwidth).

__Multicore__: 15-18 GB/s (single-channel, floating-point, 320x192 input)

*   9-11x or 2.3-2.8x speedup vs. a parallel Halide bicubic (1.6G single-channel
    8-bit per second, 320x192 input) in terms of bytes or samples.

### 6x6 non-separable

Note that a separable (outer-product) kernel only requires 6+6 (12)
multiplications per output pixel/vector. However, we do not assume anything
about the kernel rank (separability), and thus require 6x6 (36) multiplications.

__Single-core__: 2.8 GB/s (single-channel, floating-point, 320x192 input)

*   5x speedup vs. optimized (outer-product of two 1D) Lanczos without SIMD

__Multicore__: 9-10 GB/s (single-channel, floating-point, 320x192 input)

*   7-8x or 2x speedup vs. a parallel Halide 6-tap tensor-product Lanczos (1.3G
    single-channel 8-bit per second, 320x192 input) in terms of bytes or
    samples.

## Implementation details

### Data type

Our input pixels are 16-bit, so 8-bit integer multiplications are insufficient.
16-bit fixed-point arithmetic is fast but risks overflow or loss of precision
unless the value intervals are carefully managed. 32-bit integers reduce this
concern, but 32-bit multiplies are slow on Haswell (one mullo32 per cycle with
10+1 cycle latency). We instead use single-precision float, which enables 2 FMA
per cycle with 5 cycle latency. The extra bandwidth vs. 16-bit types types may
be a concern, but we can run 12 instances (on a 12-core socket) with zero
slowdown, indicating that memory bandwidth is not the bottleneck at the moment.

### Pixel layout

We require planar inputs, e.g. all red channel samples in a 2D matrix. This
allows better utilization of 8-lane SIMD compared to dedicating four SIMD lanes
to R,G,B,A samples. If upsampling is the only operation, this requires an extra
deinterleaving step, but our application involves a larger processing pipeline.

### No prefilter

Nehab and Hoppe (http://hhoppe.com/filtering.pdf) advocate generalized sampling
with an additional digital filter step. They claim "significantly higher
quality" for upsampling operations, which is contradicted by minimal MSSIM
differences between cardinal and ordinary cubic interpolators in their
experiment on page 65. We also see only minor Butteraugli differences between
Catmull-Rom and B-spline3 or OMOMS3 when upsampling 8x. As they note on page 29,
a prefilter is often omitted when upsampling because the reconstruction filter's
frequency cutoff is lower than that of the prefilter. The prefilter is claimed
to be efficient (relative to normal separable convolutions), but still involves
separate horizontal and vertical passes. To avoid cache thrashing would require
a separate ring buffer of rows, which may be less efficient than our single-pass
algorithm which only writes final outputs.

### 8x upsampling

Our code is currently specific to 8x upsampling because that is what is required
for our (DCT prediction) application. This happens to be a particularly good fit
for both 4-lane and 8-lane (AVX2) SIMD AVX2. It would be relatively easy to
adapt the code to 4x upsampling.

### Kernel support

Even (asymmetric) kernels are often used to reduce computation. Many upsampling
applications use 4-tap cubic interpolation kernels but at such extreme
magnifications (8x) we find 6x6 to be better.

### Separability

Separable kernels can be expressed as the (outer) product of two 1D kernels. For
n x n kernels, this requires n + n multiplications per pixel rather than n x n.
However, the Fourier transform of such kernels (except the Gaussian) has a
square rather than disc shape.

We instead allow arbitrary (possibly non-separable) kernels. This can avoid
structural dependencies between output pixels within the same 8x8 block.

Surprisingly, 4x4 non-separable is actually faster than the separable version
due to better utilization of FMA units. For 6x6, we only implement the
non-separable version because our application benefits from such kernels.

### Dual grid

A primal grid with `n` grid points at coordinates `k/n` would be convenient for
index computations, but is asymmetric at the borders (0 and `(n-1)/n` != 1) and
does not compensate for the asymmetric (even) kernel support. We instead use a
dual grid with coordinates offset by half the sample spacing, which only adds a
integer shift term to the index computations. Note that the offset still leads
to four identical input pixels in 8x upsampling, which is convenient for 4 and
even 8-lane SIMD.

### Kernel

We use Catmull-Rom splines out of convenience. Computational cost does not
matter because the weights are precomputed. Also, Catmull-Rom splines pass
through the control points, but that does not matter in this application.
B-splines or other kernels (arbitrary coefficients, even non-separable) can also
be used.

### Single pixel loads

For SIMD convolution, we have the choice whether to broadcast inputs or weights.
To ensure we write a unit-stride vector of output pixels, we need to broadcast
the input pixels and vary the weights. This has the additional benefit of
avoiding complexity at the borders, where it is not safe to load an entire
vector. Instead, we only load and broadcast single pixels, with bounds checking
at the borders but not in the interior.

### Single pass

Separable 2D convolutions are often implemented as two separate 1D passes.
However, this leads to cache thrashing in the vertical pass, assuming the image
does not fit into L2 caches. We instead use a single-pass algorithm that
generates four (see "Kernel support" above) horizontal convolution results and
immediately convolves them in the vertical direction to produce a final output.
This is simpler than sliding windows and has a smaller working set, thus
enabling the major speedups reported above.
