[![Build SSE](https://img.shields.io/jenkins/s/http/vali.inf.ed.ac.uk/jenkins/view/intgemm/job/intgemm-SSE.svg?label=SSE)](http://vali.inf.ed.ac.uk/jenkins/job/intgemm-SSE/)
[![Build AVX2](https://img.shields.io/jenkins/s/http/vali.inf.ed.ac.uk/jenkins/view/intgemm/job/intgemm-AVX2.svg?label=AVX2)](http://vali.inf.ed.ac.uk/jenkins/job/intgemm-AVX2/)
[![Build AVX512BW](https://img.shields.io/jenkins/s/http/vali.inf.ed.ac.uk/jenkins/view/intgemm/job/intgemm-AVX512BW.svg?label=AVX512BW)](http://vali.inf.ed.ac.uk/jenkins/job/intgemm-AVX512BW/)
![Build Ubuntu](https://github.com/kpu/intgemm/workflows/Ubuntu/badge.svg)
![Build Ubuntu debug](https://github.com/kpu/intgemm/workflows/Ubuntu%20debug/badge.svg)
![Build Ubuntu OpenMP](https://github.com/kpu/intgemm/workflows/Ubuntu%20OpenMP/badge.svg)
![Build Windows](https://github.com/kpu/intgemm/workflows/Windows/badge.svg)
![Build Mac](https://github.com/kpu/intgemm/workflows/Mac/badge.svg)
[![Intel Compiler](https://github.com/kpu/intgemm/actions/workflows/intel-19.yml/badge.svg)](https://github.com/kpu/intgemm/actions/workflows/intel-19.yml)

# Integer Matrix Multiplication

This repository implements 8-bit and 16-bit matrix multiplication:

C = A * B

It's designed with neural network inference in mind: A is typically activations, B is typically fixed parameters, and C is activations for the next layer.

A can have any number of rows.  Typically this is a batch size.
The shared dimension, A's columns and B's rows, must be a multiple of 32 (for 16-bit) or 64 (for 8-bit).
B's columns must be a multiple of 8.

## Accuracy
16-bit multiplication accumulates into 32-bit integers WITHOUT SATURATION (because there is no 32-bit add with saturation). If width is too large (i.e. >2048) or many 16-bit values are large, there is substantial risk of overflow.  Choose a smaller quantization multiplier to scale things down or implement periodic upcasting to 64-bit for me.

8-bit multiplication accumulates into 16-bit integers with saturation.  This saturates for larger widths (~1024) and is worst on SSSE3 because it accumulates in fewer values.  It's possible to upcast to 32-bit every so often, but this has not been implemented yet.

## Usage

A full example appears in [example.cc](example.cc).

Both A and B should be prepared before multiplication.
```C++
#include "intgemm/intgemm.h"

/* Not shown: allocate 64-byte aligned memory with e.g. aligned_alloc.
 * A is A_rows x width.
 * B is width x B_cols.
 */
/* Prepare A for multiplication.  This might be offline or on the fly. */
intgemm::Int16::PrepareA(A.begin(), A_prepared.begin(), quant_mult, A_rows, width);
/* Prepare B for multiplication.  This is typically done offline. */
intgemm::Int16::PrepareB(B.begin(), B_prepared.begin(), quant_mult, width, B_cols);
/* Multiply and produce results in C */
intgemm::Int16::Multiply(A_prepared.begin(), B_prepared.begin(), A_rows, width, B_cols, intgemm::callbacks::UnquantizeAndWrite(1.0 / (quant_mult * quant_mult), C.begin()));
```
For 8-bit, use `Int8` instead of `Int16`.

When repesented as floats, all of A, B, and C are in row-major format.

The last argument of `Multiply` is a callback which is usually used to performs postprocessing on the output matrix (C). Full set of built-in callbacks can be found in [callbacks/configs.h](callbacks/configs.h). You can also write your own callback. To do that you just need to:
1. Add configuration structure for your callback in [callbacks/configs.h](callbacks/configs.h).
2. Add your callback implementation:
   - in [callbacks/implementations.inl](callbacks/implementations.inl) if you want to implement it for all architecturs at the same time.
   - in `callbacks/ARCHITECTURE.h` (e.g. [callbacks/sse2.h](callbacks/sse2.h)) if you want to implement it only for the specific architecture.

For 8-bit, you can make use a of a slightly faster implementation, assuming you can determine tha quantization multipliers and prepare the biases offline:

```C++
#include "intgemm/intgemm.h"

/* Not shown: allocate 64-byte aligned memory with e.g. aligned_alloc.
 * A is A_rows x width.
 * B is width x B_cols.
 * If you want to make use of the slightly faster 8bit codepath (assuming you can cache biases and quantization multipliers)
 * This routine only supports C = A*B + Bias
 * In practise it computes C = (A+127)*B + Bias - |127|*B
 * Prepare A and B first:
 */
float alpha = 25;
float quant_mult = 127/alpha;
intgemm::Int8Shift::PrepareA(A.begin(), A_prepared.begin(), quant_mult, A_rows, width);
intgemm::Int8Shift::PrepareB(B.begin(), B_prepared.begin(), quant_mult, width, B_cols);
/* Prepare the bias (inplace) */
float unquant_mult_forprep = (-1)*(alpha)*(alpha)/(127.0f);
intgemm::Int8Shift::PrepareBias(B_prepared.begin(), width, B_cols, callbacks::UnquantizeAndAddBiasAndWrite(unquant_mult_forprep, inputBias.begin(), inputBias.begin()));
/* Multiply */
intgemm::Int8Shift::Multiply(A_prepared.begin(), B_prepared.begin(), A_rows, width, B_cols, callbacks::UnquantizeAndAddBiasAndWrite(unquant_mult_forprep, bias.begin(), C.begin()));
```

## Quantization
Floating-point values are multiplied by a user-specified constant then rounded to an integer.

In 16 bit, Jacob Devlin recommends 1024.0 for neural networks to prevent the aforementioned overflow.

In 8 bit, use 127.0 / the largest value (use MaxAbsolute).  Quantization will saturate so it's possible to use larger multipliers to obtain clipping.

## Acknowledgments
The original 16-bit SSE2 code came from:

Sharp Models on Dull Hardware: Fast and Accurate Neural Machine Translation Decoding on the CPU by Jacob Devlin https://arxiv.org/abs/1705.01991 under the MIT license.
