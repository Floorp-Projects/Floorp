<!-- Thanks for filing a bindgen issue! We appreciate it :-) -->

### Input C/C++ Header

```C++
// Insert your minimal C or C++ header here.
//
// It should *NOT* have any `#include`s! Not all systems have the same header
// files, and therefore any `#include` harms reproducibility. Additionally,
// the test case isn't minimal since the included file almost assuredly
// contains things that aren't necessary to reproduce the bug, and makes
// tracking it down much more difficult.
//
// Use the `--dump-preprocessed-input` flag or the
// `bindgen::Builder::dump_preprocessed_input` method to make your test case
// standalone and without `#include`s, and then use C-Reduce to minimize it:
// https://github.com/rust-lang-nursery/rust-bindgen/blob/master/CONTRIBUTING.md#using-creduce-to-minimize-test-cases
```

### Bindgen Invocation

<!-- Place either the `bindgen::Builder` or the command line flags used here. -->

```Rust
bindgen::Builder::default()
    .header("input.h")
    .generate()
    .unwrap()
```

or

```
$ bindgen input.h --whatever --flags
```

### Actual Results

```
Insert panic message and backtrace (set the `RUST_BACKTRACE=1` env var) here.
```

and/or

```rust
// Insert the (incorrect/buggy) generated bindings here
```

and/or

```
Insert compilation errors generated when compiling the bindings with rustc here
```

### Expected Results

<!--
Replace this with a description of what you expected instead of the actual
results. The more precise, the better! For example, if a struct in the generated
bindings is missing a field that exists in the C/C++ struct, note that here.
-->
