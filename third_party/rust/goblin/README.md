# libgoblin [![Build status][travis-badge]][travis-url] [![crates.io version][crates-goblin-badge]][crates-goblin]

<!-- Badges' links -->

[travis-badge]: https://travis-ci.org/m4b/goblin.svg?branch=master
[travis-url]: https://travis-ci.org/m4b/goblin
[crates-goblin-badge]: https://img.shields.io/crates/v/goblin.svg
[crates-goblin]: https://crates.io/crates/goblin

![say the right words](https://s-media-cache-ak0.pinimg.com/736x/1b/6a/aa/1b6aaa2bae005e2fed84b1a7c32ecb1b.jpg)

### Documentation

https://docs.rs/goblin/

[changelog](CHANGELOG.md)

### Usage

Goblin requires `rustc` 1.36.0.

Add to your `Cargo.toml`

```toml
[dependencies]
goblin = "0.1"
```

### Features

* awesome crate name
* zero-copy, cross-platform, endian-aware, ELF64/32 implementation - wow!
* zero-copy, cross-platform, endian-aware, 32/64 bit Mach-o parser - zoiks!
* PE 32/64-bit parser - bing!
* a Unix _and_ BSD style archive parser (latter courtesy of [@willglynn]) - huzzah!
* many cfg options - it will make your head spin, and make you angry when reading the source!
* fuzzed - "I am happy to report that goblin withstood 100 million fuzzing runs, 1 million runs
  each for seed 1\~100." - [@sanxiyn]
* tests

`libgoblin` aims to be your one-stop shop for binary parsing, loading, and analysis.

### Use-cases

Goblin primarily supports the following important use cases:

1. Core, std-free `#[repr(C)]` structs, tiny compile time, 32/64 (or both) at your leisure.

1. Type punning. Define a function once on a type, but have it work on 32 or 64-bit variants -
   without really changing anything, and no macros! See `examples/automagic.rs` for a basic example.

1. `std` mode. This throws in read and write impls via `Pread` and `Pwrite`, reading from file,
   convenience allocations, extra methods, etc. This is for clients who can allocate and want to
   read binaries off disk.

1. `Endian_fd`. A truly terrible name :laughing: this is for binary analysis like in [panopticon]
   or [falcon] which needs to read binaries of foreign endianness, _or_ as a basis for
   constructing cross platform foreign architecture binutils, e.g. [cargo-sym] and [bingrep] are
   simple examples of this, but the sky is the limit.

Here are some things you could do with this crate (or help to implement so they could be done):

1. Write a compiler and use it to [generate binaries][faerie] (all the raw C structs have
   [`Pwrite`][scroll] derived).
1. Write a binary analysis tool which loads, parses, and analyzes various binary formats, e.g.,
   [panopticon] or [falcon].
1. Write a [semi-functioning dynamic linker][dryad].
1. Write a [kernel][redox-os] and load binaries using `no_std` cfg. I.e., it is essentially just
   struct and const defs (like a C header) - no fd, no output, no std.
1. Write a [bin2json] tool, because why shouldn't binary formats be in JSON?

<!-- Related projects  -->

[cargo-sym]: https://github.com/m4b/cargo-sym
[bingrep]: https://github.com/m4b/bingrep
[faerie]: https://github.com/m4b/faerie
[dryad]: https://github.com/m4b/dryad
[scroll]: https://github.com/m4b/scroll
[redox-os]: https://github.com/redox-os/redox
[bin2json]: https://github.com/m4b/bin2json
[panopticon]: https://github.com/das-labor/panopticon
[falcon]: https://github.com/endeav0r/falcon

### Cfgs

`libgoblin` is designed to be massively configurable. The current flags are:

* elf64 - 64-bit elf binaries, `repr(C)` struct defs
* elf32 - 32-bit elf binaries, `repr(C)` struct defs
* mach64 - 64-bit mach-o `repr(C)` struct defs
* mach32 - 32-bit mach-o `repr(C)` struct defs
* pe32 - 32-bit PE `repr(C)` struct defs
* pe64 - 64-bit PE `repr(C)` struct defs
* archive - a Unix Archive parser
* endian_fd - parses according to the endianness in the binary
* std - to allow `no_std` environments

# Contributors

Thank you all :heart: !

In lexicographic order:

- [@amanieu]
- [@burjui]
- [@flanfly]
- [@ibabushkin]
- [@jan-auer]
- [@jdub]
- [@jrmuizel]
- [@jsgf]
- [@kjempelodott]
- [@le-jzr]
- [@lion128]
- [@llogiq]
- [@lzutao]
- [@lzybkr]
- [@m4b]
- [@mitsuhiko]
- [@mre]
- [@pchickey]
- [@philipc]
- [@Pzixel]
- [@raindev]
- [@rocallahan]
- [@sanxiyn]
- [@tathanhdinh]
- [@Techno-coder]
- [@ticki]
- [@wickerwacka]
- [@willglynn]
- [@wyxloading]
- [@xcoldhandsx]

<!-- Contributors -->

[@amanieu]: https://github.com/amanieu
[@burjui]: https://github.com/burjui
[@flanfly]: https://github.com/flanfly
[@ibabushkin]: https://github.com/ibabushkin
[@jan-auer]: https://github.com/jan-auer
[@jdub]: https://github.com/jdub
[@jrmuizel]: https://github.com/jrmuizel
[@jsgf]: https://github.com/jsgf
[@kjempelodott]: https://github.com/kjempelodott
[@le-jzr]: https://github.com/le-jzr
[@lion128]: https://github.com/lion128
[@llogiq]: https://github.com/llogiq
[@lzutao]: https://github.com/lzutao
[@lzybkr]: https://github.com/lzybkr
[@m4b]: https://github.com/m4b
[@mitsuhiko]: https://github.com/mitsuhiko
[@mre]: https://github.com/mre
[@pchickey]: https://github.com/pchickey
[@philipc]: https://github.com/philipc
[@Pzixel]: https://github.com/Pzixel
[@raindev]: https://github.com/raindev
[@rocallahan]: https://github.com/rocallahan
[@sanxiyn]: https://github.com/sanxiyn
[@tathanhdinh]: https://github.com/tathanhdinh
[@Techno-coder]: https://github.com/Techno-coder
[@ticki]: https://github.com/ticki
[@wickerwacka]: https://github.com/wickerwaka
[@willglynn]: https://github.com/willglynn
[@wyxloading]: https://github.com/wyxloading
[@xcoldhandsx]: https://github.com/xcoldhandsx

## Contributing

1. Please prefix commits with the affected binary component; the more specific the better, e.g.,
   if you only modify relocations in the elf module, then do "elf.reloc: added new constants for Z80"
1. Commit messages must explain their change, no generic "changed", or "fix"; if you push commits
   like this on a PR, be aware [@m4b] or someone will most likely squash them.
1. If you are making a large change to a module, please raise an issue first and lets discuss;
   I don't want to waste your time if its not a good technical direction, or etc.
1. If your PR is not getting attention, please respond to all relevant comments raised on the PR,
   and if still no response, ping [@m4b], [@philipc], or [@willglynn] in github and also feel free
   to email [@m4b].
1. Please add tests if you are adding a new feature. Feel free to add tests even if you are not,
   tests are awesome and easy in rust.
1. Once cargo format is officially released, please format your _patch_ using the default settings.
