# DC prediction

[TOC]

<!--*
# Document freshness: For more information, see go/fresh-source.
freshness: { owner: 'janwas' reviewed: '2019-02-22' }
*-->

## Background

After the DCT integral transform, we wish to reduce the encoded size of DC
coefficients (or original pixels if the transform is omitted).
pik/dc_predictor.h provides `Shrink` and `Expand` functions to reduce the
magnitude of pixels by subtracting predicted values computed from their
neighbors. In the ideal case of flat or slowly varying regions, the resulting
"residuals" are zero and thus very efficiently encodable.

## Design goals

1.  Benefit from SIMD.
1.  Use prior horizontal neighbor for prediction because it has the highest
    correlation.
1.  Use previously decoded planes (i.e. luminance) for improving prediction of
    chrominance.
1.  Lossless (no overflow/rounding errors) for signed integers in [-32768,
    32768).

## Prior approaches

CALIC GAP and LOCO-I/JPEG-LS MED are somewhat adaptive: they choose from one of
several simple predictors based on properties of the pixel neighborhood (let N
denote the pixel above the current one, W = left, and NW above-left). History
Based Blending introduced more freedom by adding adaptive weights and several
more predictors including averaging, gradients and neighbors. We use a similar
approach, but with selecting rather than blending predictors to avoid
multiplications, and a more efficient cost estimator.

## Basic algorithm

```
Compute all predictors for the already decoded N and W pixel.
See which single predictor performs best (lowest total absolute residual).
Residual := current pixel - best predictor's prediction for current pixel.
To expand again, add this same prediction to the stored residual.
```

## Details

### Dependencies between SIMD lanes

The SIMD and prior horizontal neighbor requirements seem to be contradictory. If
all steps are computed in units of SIMD vectors, then the prior neighbor may be
part of the same vector and thus not computed beforehand. Instead, we compute
multiple _predictors_ at a time using SIMD for the same pixel. This is feasible
because several predictors use the same computation (e.g. Average) on
independent inputs in the SIMD vector lanes.

### Predictors

We use several kinds of predictors.

1.  **Average**: Noisy areas have outliers; computing the average is a simple
    way to reduce their impact without expensive non-linear computations. We use
    a (saturated) add and shift rather than the average+round SIMD instruction.
1.  **Gradient**: Smooth areas benefit from a gradient; we find it helpful to
    clamp the JPEG-LS-style N+W-NW to the min/max of all neighbor pixels.
1.  **Edge**: Finally, unchanged N/W neighbor values are also helpful for
    regions with edges because they violate the assumptions underlying the
    average and gradient predictors (namely a shared central tendency or
    smoothly varying neighborhood).

We chose predictors subject to the constraint that 128-bit SIMD instruction sets
can compute eight of them at a time, with a simple optimization algorithm that
tried various permutations of these basic predictors. Note that order matters:
the first predictor with min cost is chosen. The test data set was small, so it
is possible that the choice of predictors could be improved, but that involves
rewriting some intricate SIMD code.

### Finding best predictor

Given a SIMD vector with all prediction results, we compute each of their
residuals by subtracting from the (broadcasted) current pixel. X86 SSE4 provides
special support for finding the minimum element in u16x8 vectors. This lane can
efficiently moved to the first lane via shuffling. On other architectures, we
scan through the 8 lanes manually.

### Threading

No separate parallelization is needed because this is part of the JXL decoder,
which ensures all pixel decoding steps are independent (in 512x512 groups) and
computed in parallel.

### Cross-channel correlations

The adaptive predictor exploits correlations between neighboring pixels. To also
reduce correlation between channels, we require the Y channel to be decoded
first (green is in the middle of the frequency band and thus has higher
correlation with red/blue). Then, rather than computing the cost (prediction
residual) at N and W neighbors, we compute the cost at the same position in the
previously decoded luminance image.
Note: this assumes there is some remaining correlation (despite chroma from
luma already being carried out), which is plausible because the current CfL
model for DC is global, i.e. unable to remove all correlations. Once that
changes, it may be better to no longer use Y to influence the chroma channels.

### Bounds checking

Predictors only use immediately adjacent pixels, and the cost estimator also
applies predictors at immediately adjacent neighbors, so we need a border of two
pixels on each side. For the first pixel, we have no predictor; subsequent
pixels in the same row use their left neighbor. In the second row, we can apply
the adaptive predictor while supplying an imaginary extra top row that is
identical to the first row.

### Side information

Only causal (previously decoded in left to right, then top to bottom line scan
order) neighbors are used, so the decoder-side `Expand` can run without any side
information.

### Related work

lossless16 by Alex Rhatushnyak implements a more powerful predictor which uses
the error history as feedback for the prediction weights. lossless16 is indeed
better than dc_predictor for lossless photo compression. However, using
lossless16 for encoding DCs has not yet proven to be advantageous - even after
entropy-coding of the channel compact transforms, and encoding the entire DC
image in a single call.
