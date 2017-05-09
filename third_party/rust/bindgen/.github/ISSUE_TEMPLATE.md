### Input C/C++ Header

```C++
// Insert your (minimal) C/C++ header here.
```

### Bindgen Invokation

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

### `RUST_LOG=bindgen` Output

<details>

```
Insert debug logging when running bindgen with the `RUST_LOG=bindgen` environment
variable set.
```

</details>
