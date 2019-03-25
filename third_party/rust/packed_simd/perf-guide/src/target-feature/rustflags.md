# Using RUSTFLAGS

One of the easiest ways to benefit from SIMD is to allow the compiler
to generate code using certain vector instruction extensions.

The environment variable `RUSTFLAGS` can be used to pass options for code
generation to the Rust compiler. These flags will affect **all** compiled crates.

There are two flags which can be used to enable specific vector extensions:

## target-feature

- Syntax: `-C target-feature=<features>`

- Provides the compiler with a comma-separated set of instruction extensions
  to enable.

  **Example**: Use `-C target-features=+sse3,+avx` to enable generating instructions
  for [Streaming SIMD Extensions 3](https://en.wikipedia.org/wiki/SSE3) and
  [Advanced Vector Extensions](https://en.wikipedia.org/wiki/Advanced_Vector_Extensions).

- To list target triples for all targets supported by Rust, use:

  ```sh
  rustc --print target-list
  ```

- To list all support target features for a certain target triple, use:

  ```sh
  rustc --target=${TRIPLE} --print target-features
  ```

- Note that all CPU features are independent, and will have to be enabled individually.

  **Example**: Setting `-C target-features=+avx2` will _not_ enable `fma`, even though
  all CPUs which support AVX2 also support FMA. To enable both, one has to use
  `-C target-features=+avx2,+fma`

- Some features also depend on other features, which need to be enabled for the
  target instructions to be generated.

  **Example**: Unless `v7` is specified as the target CPU (see below), to enable
  NEON on ARM it is necessary to use `-C target-feature=+v7,+neon`.

## target-cpu

- Syntax: `-C target-cpu=<cpu>`

- Sets the identifier of a CPU family / model for which to build and optimize the code.

  **Example**: `RUSTFLAGS='-C target-cpu=cortex-a75'`

- To list all supported target CPUs for a certain target triple, use:

  ```sh
  rustc --target=${TRIPLE} --print target-cpus
  ```

  **Example**:

  ```sh
  rustc --target=i686-pc-windows-msvc --print target-cpus
  ```

- The compiler will translate this into a list of target features. Therefore,
  individual feature checks (`#[cfg(target_feature = "...")]`) will still
  work properly.

- It will cause the code generator to optimize the generated code for that
  specific CPU model.

- Using `native` as the CPU model will cause Rust to generate and optimize code
  for the CPU running the compiler. It is useful when building programs which you
  plan to only use locally. This should never be used when the generated programs
  are meant to be run on other computers, such as when packaging for distribution
  or cross-compiling.
