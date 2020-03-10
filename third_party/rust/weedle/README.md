<div align="center">

  <h1>Weedle</h1>

  <strong>A Web IDL parser</strong>

  <p>
    <a href="https://travis-ci.org/rustwasm/weedle"><img src="https://img.shields.io/travis/rustwasm/weedle.svg?style=flat-square" alt="Build Status" /></a>
    <a href="https://crates.io/crates/weedle"><img src="https://img.shields.io/crates/v/weedle.svg?style=flat-square" alt="Crates.io version" /></a>
    <a href="https://crates.io/crates/weedle"><img src="https://img.shields.io/crates/d/weedle.svg?style=flat-square" alt="Download" /></a>
    <a href="https://docs.rs/weedle"><img src="https://img.shields.io/badge/docs-latest-blue.svg?style=flat-square" alt="docs.rs docs" /></a>
  </p>

  <h3>
    <a href="https://docs.rs/weedle">API Docs</a>
    <span> | </span>
    <a href="https://discordapp.com/channels/442252698964721669/443151097398296587">Chat</a>
  </h3>

  <sub>Built with 🦀🕸 by <a href="https://rustwasm.github.io/">The Rust and WebAssembly Working Group</a></sub>
</div>

## About

Parses valid WebIDL definitions & produces a data structure starting from
[`Definitions`](https://docs.rs/weedle/latest/weedle/type.Definitions.html).

## Usage

### `Cargo.toml`

```toml
[dependencies]
weedle = "0.9.0"
```

### `src/main.rs`

```rust
fn main() {
    let parsed = weedle::parse("
        interface Window {
            readonly attribute Storage sessionStorage;
        };
    ").unwrap();

    println!("{:?}", parsed);
}
```
