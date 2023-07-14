# SIMD Everywhere
<!-- ALL-CONTRIBUTORS-BADGE:START - Do not remove or modify this section -->
[![All Contributors](https://img.shields.io/badge/all_contributors-58-orange.svg?style=flat-square)](#contributors-)
<!-- ALL-CONTRIBUTORS-BADGE:END -->
[![Chat](https://badges.gitter.im/gitterHQ/gitter.png)](https://matrix.to/#/#simd-everywhere_community:gitter.im)
[![codecov](https://codecov.io/gh/simd-everywhere/simde/branch/master/graph/badge.svg?token=jcMBoRk0ui)](https://codecov.io/gh/simd-everywhere/simde)

The SIMDe header-only library provides fast, portable implementations of
[SIMD intrinsics](https://en.wikipedia.org/wiki/SIMD) on hardware which
doesn't natively support them, such as calling [SSE](https://en.wikipedia.org/wiki/Streaming_SIMD_Extensions)
functions on ARM.  There is no performance penalty if the hardware
supports the native implementation (*e.g.*, SSE/[AVX](https://en.wikipedia.org/wiki/Advanced_Vector_Extensions)
runs at full speed on [x86](https://en.wikipedia.org/wiki/X86),
[NEON](https://en.wikipedia.org/wiki/ARM_architecture#Advanced_SIMD_(Neon)) on [ARM](https://en.wikipedia.org/wiki/ARM_architecture),
*etc.*).

This makes porting code to other architectures much easier in a few
key ways:

First, instead of forcing you to rewrite everything for each
architecture, SIMDe lets you get a port up and running almost
effortlessly.  You can then start working on switching the most
performance-critical sections to native intrinsics, improving
performance gradually.  SIMDe lets (for example) SSE/AVX and NEON code
exist side-by-side, in the same implementation.

Second, SIMDe makes it easier to write code targeting [ISA](https://en.wikipedia.org/wiki/Instruction_set_architecture)
extensions you don't have convenient access to.  You can run NEON code on your
x86 machine *without an emulator*.  Obviously you'll eventually want
to test on the actual hardware you're targeting, but for most
development, SIMDe can provide a much easier path.

SIMDe takes a very different approach from most other SIMD abstraction
layers in that it aims to expose the entire functionality of the
underlying instruction set.  Instead of limiting functionality to the
lowest common denominator, SIMDe tries to minimize the amount of
effort required to port while still allowing you the space to optimize
as needed.

The current focus is on writing complete portable implementations,
though a large number of functions already have accelerated
implementations using one (or more) of the following:

 * SIMD intrinsics from other ISA extensions (e.g., using NEON to
   implement SSE).
 * Compiler-specific vector extensions and built-ins such as
   [`__builtin_shufflevector`](http://clang.llvm.org/docs/LanguageExtensions.html#langext-builtin-shufflevector)
   and
   [`__builtin_convertvector`](http://clang.llvm.org/docs/LanguageExtensions.html#langext-builtin-convertvector)
 * Compiler auto-vectorization hints, using:
   * [OpenMP 4 SIMD](http://www.openmp.org/)
   * [Cilk Plus](https://www.cilkplus.org/)
   * [GCC loop-specific pragmas](https://gcc.gnu.org/onlinedocs/gcc/Loop-Specific-Pragmas.html)
   * [clang pragma loop hint directives](http://llvm.org/docs/Vectorizers.html#pragma-loop-hint-directives)

You can [try SIMDe online](https://simde.netlify.app/godbolt/demo)
using Compiler Explorer and an amalgamated SIMDe header.

If you have any questions, please feel free to use the
[issue tracker](https://github.com/simd-everywhere/simde/issues) or the
[mailing list](https://groups.google.com/forum/#!forum/simde).

## Current Status

There are currently complete implementations of the following instruction
set extensions:

* x86 / x86_64
  * [MMX](https://en.wikipedia.org/wiki/MMX_(instruction_set))
  * [SSE](https://en.wikipedia.org/wiki/Streaming_SIMD_Extensions)
  * [SSE2](https://en.wikipedia.org/wiki/SSE2)
  * [SSE3](https://en.wikipedia.org/wiki/SSE3)
  * [SSSE3](https://en.wikipedia.org/wiki/SSSE3)
  * [SSE4.1](https://en.wikipedia.org/wiki/SSE4#SSE4.1)
  * [AVX](https://en.wikipedia.org/wiki/Advanced_Vector_Extensions)
  * [AVX2](https://en.wikipedia.org/wiki/Advanced_Vector_Extensions#Advanced_Vector_Extensions_2)
  * [FMA](https://en.wikipedia.org/wiki/FMA_instruction_set)
  * [GFNI](https://en.wikipedia.org/wiki/AVX-512#GFNI)
  * [CLMUL](https://en.wikipedia.org/wiki/CLMUL_instruction_set)
  * [XOP](https://en.wikipedia.org/wiki/XOP_instruction_set)
  * [SVML](https://software.intel.com/content/www/us/en/develop/documentation/cpp-compiler-developer-guide-and-reference/top/compiler-reference/intrinsics/intrinsics-for-intel-advanced-vector-extensions-512-intel-avx-512-instructions/intrinsics-for-arithmetic-operations-1/intrinsics-for-short-vector-math-library-svml-operations.html)
* WebAssembly
  * [SIMD128](https://github.com/WebAssembly/simd)

As well as partial support for many others, including NEON and SVE in
addition to several AVX-512 extensions.  See the
[instruction-set-support](https://github.com/simd-everywhere/simde/issues?q=is%3Aissue+is%3Aopen+label%3Ainstruction-set-support+sort%3Aupdated-desc)
label in the issue tracker for details on progress.  If you'd like to
be notified when an instruction set is available you may subscribe to
the relevant issue.

If you have a project you're interested in using with SIMDe but we
don't yet support all the functions you need, please file an issue
with a list of what's missing so we know what to prioritize.

The default branch is protected so commits never reach it unless
they have passed extensive CI checks.  Status badges don't really
make sense since they will always be green, but here are the links:

* [GitHub Actions](https://github.com/simd-everywhere/simde/actions)
* [Cirrus CI](https://cirrus-ci.com/github/simd-everywhere/simde)
* [Semaphore CI](https://nemequ.semaphoreci.com/projects/simde)
* [Circle CI](https://app.circleci.com/pipelines/github/simd-everywhere/simde)
* [AppVeyor](https://ci.appveyor.com/project/nemequ/simde)
* [Azure Pipelines](https://dev.azure.com/simd-everywhere/SIMDe/_build)
* [Drone CI](https://cloud.drone.io/simd-everywhere/simde/)

If you're adding a new build I suggest Cirrus CI, which is where we
currently have the most room given the number of builds currently on
the platform and the quotas for free/open-source usage.  Alternately,
feel free to set up another provider (such as
[Codefresh](https://codefresh.io/),
[Shippable](https://www.shippable.com/),
[Bitrise](https://www.bitrise.io/),
[Werkaer](https://app.wercker.com/), etc.).

*Notice*: we plan on changing the name of the default branch from
"master" to something else soon; we are just trying to wait to see what
name git settles on so we can be consistent.

## Contributing

First off, if you're reading this: thank you! Even considering
contributing to SIMDe is very much appreciated!

SIMDe is a fairly large undertaking; there are a *lot* of functions to
get through and a lot of opportunities for optimization on different
platforms, so we're very happy for any help you can provide.

Programmers of all skill levels are welcome, there are lots of tasks
which are pretty straightforward and don't require any special
expertise.

If you're not sure how you'd like to contribute, please consider taking
a look at [the issue tracker](https://github.com/simd-everywhere/simde/issues).
There is a [good first issue](https://github.com/simd-everywhere/simde/issues?q=is%3Aissue+is%3Aopen+label%3A%22good+first+issue%22)
tag if you want to ease into a your first contributions, but if you're
interested in something else please get in touch via the issue tracker;
we're happy to help you get a handle on whatever you are interested in.

If you're interested in implementing currently unimplemented functions,
there is [a
guide](https://github.com/simd-everywhere/simde/wiki/Implementing-a-New-Function)
explaining how to add new functions and how to quickly and easily get
a test case in place.  It's a bit rough right now, but if anything is
unclear please feel free to use the issue tracker to ask about
anything you're not clear on.

## Usage

First, it is important to note that *you do not need two separate
versions* (one using SIMDe, the other native).  If the native functions
are available SIMDe will use them, and compilers easily optimize away
any overhead from SIMDe; all they have to do is some basic inlining.
`-O2` should be enough, but we strongly recommend `-O3` (or whatever
flag instructs your compiler to aggressizely optimize) since many of
the portable fallbacks are substantially faster with aggressive
auto-vectorization that isn't enabled at lower optimization levels.

Each instruction set has a separate file; `x86/mmx.h` for MMX,
`x86/sse.h` for SSE, `x86/sse2.h` for SSE2, and so on.  Just include
the header for whichever instruction set(s) you want *instead of the
native version* (if you include the native version after SIMDe it will
result in compile-time errors if native aliases are enabled).  SIMDe
will provide the fastest implementation it can given which extensions
you've enabled in your compiler (i.e., if you want to use NEON to
implement SSE, you may need to pass something like `-mfpu=neon`
or `-march=armv8-a+simd`.  See
[GCC ARM-Options](https://gcc.gnu.org/onlinedocs/gcc/ARM-Options.html)
for more information).

If you define `SIMDE_ENABLE_NATIVE_ALIASES` before including SIMDe
you can use the same names as the native functions.  Unfortunately,
this is somewhat error-prone due to portability issues in the APIs, so
it's recommended to only do this for testing.  When
`SIMDE_ENABLE_NATIVE_ALIASES` is undefined only the versions prefixed
with `simde_` will be available; for example, the MMX `_mm_add_pi8`
intrinsic becomes `simde_mm_add_pi8`, and `__m64` becomes `simde__m64`.

Since SIMDe is meant to be portable, many functions which assume types
are of a specific size have been altered to use fixed-width types
instead.  For example, Intel's APIs use `char` for signed 8-bit
integers, but `char` on ARM is generally unsigned.  SIMDe uses `int8_t`
to make the API portable, but that means your code may require some
minor changes (such as using `int8_t` instead of `char`) to work on
other platforms.

That said, the changes are usually quite minor.  It's often enough to
just use search and replace, manual changes are required pretty
infrequently.

### OpenMP 4 SIMD

SIMDe makes extensive use of annotations to help the compiler vectorize
code.  By far the best annotations use the SIMD support built in to
OpenMP 4, so if your compiler supports these annotations we strongly
recommend you enable them.

If you are already using OpenMP, SIMDe will automatically detect it
using the `_OPENMP` macro and no further action is required.

Some compilers allow you to enable OpenMP SIMD *without* enabling the
full OpenMP.  In such cases there is no runtime dependency on OpenMP
and no runtime overhead; SIMDe will just be faster.  Unfortunately,
SIMDe has no way to detect such situations (the `_OPENMP` macro is not
defined), so after enabling it in your compiler you'll need to define
`SIMDE_ENABLE_OPENMP` (e.g., by passing `-DSIMDE_ENABLE_OPENMP`) to get
SIMDe to output the relevant pragmas.

Enabling OpenMP SIMD support varies by compiler:

 * GCC 4.9+ and clang 6+ support a `-fopenmp-simd` command line flag.
 * ICC supports a `-qopenmp-simd` command line flag.
 * MCST's LCC enables OpenMP SIMD by default, so no flags are needed
   (technically you don't even need to pass `-DSIMDE_ENABLE_OPENMP`).

We are not currently aware of any other compilers which allow you to
enable OpenMP SIMD support without enabling full OpenMP (if you are
please file an issue to let us know).  You should determine whether you
wish to enable full OpenMP support on a case-by-case basis, but it is
likely that the overhead of linking to (but not using) the OpenMP
runtime library will be dwarfed by the performance improvements from
using the OpenMP SIMD annotations in SIMDe.

If you choose not to use OpenMP SIMD, SIMDe also supports
using [Cilk Plus](https://www.cilkplus.org/), [GCC loop-specific
pragmas](https://gcc.gnu.org/onlinedocs/gcc/Loop-Specific-Pragmas.html),
or [clang pragma loop hint
directives](http://llvm.org/docs/Vectorizers.html#pragma-loop-hint-directives),
though these are not nearly as effective as OpenMP SIMD and depending
on them will likely result in less efficient code.  All of these are
detected automatically by SIMDe, so if they are enabled in your
compiler nothing more is required.

If for some reason you do not wish to enable OpenMP 4 SIMD support even
though SIMDe detects it, you should define `SIMDE_DISABLE_OPENMP` prior
to including SIMDe.

## Portability

### Compilers

SIMDe does depend on some C99 features, though the subset supported by
MSVC also works.  While we do our best to make sure we provide optimized
implementations where they are supported, SIMDe does contain portable
fallbacks which are designed to work on any C99 compiler.

Every commit is tested in CI on multiple compilers, platforms, and
configurations, and our test coverage is extremely extensive.
Currently tested compilers include:

 * GCC versions back to 4.8
 * Clang versions back to 3.8
 * Microsoft Visual Studio back to 12 (2013)
 * IBM XL C/C++
 * Intel C/C++ Compiler (ICC)

I'm generally willing to accept patches to add support for other
compilers, as long as they're not too disruptive, *especially* if we
can get CI support going.  If using one of our existing CI providers
isn't an option then other CI platforms can be added.

### Hardware

The following architectures are tested in CI for every commit:

 * x86_64
 * x86
 * AArch64
 * ARMv8
 * ARMv7
 * PPC64
 * MIPS Loongson

We would love to add more, so patches are extremely welcome!

## Related Projects

 * The "builtins" module in
   [portable-snippets](https://github.com/nemequ/portable-snippets)
   does much the same thing, but for compiler-specific intrinsics
   (think `__builtin_clz` and `_BitScanForward`), **not** SIMD
   intrinsics.
 * Intel offers an emulator, the [Intel® Software Development
   Emulator](https://software.intel.com/en-us/articles/intel-software-development-emulator/)
   which can be used to develop software which uses Intel intrinsics
   without having to own hardware which supports them, though it
   doesn't help for deployment.
 * [Iris](https://github.com/AlexYaruki/iris) is the only other project
   I'm aware of which is attempting to create portable implementations
   like SIMDe.  SIMDe is much further along on the Intel side, but Iris
   looks to be in better shape on ARM.  C++-only, Apache 2.0 license.
   AFAICT there are no accelerated fallbacks, nor is there a good way to
   add them since it relies extensively on templates.
 * There are a few projects trying to implement one set with another:
   * [ARM_NEON_2_x86_SSE](https://github.com/intel/ARM_NEON_2_x86_SSE)
     — implementing NEON using SSE.  Quite extensive, Apache 2.0
     license.
   * [sse2neon](https://github.com/jratcliff63367/sse2neon) —
     implementing SSE using NEON.  This code has already been merged
     into SIMDe.
   * [veclib](https://github.com/IvantheDugtrio/veclib) — implementing
     SSE2 using AltiVec/VMX, using a non-free IBM library called
     [powerveclib](https://www.ibm.com/developerworks/community/groups/community/powerveclib/)
   * [SSE-to-NEON](https://github.com/otim/SSE-to-NEON) — implementing
     SSE with NEON.  Non-free, C++.
 * [arm-neon-tests](https://github.com/christophe-lyon/arm-neon-tests)
   contains tests to verify NEON implementations.

If you know of any other related projects, please [let us
know](https://github.com/simd-everywhere/simde/issues/new)!

## Caveats

Sometime features can't be emulated.  If SIMDe is operating in native
mode the functions will work as expected, but if there is no native
support some caveats apply:

 * Many functions require <math.h> and/or <fenv.h>.  SIMDe will still
   work without those headers, but the results of those functions are
   undefined.
 * x86 / x86_64
   * SSE
     * `SIMDE_MM_SET_ROUNDING_MODE()` will use `fesetround()`, altering
       the global rounding mode.
     * `simde_mm_getcsr` and `simde_mm_setcsr` only implement bits 13
       and 14 (rounding mode).
   * AVX
     * `simde_mm256_test*` do not set the CF/ZF registers as there is
       no portable way to implement that functionality.
     * `simde_mm256_zeroall` and `simde_mm256_zeroupper` are not
       implemented as there is no portable way to implement that
       functionality.

Additionally, there are some known limitations which apply when using
native aliases (`SIMDE_ENABLE_NATIVE_ALIASES`):

* On Windows x86 (but not x86_64), some MMX functions and SSE/SSE2
  functions which use MMX types (__m64) other than for pointers may
  return incorrect results.

Also, as mentioned earlier, while some APIs make assumptions about
basic types (*e.g.*, `int` is 32 bits), SIMDe does not, so many types
have been altered to use portable fixed-width versions such as
`int32_t`.

If you find any other differences, please file an issue so we can either fix
it or add it to the list above.

## Benefactors

SIMDe uses resources provided for free by a number of organizations.
While this shouldn't be taken to imply endorsement of SIMDe, we're
tremendously grateful for their support:

 * [IntegriCloud](https://integricloud.com/) — provides access to a very
   fast POWER9 server for developing AltiVec/VMX support.
 * [GCC Compile Farm](https://gcc.gnu.org/wiki/CompileFarm) — provides
   access to a wide range of machines with different architectures for
   developing support for various ISA extensions.
 * [CodeCov.io](https://codecov.io/) — provides code coverage analysis
   for our test cases.
 * [Google](https://www.google.com/) ­— financing
   [Summer of Code](https://summerofcode.withgoogle.com/), substantial
   amounts of code (Sean Maher's contributions), and an [Open Source Peer
   Bonus](https://opensource.google/docs/growing/peer-bonus/).

Without such organizations donating resources, SIMDe wouldn't be nearly
as useful or usable as it is today.

We would also like to thank anyone who has helped develop the myriad
of software on which SIMDe relies, including compilers and analysis
tools.

Finally, a special thank you to
[anyone who has contributed](https://github.com/simd-everywhere/simde/graphs/contributors)
to SIMDe, filed bugs, provided suggestions, or helped with SIMDe
development in any way.

## License

SIMDe is distributed under an MIT-style license; see COPYING for
details.

## Contributors ✨

Thanks goes to these wonderful people ([emoji key](https://allcontributors.org/docs/en/emoji-key)):

<!-- ALL-CONTRIBUTORS-LIST:START - Do not remove or modify this section -->
<!-- prettier-ignore-start -->
<!-- markdownlint-disable -->
<table>
  <tr>
    <td align="center"><a href="https://nemequ.github.io/"><img src="https://avatars0.githubusercontent.com/u/1151?v=4" width="100px;" alt=""/><br /><sub><b>Evan Nemerson</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=nemequ" title="Code">💻</a> <a href="#content-nemequ" title="Content">🖋</a> <a href="https://github.com/nemequ/simde/commits?author=nemequ" title="Documentation">📖</a> <a href="#example-nemequ" title="Examples">💡</a> <a href="#ideas-nemequ" title="Ideas, Planning, & Feedback">🤔</a> <a href="#question-nemequ" title="Answering Questions">💬</a> <a href="https://github.com/nemequ/simde/pulls?q=is%3Apr+reviewed-by%3Anemequ" title="Reviewed Pull Requests">👀</a> <a href="https://github.com/nemequ/simde/commits?author=nemequ" title="Tests">⚠️</a> <a href="#tutorial-nemequ" title="Tutorials">✅</a> <a href="#talk-nemequ" title="Talks">📢</a> <a href="https://github.com/nemequ/simde/issues?q=author%3Anemequ" title="Bug reports">🐛</a> <a href="#infra-nemequ" title="Infrastructure (Hosting, Build-Tools, etc)">🚇</a> <a href="#maintenance-nemequ" title="Maintenance">🚧</a> <a href="#projectManagement-nemequ" title="Project Management">📆</a></td>
    <td align="center"><a href="https://orcid.org/0000-0002-2961-9670"><img src="https://avatars3.githubusercontent.com/u/1330696?v=4" width="100px;" alt=""/><br /><sub><b>Michael R. Crusoe</b></sub></a><br /><a href="https://github.com/nemequ/simde/issues?q=author%3Amr-c" title="Bug reports">🐛</a> <a href="https://github.com/nemequ/simde/commits?author=mr-c" title="Code">💻</a> <a href="#eventOrganizing-mr-c" title="Event Organizing">📋</a> <a href="#fundingFinding-mr-c" title="Funding Finding">🔍</a> <a href="#ideas-mr-c" title="Ideas, Planning, & Feedback">🤔</a> <a href="#infra-mr-c" title="Infrastructure (Hosting, Build-Tools, etc)">🚇</a> <a href="#platform-mr-c" title="Packaging/porting to new platform">📦</a> <a href="https://github.com/nemequ/simde/commits?author=mr-c" title="Tests">⚠️</a></td>
    <td align="center"><a href="https://github.com/himanshi18037"><img src="https://avatars1.githubusercontent.com/u/43923076?v=4" width="100px;" alt=""/><br /><sub><b>HIMANSHI MATHUR</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=himanshi18037" title="Code">💻</a> <a href="https://github.com/nemequ/simde/commits?author=himanshi18037" title="Tests">⚠️</a></td>
    <td align="center"><a href="https://github.com/masterchef2209"><img src="https://avatars0.githubusercontent.com/u/27916352?v=4" width="100px;" alt=""/><br /><sub><b>Hidayat Khan</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=masterchef2209" title="Code">💻</a> <a href="https://github.com/nemequ/simde/commits?author=masterchef2209" title="Tests">⚠️</a></td>
    <td align="center"><a href="https://github.com/rosbif"><img src="https://avatars1.githubusercontent.com/u/553899?v=4" width="100px;" alt=""/><br /><sub><b>rosbif</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=rosbif" title="Code">💻</a> <a href="https://github.com/nemequ/simde/commits?author=rosbif" title="Tests">⚠️</a> <a href="https://github.com/nemequ/simde/issues?q=author%3Arosbif" title="Bug reports">🐛</a> <a href="#ideas-rosbif" title="Ideas, Planning, & Feedback">🤔</a> <a href="https://github.com/nemequ/simde/commits?author=rosbif" title="Documentation">📖</a></td>
    <td align="center"><a href="http://junaruga.hatenablog.com/"><img src="https://avatars2.githubusercontent.com/u/121989?v=4" width="100px;" alt=""/><br /><sub><b>Jun Aruga</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=junaruga" title="Code">💻</a> <a href="#ideas-junaruga" title="Ideas, Planning, & Feedback">🤔</a> <a href="#platform-junaruga" title="Packaging/porting to new platform">📦</a> <a href="#infra-junaruga" title="Infrastructure (Hosting, Build-Tools, etc)">🚇</a> <a href="#maintenance-junaruga" title="Maintenance">🚧</a> <a href="https://github.com/nemequ/simde/commits?author=junaruga" title="Tests">⚠️</a> <a href="https://github.com/nemequ/simde/issues?q=author%3Ajunaruga" title="Bug reports">🐛</a></td>
    <td align="center"><a href="https://github.com/marmeladema"><img src="https://avatars2.githubusercontent.com/u/1629419?v=4" width="100px;" alt=""/><br /><sub><b>Élie ROUDNINSKI</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=marmeladema" title="Code">💻</a> <a href="https://github.com/nemequ/simde/commits?author=marmeladema" title="Tests">⚠️</a></td>
  </tr>
  <tr>
    <td align="center"><a href="http://www.bache.name"><img src="https://avatars3.githubusercontent.com/u/7937081?v=4" width="100px;" alt=""/><br /><sub><b>Jesper Storm Bache</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=jsbache" title="Code">💻</a></td>
    <td align="center"><a href="https://github.com/jeffdaily"><img src="https://avatars1.githubusercontent.com/u/904248?v=4" width="100px;" alt=""/><br /><sub><b>Jeff Daily</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=jeffdaily" title="Code">💻</a> <a href="#infra-jeffdaily" title="Infrastructure (Hosting, Build-Tools, etc)">🚇</a></td>
    <td align="center"><a href="https://github.com/yekm"><img src="https://avatars2.githubusercontent.com/u/205196?v=4" width="100px;" alt=""/><br /><sub><b>Pavel</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=yekm" title="Code">💻</a></td>
    <td align="center"><a href="https://github.com/sabarishbollapragada"><img src="https://avatars3.githubusercontent.com/u/36379720?v=4" width="100px;" alt=""/><br /><sub><b>Sabarish Bollapragada</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=sabarishbollapragada" title="Code">💻</a></td>
    <td align="center"><a href="http://www.thegavinli.com/"><img src="https://avatars2.githubusercontent.com/u/371529?v=4" width="100px;" alt=""/><br /><sub><b>Gavin Li</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=gh2o" title="Code">💻</a></td>
    <td align="center"><a href="http://www.yiningkarlli.com"><img src="https://avatars0.githubusercontent.com/u/1057198?v=4" width="100px;" alt=""/><br /><sub><b>Yining Karl Li</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=betajippity" title="Code">💻</a></td>
    <td align="center"><a href="https://www.facebook.com/anirbandey303"><img src="https://avatars1.githubusercontent.com/u/29774651?v=4" width="100px;" alt=""/><br /><sub><b>Anirban Dey</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=anirbandey303" title="Documentation">📖</a></td>
  </tr>
  <tr>
    <td align="center"><a href="https://github.com/Un1Gfn"><img src="https://avatars3.githubusercontent.com/u/28521292?v=4" width="100px;" alt=""/><br /><sub><b>Darren Ng</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=Un1Gfn" title="Documentation">📖</a></td>
    <td align="center"><a href="https://github.com/FaresSalem"><img src="https://avatars0.githubusercontent.com/u/7736245?v=4" width="100px;" alt=""/><br /><sub><b>FaresSalem</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=FaresSalem" title="Documentation">📖</a></td>
    <td align="center"><a href="https://github.com/GorePradnyesh"><img src="https://avatars0.githubusercontent.com/u/843197?v=4" width="100px;" alt=""/><br /><sub><b>Pradnyesh Gore</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=GorePradnyesh" title="Code">💻</a></td>
    <td align="center"><a href="https://github.com/seanptmaher"><img src="https://avatars0.githubusercontent.com/u/39571964?v=4" width="100px;" alt=""/><br /><sub><b>Sean Maher</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=seanptmaher" title="Code">💻</a></td>
    <td align="center"><a href="https://zh.wikipedia.org/wiki/User:Artoria2e5"><img src="https://avatars.githubusercontent.com/u/6459309?v=4" width="100px;" alt=""/><br /><sub><b>Mingye Wang</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=Artoria2e5" title="Documentation">📖</a></td>
    <td align="center"><a href="http://www.ngzhian.com"><img src="https://avatars.githubusercontent.com/u/1749303?v=4" width="100px;" alt=""/><br /><sub><b>Ng Zhi An</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=ngzhian" title="Code">💻</a> <a href="https://github.com/nemequ/simde/commits?author=ngzhian" title="Documentation">📖</a></td>
    <td align="center"><a href="https://github.com/Glitch18"><img src="https://avatars.githubusercontent.com/u/30515829?v=4" width="100px;" alt=""/><br /><sub><b>Atharva Nimbalkar</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=Glitch18" title="Code">💻</a> <a href="https://github.com/nemequ/simde/commits?author=Glitch18" title="Tests">⚠️</a></td>
  </tr>
  <tr>
    <td align="center"><a href="https://github.com/simba611"><img src="https://avatars.githubusercontent.com/u/59763921?v=4" width="100px;" alt=""/><br /><sub><b>simba611</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=simba611" title="Code">💻</a> <a href="https://github.com/nemequ/simde/commits?author=simba611" title="Tests">⚠️</a></td>
    <td align="center"><a href="https://ashnewmanjones.com"><img src="https://avatars.githubusercontent.com/u/29983014?v=4" width="100px;" alt=""/><br /><sub><b>Ashleigh Newman-Jones</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=ashnewmanjones" title="Code">💻</a> <a href="https://github.com/nemequ/simde/commits?author=ashnewmanjones" title="Tests">⚠️</a></td>
    <td align="center"><a href="https://alum.mit.edu/www/wrv/"><img src="https://avatars.githubusercontent.com/u/5039686?v=4" width="100px;" alt=""/><br /><sub><b>Willy R. Vasquez</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=wrv" title="Code">💻</a> <a href="#maintenance-wrv" title="Maintenance">🚧</a> <a href="https://github.com/nemequ/simde/commits?author=wrv" title="Tests">⚠️</a></td>
    <td align="center"><a href="https://cs.stanford.edu/~keithw"><img src="https://avatars.githubusercontent.com/u/208955?v=4" width="100px;" alt=""/><br /><sub><b>Keith Winstein</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=keithw" title="Code">💻</a> <a href="#maintenance-keithw" title="Maintenance">🚧</a> <a href="https://github.com/nemequ/simde/commits?author=keithw" title="Tests">⚠️</a></td>
    <td align="center"><a href="https://github.com/SoapGentoo"><img src="https://avatars.githubusercontent.com/u/16636962?v=4" width="100px;" alt=""/><br /><sub><b>David Seifert</b></sub></a><br /><a href="#maintenance-SoapGentoo" title="Maintenance">🚧</a></td>
    <td align="center"><a href="https://mirdita.de"><img src="https://avatars.githubusercontent.com/u/63657?v=4" width="100px;" alt=""/><br /><sub><b>Milot Mirdita</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=milot-mirdita" title="Code">💻</a> <a href="#maintenance-milot-mirdita" title="Maintenance">🚧</a> <a href="https://github.com/nemequ/simde/commits?author=milot-mirdita" title="Tests">⚠️</a></td>
    <td align="center"><a href="http://bitpatch.com"><img src="https://avatars.githubusercontent.com/u/4021602?v=4" width="100px;" alt=""/><br /><sub><b>aqrit</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=aqrit" title="Code">💻</a> <a href="#maintenance-aqrit" title="Maintenance">🚧</a></td>
  </tr>
  <tr>
    <td align="center"><a href="https://github.com/dgazzoni"><img src="https://avatars.githubusercontent.com/u/17934171?v=4" width="100px;" alt=""/><br /><sub><b>Décio Luiz Gazzoni Filho</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=dgazzoni" title="Code">💻</a> <a href="#maintenance-dgazzoni" title="Maintenance">🚧</a> <a href="https://github.com/nemequ/simde/commits?author=dgazzoni" title="Tests">⚠️</a></td>
    <td align="center"><a href="http://vk.com/muzzdiez"><img src="https://avatars.githubusercontent.com/u/16463967?v=4" width="100px;" alt=""/><br /><sub><b>Igor Molchanov</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=makise-homura" title="Code">💻</a> <a href="#maintenance-makise-homura" title="Maintenance">🚧</a> <a href="#platform-makise-homura" title="Packaging/porting to new platform">📦</a></td>
    <td align="center"><a href="https://github.com/anrodrig"><img src="https://avatars.githubusercontent.com/u/35815808?v=4" width="100px;" alt=""/><br /><sub><b>Andrew Rodriguez</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=anrodrig" title="Code">💻</a> <a href="#maintenance-anrodrig" title="Maintenance">🚧</a> <a href="https://github.com/nemequ/simde/commits?author=anrodrig" title="Tests">⚠️</a></td>
    <td align="center"><a href="https://github.com/Changqing-JING"><img src="https://avatars.githubusercontent.com/u/59640930?v=4" width="100px;" alt=""/><br /><sub><b>Changqing  Jing</b></sub></a><br /><a href="#maintenance-Changqing-JING" title="Maintenance">🚧</a></td>
    <td align="center"><a href="http://jpcima.sdf1.org/"><img src="https://avatars.githubusercontent.com/u/17614485?v=4" width="100px;" alt=""/><br /><sub><b>JP Cimalando</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=jpcima" title="Code">💻</a> <a href="#infra-jpcima" title="Infrastructure (Hosting, Build-Tools, etc)">🚇</a></td>
    <td align="center"><a href="https://flygoat.com/"><img src="https://avatars.githubusercontent.com/u/5955297?v=4" width="100px;" alt=""/><br /><sub><b>Jiaxun Yang</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=FlyGoat" title="Code">💻</a> <a href="#platform-FlyGoat" title="Packaging/porting to new platform">📦</a></td>
    <td align="center"><a href="https://github.com/ktgw0316"><img src="https://avatars.githubusercontent.com/u/3116763?v=4" width="100px;" alt=""/><br /><sub><b>Masahiro Kitagawa</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=ktgw0316" title="Code">💻</a> <a href="https://github.com/nemequ/simde/commits?author=ktgw0316" title="Tests">⚠️</a></td>
  </tr>
  <tr>
    <td align="center"><a href="https://github.com/operasfantom"><img src="https://avatars.githubusercontent.com/u/31903947?v=4" width="100px;" alt=""/><br /><sub><b>Pavel Iatchenii</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=operasfantom" title="Code">💻</a> <a href="https://github.com/nemequ/simde/commits?author=operasfantom" title="Tests">⚠️</a></td>
    <td align="center"><a href="https://tommyvct.tk"><img src="https://avatars.githubusercontent.com/u/7903172?v=4" width="100px;" alt=""/><br /><sub><b>Tommy Vercetti</b></sub></a><br /><a href="#maintenance-tommyvct" title="Maintenance">🚧</a></td>
    <td align="center"><a href="https://github.com/rscohn2"><img src="https://avatars.githubusercontent.com/u/1936730?v=4" width="100px;" alt=""/><br /><sub><b>Robert Cohn</b></sub></a><br /><a href="#maintenance-rscohn2" title="Maintenance">🚧</a></td>
    <td align="center"><a href="https://github.com/adamnovak"><img src="https://avatars.githubusercontent.com/u/5062495?v=4" width="100px;" alt=""/><br /><sub><b>Adam Novak</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=adamnovak" title="Documentation">📖</a></td>
    <td align="center"><a href="https://github.com/boris-kuz"><img src="https://avatars.githubusercontent.com/u/18011434?v=4" width="100px;" alt=""/><br /><sub><b>boris-kuz</b></sub></a><br /><a href="#maintenance-boris-kuz" title="Maintenance">🚧</a></td>
    <td align="center"><a href="https://github.com/Epixu"><img src="https://avatars.githubusercontent.com/u/1529601?v=4" width="100px;" alt=""/><br /><sub><b>Dimo Markov</b></sub></a><br /><a href="#maintenance-Epixu" title="Maintenance">🚧</a></td>
    <td align="center"><a href="https://dhbloo.github.io/"><img src="https://avatars.githubusercontent.com/u/32353216?v=4" width="100px;" alt=""/><br /><sub><b>dblue</b></sub></a><br /><a href="#maintenance-dhbloo" title="Maintenance">🚧</a></td>
  </tr>
  <tr>
    <td align="center"><a href="https://github.com/zekehul"><img src="https://avatars.githubusercontent.com/u/8780526?v=4" width="100px;" alt=""/><br /><sub><b>zekehul</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=zekehul" title="Code">💻</a> <a href="#maintenance-zekehul" title="Maintenance">🚧</a></td>
    <td align="center"><a href="https://open-cells.com"><img src="https://avatars.githubusercontent.com/u/40661368?v=4" width="100px;" alt=""/><br /><sub><b>Laurent Thomas</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=LaurentThomas" title="Code">💻</a></td>
    <td align="center"><a href="https://github.com/maxbachmann"><img src="https://avatars.githubusercontent.com/u/44199644?v=4" width="100px;" alt=""/><br /><sub><b>Max Bachmann</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=maxbachmann" title="Documentation">📖</a></td>
    <td align="center"><a href="https://github.com/psaab"><img src="https://avatars.githubusercontent.com/u/196946?v=4" width="100px;" alt=""/><br /><sub><b>psaab</b></sub></a><br /><a href="#maintenance-psaab" title="Maintenance">🚧</a></td>
    <td align="center"><a href="http://superduper.net/"><img src="https://avatars.githubusercontent.com/u/515813?v=4" width="100px;" alt=""/><br /><sub><b>Sam Clegg</b></sub></a><br /><a href="#maintenance-sbc100" title="Maintenance">🚧</a></td>
    <td align="center"><a href="https://twitter.com/tlively52"><img src="https://avatars.githubusercontent.com/u/7121787?v=4" width="100px;" alt=""/><br /><sub><b>Thomas Lively</b></sub></a><br /><a href="https://github.com/nemequ/simde/issues?q=author%3Atlively" title="Bug reports">🐛</a> <a href="#ideas-tlively" title="Ideas, Planning, & Feedback">🤔</a> <a href="#maintenance-tlively" title="Maintenance">🚧</a></td>
    <td align="center"><a href="http://blog.coderzh.com/"><img src="https://avatars.githubusercontent.com/u/329164?v=4" width="100px;" alt=""/><br /><sub><b>coderzh</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=coderzh" title="Code">💻</a> <a href="https://github.com/nemequ/simde/commits?author=coderzh" title="Tests">⚠️</a></td>
  </tr>
  <tr>
    <td align="center"><a href="https://github.com/k-dominik"><img src="https://avatars.githubusercontent.com/u/24434157?v=4" width="100px;" alt=""/><br /><sub><b>Dominik Kutra</b></sub></a><br /><a href="https://github.com/nemequ/simde/commits?author=k-dominik" title="Code">💻</a> <a href="https://github.com/nemequ/simde/commits?author=k-dominik" title="Tests">⚠️</a></td>
    <td align="center"><a href="https://github.com/Lithrein"><img src="https://avatars.githubusercontent.com/u/207981?v=4" width="100px;" alt=""/><br /><sub><b>Lithrein</b></sub></a><br /><a href="#maintenance-Lithrein" title="Maintenance">🚧</a></td>
    <td align="center"><a href="https://github.com/quyykk"><img src="https://avatars.githubusercontent.com/u/85879619?v=4" width="100px;" alt=""/><br /><sub><b>Nick</b></sub></a><br /><a href="#maintenance-quyykk" title="Maintenance">🚧</a></td>
    <td align="center"><a href="https://github.com/thomasdwu"><img src="https://avatars.githubusercontent.com/u/6819659?v=4" width="100px;" alt=""/><br /><sub><b>thomasdwu</b></sub></a><br /><a href="#maintenance-thomasdwu" title="Maintenance">🚧</a></td>
    <td align="center"><a href="https://github.com/Remnant44"><img src="https://avatars.githubusercontent.com/u/54461740?v=4" width="100px;" alt=""/><br /><sub><b>Stephen</b></sub></a><br /><a href="https://github.com/nemequ/simde/issues?q=author%3ARemnant44" title="Bug reports">🐛</a></td>
    <td align="center"><a href="https://github.com/johnplatts"><img src="https://avatars.githubusercontent.com/u/28941113?v=4" width="100px;" alt=""/><br /><sub><b>John Platts</b></sub></a><br /><a href="https://github.com/nemequ/simde/issues?q=author%3Ajohnplatts" title="Bug reports">🐛</a></td>
    <td align="center"><a href="https://github.com/tycho"><img src="https://avatars.githubusercontent.com/u/29616?v=4" width="100px;" alt=""/><br /><sub><b>Steven Noonan</b></sub></a><br /><a href="https://github.com/nemequ/simde/issues?q=author%3Atycho" title="Bug reports">🐛</a></td>
  </tr>
  <tr>
    <td align="center"><a href="https://github.com/p0nce"><img src="https://avatars.githubusercontent.com/u/1067485?v=4" width="100px;" alt=""/><br /><sub><b>p0nce </b></sub></a><br /><a href="https://github.com/nemequ/simde/issues?q=author%3Ap0nce" title="Bug reports">🐛</a></td>
    <td align="center"><a href="https://bonedaddy.net/pabs3/"><img src="https://avatars.githubusercontent.com/u/328602?v=4" width="100px;" alt=""/><br /><sub><b>Paul Wise</b></sub></a><br /><a href="https://github.com/nemequ/simde/issues?q=author%3Apabs3" title="Bug reports">🐛</a></td>
  </tr>
</table>

<!-- markdownlint-enable -->
<!-- prettier-ignore-end -->
<!-- ALL-CONTRIBUTORS-LIST:END -->

This project follows the [all-contributors](https://github.com/all-contributors/all-contributors) specification. Contributions of any kind are welcome!
