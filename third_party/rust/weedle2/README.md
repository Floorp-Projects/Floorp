<div align="center">

  <h1>Weedle 2 - Electric Boogaloo</h1>

  <strong>A Web IDL parser</strong>

  <p>
    <a href="https://crates.io/crates/weedle2"><img src="https://img.shields.io/crates/v/weedle2.svg?style=flat-square" alt="Crates.io version" /></a>
    <a href="https://docs.rs/weedle2"><img src="https://img.shields.io/badge/docs-latest-blue.svg?style=flat-square" alt="Documentation" /></a>
    <a href="LICENSE"><img src="https://img.shields.io/crates/l/weedle2/2.0.0?style=flat-square" alt="MIT License" /></a>
  </p>

  <sub>
  Built with 🦀🕸 by <a href="https://rustwasm.github.io/">The Rust and WebAssembly Working Group</a>.
  <br>
  Forked to extend the functionality beyond WebIDL needs.
  </sub>
</div>

## About

Parses valid WebIDL definitions & produces a data structure starting from
[`Definitions`](https://docs.rs/weedle/latest/weedle/type.Definitions.html).

## Usage

### `Cargo.toml`

```toml
[dependencies]
weedle2 = "5.0.0"
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
