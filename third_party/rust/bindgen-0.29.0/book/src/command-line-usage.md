# Command Line Usage

Install the `bindgen` executable with `cargo`:

```bash
$ cargo install bindgen
```

The `bindgen` executable is installed to `~/.cargo/bin`. You have to add that
directory to your `$PATH` to use `bindgen`.

`bindgen` takes the path to an input C or C++ header file, and optionally an
output file path for the generated bindings. If the output file path is not
supplied, the bindings are printed to `stdout`.

If we wanted to generated Rust FFI bindings from a C header named `input.h` and
put them in the `bindings.rs` file, we would invoke `bindgen` like this:

```bash
$ bindgen input.h -o bindings.rs
```

For more details, pass the `--help` flag:

```bash
$ bindgen --help
```
