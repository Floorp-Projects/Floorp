<div align="center">
  <h1><code>wat</code></h1>

<strong>A <a href="https://bytecodealliance.org/">Bytecode Alliance</a> project</strong>

  <p>
    <strong>A Rust parser for the <a href="https://webassembly.github.io/spec/core/text/index.html">WebAssembly Text Format (WAT)</a>.</strong>
  </p>

  <p>
    <a href="https://crates.io/crates/wat"><img src="https://img.shields.io/crates/v/wat.svg?style=flat-square" alt="Crates.io version" /></a>
    <a href="https://crates.io/crates/wat"><img src="https://img.shields.io/crates/d/wat.svg?style=flat-square" alt="Download" /></a>
    <a href="https://docs.rs/wat/"><img src="https://img.shields.io/static/v1?label=docs&message=wat&color=blue&style=flat-square" alt="docs.rs docs" /></a>
    <a href="https://docs.rs/wast/"><img src="https://img.shields.io/static/v1?label=docs&message=wast&color=blue&style=flat-square" alt="docs.rs docs" /></a>
  </p>
</div>

## Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
wat = "1.0"
```

And then you can parse WAT to binary WebAssembly via:

```rust
// Parse from a file ...
let binary = wat::parse_file("./foo.wat")?;

// ... or a string
let wat = r#"
    (module
        (func $foo)

        (func (export "bar")
            call $foo
        )
    )
"#;
let binary = wat::parse_str(wat)?;
```

## Low-level parsing

This repository and project also aims to provide low-level parsing support for
the WAT and WAST formats. Effectively, if you've got an s-expression lookalike
that you'd like to parse, you should be able to parse it!

The `wat` crate does not support this because it strives to provide strong
API-level stability guarantees, but the `wast` crate has all the
low-level details and is the implementation of the `wast` crate. Be sure to
[check out its `README.md`](../wast/README.md) for more information.

## Stability and WebAssembly Features

Consult the [crate documentation](https://docs.rs/wat) for more information,
but the general idea is this crate will not issue a semver-breaking change for
breaking changes in the WAT format, either for MVP features or post-MVP
features. No opt-in is required to use WebAssembly features, so using them may
break if the upstream spec changes.

# License

This project is licensed under the Apache 2.0 license with the LLVM exception.
See [LICENSE](LICENSE) for more details.

### Contribution

Unless you explicitly state otherwise, any contribution intentionally submitted
for inclusion in this project by you, as defined in the Apache-2.0 license,
shall be licensed as above, without any additional terms or conditions.
