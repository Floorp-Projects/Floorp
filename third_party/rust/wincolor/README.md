wincolor
========
A simple Windows specific API for controlling text color in a Windows console.
The purpose of this crate is to expose the full inflexibility of the Windows
console without any platform independent abstraction.

[![Windows build status](https://ci.appveyor.com/api/projects/status/github/BurntSushi/ripgrep?svg=true)](https://ci.appveyor.com/project/BurntSushi/ripgrep)
[![](https://img.shields.io/crates/v/wincolor.svg)](https://crates.io/crates/wincolor)

Dual-licensed under MIT or the [UNLICENSE](http://unlicense.org).

### Documentation

[https://docs.rs/wincolor](https://docs.rs/wincolor)

### Usage

Add this to your `Cargo.toml`:

```toml
[dependencies]
wincolor = "0.1"
```

and this to your crate root:

```rust
extern crate wincolor;
```

### Example

This is a simple example that shows how to write text with a foreground color
of cyan and the intense attribute set:

```rust
use wincolor::{Console, Color, Intense};

let mut con = Console::stdout().unwrap();
con.fg(Intense::Yes, Color::Cyan).unwrap();
println!("This text will be intense cyan.");
con.reset().unwrap();
println!("This text will be normal.");
```
