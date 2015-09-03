wires
================

> WebDriver <-> Marionette proxy

## Build Project

Download rust from [rust-lang.org](https://www.rust-lang.org/)

To build the project:

```
cargo build
```

## Usage

```
cargo run [options] [--] [<args>...]
```

For example, you can specify a binary path to Firefox and run the proxy:

```
cargo run -- -b /Applications/FirefoxNightly.app/Contents/MacOS/firefox-bin
```
