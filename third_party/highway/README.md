# Efficient and performance-portable vector software

[//]: # (placeholder, do not remove)

Highway is a C++ library that provides portable SIMD/vector intrinsics.

## Why

We are passionate about high-performance software. We see major untapped
potential in CPUs (servers, mobile, desktops). Highway is for engineers who want
to reliably and economically push the boundaries of what is possible in
software.

## How

CPUs provide SIMD/vector instructions that apply the same operation to multiple
data items. This can reduce energy usage e.g. *fivefold* because fewer
instructions are executed. We also often see *5-10x* speedups.

Highway makes SIMD/vector programming practical and workable according to these
guiding principles:

**Does what you expect**: Highway is a C++ library with carefully-chosen
functions that map well to CPU instructions without extensive compiler
transformations. The resulting code is more predictable and robust to code
changes/compiler updates than autovectorization.

**Works on widely-used platforms**: Highway supports four architectures; the
same application code can target eight instruction sets, including those with
'scalable' vectors (size unknown at compile time). Highway only requires C++11
and supports four families of compilers. If you would like to use Highway on
other platforms, please raise an issue.

**Flexible to deploy**: Applications using Highway can run on heterogeneous
clouds or client devices, choosing the best available instruction set at
runtime. Alternatively, developers may choose to target a single instruction set
without any runtime overhead. In both cases, the application code is the same
except for swapping `HWY_STATIC_DISPATCH` with `HWY_DYNAMIC_DISPATCH` plus one
line of code.

**Suitable for a variety of domains**: Highway provides an extensive set of
operations, used for image processing (floating-point), compression, video
analysis, linear algebra, cryptography, sorting and random generation. We
recognise that new use-cases may require additional ops and are happy to add
them where it makes sense (e.g. no performance cliffs on some architectures). If
you would like to discuss, please file an issue.

**Rewards data-parallel design**: Highway provides tools such as Gather,
MaskedLoad, and FixedTag to enable speedups for legacy data structures. However,
the biggest gains are unlocked by designing algorithms and data structures for
scalable vectors. Helpful techniques include batching, structure-of-array
layouts, and aligned/padded allocations.

## Examples

Online demos using Compiler Explorer:

-   [generating code for multiple targets](https://gcc.godbolt.org/z/n6rx6xK5h) (recommended)
-   [single target using -m flags](https://gcc.godbolt.org/z/rGnjMevKG)

Projects using Highway: (to add yours, feel free to raise an issue or contact us
via the below email)

*   [iresearch database index](https://github.com/iresearch-toolkit/iresearch/blob/e7638e7a4b99136ca41f82be6edccf01351a7223/core/utils/simd_utils.hpp)
*   [JPEG XL image codec](https://github.com/libjxl/libjxl)
*   [Grok JPEG 2000 image codec](https://github.com/GrokImageCompression/grok)
*   [vectorized Quicksort](https://github.com/google/highway/tree/master/hwy/contrib/sort)

## Current status

### Targets

Supported targets: scalar, S-SSE3, SSE4, AVX2, AVX-512, AVX3_DL (~Icelake,
requires opt-in by defining `HWY_WANT_AVX3_DL`), NEON (ARMv7 and v8), SVE, SVE2,
WASM SIMD.

SVE was initially tested using farm_sve (see acknowledgments). A subset of RVV
is implemented and tested with LLVM and QEMU. Work is underway to add RVV ops
which were not yet supported by GCC.

### Versioning

Highway releases aim to follow the semver.org system (MAJOR.MINOR.PATCH),
incrementing MINOR after backward-compatible additions and PATCH after
backward-compatible fixes. We recommend using releases (rather than the Git tip)
because they are tested more extensively, see below.

Version 0.11 is considered stable enough to use in other projects.
Version 1.0 will signal an increased focus on backwards compatibility and will
be reached after the RVV target is finished (planned for 2022H1).

### Testing

Continuous integration tests build with a recent version of Clang (running on
x86 and QEMU for ARM) and MSVC from VS2015 (running on x86).

Before releases, we also test on x86 with Clang and GCC, and ARMv7/8 via
GCC cross-compile and QEMU. See the
[testing process](g3doc/release_testing_process.md) for details.

### Related modules

The `contrib` directory contains SIMD-related utilities: an image class with
aligned rows, a math library (16 functions already implemented, mostly
trigonometry), and functions for computing dot products and sorting.

## Installation

This project uses CMake to generate and build. In a Debian-based system you can
install it via:

```bash
sudo apt install cmake
```

Highway's unit tests use [googletest](https://github.com/google/googletest).
By default, Highway's CMake downloads this dependency at configuration time.
You can disable this by setting the `HWY_SYSTEM_GTEST` CMake variable to ON and
installing gtest separately:

```bash
sudo apt install libgtest-dev
```

To build Highway as a shared or static library (depending on BUILD_SHARED_LIBS),
the standard CMake workflow can be used:

```bash
mkdir -p build && cd build
cmake ..
make -j && make test
```

Or you can run `run_tests.sh` (`run_tests.bat` on Windows).

Bazel is also supported for building, but it is not as widely used/tested.

## Quick start

You can use the `benchmark` inside examples/ as a starting point.

A [quick-reference page](g3doc/quick_reference.md) briefly lists all operations
and their parameters, and the [instruction_matrix](g3doc/instruction_matrix.pdf)
indicates the number of instructions per operation.

We recommend using full SIMD vectors whenever possible for maximum performance
portability. To obtain them, pass a `ScalableTag<float>` (or equivalently
`HWY_FULL(float)`) tag to functions such as `Zero/Set/Load`. There are two
alternatives for use-cases requiring an upper bound on the lanes:

-   For up to a power of two `N`, specify `CappedTag<T, N>` (or
    equivalently `HWY_CAPPED(T, N)`). This is useful for data structures such as
    a narrow matrix. A loop is still required because vectors may actually have
    fewer than `N` lanes.

-   For exactly a power of two `N` lanes, specify `FixedTag<T, N>`. The largest
    supported `N` depends on the target, but is guaranteed to be at least
    `16/sizeof(T)`.

Functions using Highway must either be inside `namespace HWY_NAMESPACE {`
(possibly nested in one or more other namespaces defined by the project), OR
each op must be prefixed with `hn::`, e.g. `namespace hn = hwy::HWY_NAMESPACE;
hn::LoadDup128()`. Additionally, each function using Highway must either be
prefixed with `HWY_ATTR`, OR reside between `HWY_BEFORE_NAMESPACE()` and
`HWY_AFTER_NAMESPACE()`.

*   For static dispatch, `HWY_TARGET` will be the best available target among
    `HWY_BASELINE_TARGETS`, i.e. those allowed for use by the compiler (see
    [quick-reference](g3doc/quick_reference.md)). Functions inside
    `HWY_NAMESPACE` can be called using `HWY_STATIC_DISPATCH(func)(args)` within
    the same module they are defined in. You can call the function from other
    modules by wrapping it in a regular function and declaring the regular
    function in a header.

*   For dynamic dispatch, a table of function pointers is generated via the
    `HWY_EXPORT` macro that is used by `HWY_DYNAMIC_DISPATCH(func)(args)` to
    call the best function pointer for the current CPU's supported targets. A
    module is automatically compiled for each target in `HWY_TARGETS` (see
    [quick-reference](g3doc/quick_reference.md)) if `HWY_TARGET_INCLUDE` is
    defined and `foreach_target.h` is included.

## Compiler flags

Applications should be compiled with optimizations enabled - without inlining,
SIMD code may slow down by factors of 10 to 100. For clang and GCC, `-O2` is
generally sufficient.

For MSVC, we recommend compiling with `/Gv` to allow non-inlined functions to
pass vector arguments in registers. If intending to use the AVX2 target together
with half-width vectors (e.g. for `PromoteTo`), it is also important to compile
with `/arch:AVX2`. This seems to be the only way to generate VEX-encoded SSE4
instructions on MSVC. Otherwise, mixing VEX-encoded AVX2 instructions and
non-VEX SSE4 may cause severe performance degradation. Unfortunately, the
resulting binary will then require AVX2. Note that no such flag is needed for
clang and GCC because they support target-specific attributes, which we use to
ensure proper VEX code generation for AVX2 targets.

## Strip-mining loops

To vectorize a loop, "strip-mining" transforms it into an outer loop and inner
loop with number of iterations matching the preferred vector width.

In this section, let `T` denote the element type, `d = ScalableTag<T>`, `count`
the number of elements to process, and `N = Lanes(d)` the number of lanes in a
full vector. Assume the loop body is given as a function `template<bool partial,
class D> void LoopBody(D d, size_t index, size_t max_n)`.

Highway offers several ways to express loops where `N` need not divide `count`:

*   Ensure all inputs/outputs are padded. Then the loop is simply

    ```
    for (size_t i = 0; i < count; i += N) LoopBody<false>(d, i, 0);
    ```
    Here, the template parameter and second function argument are not needed.

    This is the preferred option, unless `N` is in the thousands and vector
    operations are pipelined with long latencies. This was the case for
    supercomputers in the 90s, but nowadays ALUs are cheap and we see most
    implementations split vectors into 1, 2 or 4 parts, so there is little cost
    to processing entire vectors even if we do not need all their lanes. Indeed
    this avoids the (potentially large) cost of predication or partial
    loads/stores on older targets, and does not duplicate code.

*   Process whole vectors as above, followed by a scalar loop:

    ```
    size_t i = 0;
    for (; i + N <= count; i += N) LoopBody<false>(d, i, 0);
    for (; i < count; ++i) LoopBody<false>(HWY_CAPPED(T, 1)(), i, 0);
    ```
    The template parameter and second function arguments are again not needed.

    This avoids duplicating code, and is reasonable if `count` is large.
    If `count` is small, the second loop may be slower than the next option.

*   Process whole vectors as above, followed by a single call to a modified
    `LoopBody` with masking:

    ```
    size_t i = 0;
    for (; i + N <= count; i += N) {
      LoopBody<false>(d, i, 0);
    }
    if (i < count) {
      LoopBody<true>(d, i, count - i);
    }
    ```
    Now the template parameter and third function argument can be used inside
    `LoopBody` to 'blend' the new partial vector with previous memory contents:
    `Store(IfThenElse(FirstN(d, N), partial, prev_full), d, aligned_pointer);`.

    This is a good default when it is infeasible to ensure vectors are padded.
    In contrast to the scalar loop, only a single final iteration is needed.
    The increased code size from two loop bodies is expected to be worthwhile
    because it avoids the cost of masking in all but the final iteration.

## Additional resources

*   [Highway introduction (slides)](g3doc/highway_intro.pdf)
*   [Overview of instructions per operation on different architectures](g3doc/instruction_matrix.pdf)
*   [Design philosophy and comparison](g3doc/design_philosophy.md)

## Acknowledgments

We have used [farm-sve](https://gitlab.inria.fr/bramas/farm-sve) by Berenger
Bramas; it has proved useful for checking the SVE port on an x86 development
machine.

This is not an officially supported Google product.
Contact: janwas@google.com
