<div align="center">
  <h1><code>wasi</code></h1>

<strong>A <a href="https://bytecodealliance.org/">Bytecode Alliance</a> project</strong>

  <p>
    <strong>WASI API Bindings for Rust</strong>
  </p>

  <p>
    <a href="https://crates.io/crates/wasi"><img src="https://img.shields.io/crates/v/wasi.svg?style=flat-square" alt="Crates.io version" /></a>
    <a href="https://crates.io/crates/wasi"><img src="https://img.shields.io/crates/d/wasi.svg?style=flat-square" alt="Download" /></a>
    <a href="https://docs.rs/wasi/"><img src="https://img.shields.io/badge/docs-latest-blue.svg?style=flat-square" alt="docs.rs docs" /></a>
  </p>
</div>

This crate contains API bindings for [WASI](https://github.com/WebAssembly/WASI)
system calls in Rust, and currently reflects the `wasi_snapshot_preview1`
module. This crate is quite low-level and provides conceptually a "system call"
interface. In most settings, it's better to use the Rust standard library, which
has WASI support.

The `wasi` crate is also entirely procedurally generated from the `*.witx` files
describing the WASI apis. While some conveniences are provided the bindings here
are intentionally low-level!

# Usage

First you can depend on this crate via `Cargo.toml`:

```toml
[dependencies]
wasi = "0.8.0"
```

Next you can use the APIs in the root of the module like so:

```rust
fn main() {
    let stdout = 1;
    let message = "Hello, World!\n";
    let data = [wasi::Ciovec {
        buf: message.as_ptr(),
        buf_len: message.len(),
    }];
    wasi::fd_write(stdout, &data).unwrap();
}
```

Next you can use a tool like [`cargo
wasi`](https://github.com/bytecodealliance/cargo-wasi) to compile and run your
project:

To compile Rust projects to wasm using WASI, use the `wasm32-wasi` target,
like this:

```
$ cargo wasi run
   Compiling wasi v0.8.0+wasi-snapshot-preview1
   Compiling wut v0.1.0 (/code)
    Finished dev [unoptimized + debuginfo] target(s) in 0.34s
     Running `/.cargo/bin/cargo-wasi target/wasm32-wasi/debug/wut.wasm`
     Running `target/wasm32-wasi/debug/wut.wasm`
Hello, World!
```

# License

This project is licensed under the Apache 2.0 license with the LLVM exception.
See [LICENSE](LICENSE) for more details.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this project by you, as defined in the Apache-2.0 license,
shall be licensed as above, without any additional terms or conditions.
