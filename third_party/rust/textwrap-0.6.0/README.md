# Textwrap

[![](https://img.shields.io/crates/v/textwrap.svg)][crates-io]
[![](https://docs.rs/textwrap/badge.svg)][api-docs]
[![](https://travis-ci.org/mgeisler/textwrap.svg?branch=master)][travis-ci]
[![](https://ci.appveyor.com/api/projects/status/yo6iak55nraupjw3/branch/master?svg=true)][appveyor]

Textwrap is a small Rust crate for word wrapping text. You can use it
to format strings for display in commandline applications. The crate
name and interface is inspired by
the [Python textwrap module][py-textwrap].

## Usage

Add this to your `Cargo.toml`:
```toml
[dependencies]
textwrap = "0.6"
```

and this to your crate root:
```rust
extern crate textwrap;
```

If you would like to have automatic hyphenation, specify the
dependency as:
```toml
[dependencies]
textwrap = { version: "0.6", features: ["hyphenation"] }
```

## Documentation

**[API documentation][api-docs]**

## Getting Started

Word wrapping single strings is easy using the `fill` function:
```rust
extern crate textwrap;
use textwrap::fill;

fn main() {
    let text = "textwrap: a small library for wrapping text.";
    println!("{}", fill(text, 18));
}
```
The output is
```
textwrap: a small
library for
wrapping text.
```

With the `hyphenation` feature, you can get automatic hyphenation
for [about 70 languages][patterns]. Your program must load and
configure the hyphenation patterns to use:
```rust
extern crate hyphenation;
extern crate textwrap;

use hyphenation::Language;
use textwrap::Wrapper;

fn main() {
    let corpus = hyphenation::load(Language::English_US).unwrap();
    let wrapper = Wrapper::new(18).word_splitter(Box::new(corpus));
    let text = "textwrap: a small library for wrapping text.";
    println!("{}", wrapper.fill(text))
}
```

The output now looks like this:
```
textwrap: a small
library for wrap-
ping text.
```

The hyphenation uses high-quality TeX hyphenation patterns.

## Examples

The library comes with a small example programs that shows various
features.

### Layout Example

The `layout` example shows how a fixed example string is wrapped at
different widths. Run the example with:

```shell
$ cargo run --features hyphenation --example layout
```

The program will use the following string:

> Memory safety without garbage collection. Concurrency without data
> races. Zero-cost abstractions.

The string is wrapped at all widths between 15 and 60 columns. With
narrow columns the output looks like this:

```
.--- Width: 15 ---.
| Memory safety   |
| without garbage |
| collection.     |
| Concurrency     |
| without data    |
| races. Zero-    |
| cost abstrac-   |
| tions.          |
.--- Width: 16 ----.
| Memory safety    |
| without garbage  |
| collection. Con- |
| currency without |
| data races. Ze-  |
| ro-cost abstrac- |
| tions.           |
```

Later, longer lines are used and the output now looks like this:

```
.-------------------- Width: 49 --------------------.
| Memory safety without garbage collection. Concur- |
| rency without data races. Zero-cost abstractions. |
.---------------------- Width: 53 ----------------------.
| Memory safety without garbage collection. Concurrency |
| without data races. Zero-cost abstractions.           |
.------------------------- Width: 59 -------------------------.
| Memory safety without garbage collection. Concurrency with- |
| out data races. Zero-cost abstractions.                     |
```

Notice how words are split at hyphens (such as "zero-cost") but also
how words are hyphenated using automatic/machine hyphenation.

### Terminal Width Example

The `termwidth` example simply shows how the width can be set
automatically to the current terminal width. Run it with this command:

```
$ cargo run --example termwidth
```

If you run it in a narrow terminal, you'll see output like this:
```
Formatted in within 60 columns:
----
Memory safety without garbage collection. Concurrency
without data races. Zero-cost abstractions.
----
```

If `stdout` is not connected to the terminal, the program will use a
default of 80 columns for the width:

```
$ cargo run --example termwidth | cat
Formatted in within 80 columns:
----
Memory safety without garbage collection. Concurrency without data races. Zero-
cost abstractions.
----
```

## Release History

This section lists the largest changes per release.

### Version 0.6.0 — May 22nd, 2017

Version 0.6.0 adds builder methods to `Wrapper` for easy one-line
initialization and configuration:

```rust
let wrapper = Wrapper::new(60).break_words(false);
```

It also add a new `NoHyphenation` word splitter that will never split
words, not even at existing hyphens.

Issues closed:

* Fixed [#28][issue-28]: Support not squeezing whitespace

### Version 0.5.0 — May 15th, 2017

Version 0.5.0 has *breaking API changes*. However, this only affects
code using the hyphenation feature. The feature is now optional, so
you will first need to enable the `hyphenation` feature as described
above. Afterwards, please change your code from
```rust
wrapper.corpus = Some(&corpus);
```
to
```rust
wrapper.splitter = corpus;
```

Other changes include optimizations, so version 0.5.0 is roughly
10-15% faster than version 0.4.0.

Issues closed:

* Fixed [#19][issue-19]: Add support for finding terminal size
* Fixed [#25][issue-25]: Handle words longer than `self.width`
* Fixed [#26][issue-26]: Support custom indentation
* Fixed [#36][issue-36]: Support building without `hyphenation`
* Fixed [#39][issue-39]: Respect non-breaking spaces

### Version 0.4.0 — January 24th, 2017

Documented complexities and tested these via `cargo bench`.

* Fixed [#13][issue-13]: Immediatedly add word if it fits
* Fixed [#14][issue-14]: Avoid splitting on initial hyphens in `--foo-bar`

### Version 0.3.0 — January 7th, 2017

Added support for automatic hyphenation.

### Version 0.2.0 — December 28th, 2016

Introduced `Wrapper` struct. Added support for wrapping on hyphens.

### Version 0.1.0 — December 17th, 2016

First public release with support for wrapping strings on whitespace.

## License

Textwrap can be distributed according to the [MIT license][mit].
Contributions will be accepted under the same license.

[crates-io]: https://crates.io/crates/textwrap
[travis-ci]: https://travis-ci.org/mgeisler/textwrap
[appveyor]: https://ci.appveyor.com/project/mgeisler/textwrap
[py-textwrap]: https://docs.python.org/library/textwrap
[patterns]: https://github.com/tapeinosyne/hyphenation/tree/master/patterns-tex
[api-docs]: https://docs.rs/textwrap/
[issue-13]: ../../issues/13
[issue-14]: ../../issues/14
[issue-19]: ../../issues/19
[issue-25]: ../../issues/25
[issue-26]: ../../issues/26
[issue-28]: ../../issues/28
[issue-36]: ../../issues/36
[issue-39]: ../../issues/39
[mit]: LICENSE
