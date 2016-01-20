# Wires [![Build Status](https://travis-ci.org/jgraham/wires.svg?branch=master)](https://travis-ci.org/jgraham/wires)

WebDriver <-> Marionette proxy

## Build Project

Download rust from [rust-lang.org](https://www.rust-lang.org/)

To build the project for release:

```bash
cargo build --no-default-features --release
```

If you want to build a debug version just use:

```bash
cargo build --no-default-features
```
The `--no-default-features` argument is required to compile on Windows due to the
 way dependencies need to be compiled.
## Usage

To use wires, follow the steps on [MDN](https://developer.mozilla.org/en-US/docs/Mozilla/QA/Marionette/WebDriver) or you can use the steps below and use a cURL client.

```
cargo run [options] [--] [<args>...]
```

For example, you can specify a binary path to Firefox and run the proxy:

```
cargo run -- -b /Applications/FirefoxNightly.app/Contents/MacOS/firefox-bin
```
