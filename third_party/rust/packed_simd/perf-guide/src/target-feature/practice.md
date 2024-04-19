# Target features in practice

Using `RUSTFLAGS` will allow the crate being compiled, as well as all its
transitive dependencies to use certain target features.

A tehnique used to avoid undefined behavior at runtime is to compile and
ship multiple binaries, each compiled with a certain set of features.
This might not be feasible in some cases, and can quickly get out of hand
as more and more vector extensions are added to an architecture.

Rust can be more flexible: you can build a single binary/library which automatically
picks the best supported vector instructions depending on the host machine.
The trick consists of monomorphizing parts of the code during building, and then
using run-time feature detection to select the right code path when running.

<!-- TODO
Explain how to create efficient functions that dispatch to different
implementations at run-time without issues (e.g. using `#[inline(always)]` for
the impls, wrapping in `#[target_feature]`, and the wrapping those in a function
that does run-time feature detection).
-->

**NOTE** (x86 specific): because the AVX (256-bit) registers extend the existing
SSE (128-bit) registers, mixing SSE and AVX instructions in a program can cause
performance issues.

The solution is to compile all code, even the code written with 128-bit vectors,
with the AVX target feature enabled. This will cause the compiler to prefix the
generated instructions with the [VEX] prefix.

[VEX]: https://en.wikipedia.org/wiki/VEX_prefix
