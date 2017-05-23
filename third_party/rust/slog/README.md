<p align="center">

  <a href="https://github.com/slog-rs/slog">
  <img src="https://cdn.rawgit.com/slog-rs/misc/master/media/slog.svg" alt="slog-rs logo">
  </a>
  <br>

  <a href="https://travis-ci.org/slog-rs/slog">
      <img src="https://img.shields.io/travis/slog-rs/slog/master.svg" alt="Travis CI Build Status">
  </a>

  <a href="https://crates.io/crates/slog">
      <img src="https://img.shields.io/crates/d/slog.svg" alt="slog-rs on crates.io">
  </a>

  <a href="https://gitter.im/slog-rs/slog">
      <img src="https://img.shields.io/gitter/room/slog-rs/slog.svg" alt="slog-rs Gitter Chat">
  </a>

  <a href="https://docs.rs/releases/search?query=slog-">
      <img src="https://docs.rs/slog/badge.svg" alt="docs-rs: release versions documentation">
  </a>
  <br>
    <strong><a href="https://github.com/slog-rs/slog/wiki/Getting-started">Getting started</a></strong>

  <a href="//github.com/slog-rs/slog/wiki/Introduction-to-structured-logging-with-slog">Introduction</a>

  <a href="//github.com/slog-rs/slog/wiki/FAQ">FAQ</a>
  <br>
  <a href="https://crates.io/search?q=slog">Crate list</a>
</p>

# slog-rs - The Logging for [Rust][rust]

### Table of Contents

* [Status & news](#status--news)
* [`slog` crate](#slog-crate)
  * [Features](#features)
  * [Advantages over log crate](#advantages-over-log-crate)
  * [Terminal output example](#terminal-output-example)
  * [Using & help](#using--help)
  * [Compatibility Policy](#compatibility-policy)
* [Slog community](#slog-community)
  * [Overview](#overview)
* [Slog related resources](#slog-related-resources)

### Status & news

`slog` is an ecosystem of reusable components for structured, extensible,
composable logging for [Rust][rust].

The ambition is to be The Logging Framework for Rust. `slog` should accommodate
variety of logging features and requirements.

### Features & technical documentation

Most of interesting documentation is using rustdoc itself.

You can view on [docs.rs/slog](https://docs.rs/slog/1/)

### Terminal output example

`slog-term` is only one of many `slog` features - useful showcase.

Automatic TTY detection and colors:

![slog-rs terminal full-format output](http://i.imgur.com/IUe80gU.png)

Compact vs full mode:

![slog-rs terminal compact output](http://i.imgur.com/P9u2sWP.png)
![slog-rs terminal full output](http://i.imgur.com/ENiy5H9.png)


## Using & help

See
[examples/features.rs](https://github.com/slog-rs/misc/blob/master/examples/features.rs)
for full quick code example overview.

See [faq] for answers to common questions and [wiki] for other documentation
articles. If you want to say hi, or need help use [slog-rs gitter] channel.

Read [Documentation](https://docs.rs/slog/) for details and features.

To report a bug or ask for features use [github issues][issues].

[faq]: https://github.com/slog-rs/slog/wiki/FAQ
[wiki]: https://github.com/slog-rs/slog/wiki/
[rust]: http://rust-lang.org
[slog-rs gitter]: https://gitter.im/slog-rs/slog
[issues]: //github.com/slog-rs/slog/issues

#### In your project

In Cargo.toml:

```
[dependencies]
slog = "1.2"
```

In your `main.rs`:

```
#[macro_use]
extern crate slog;
```

### Compatibility Policy

`slog` follows SemVer: this is the official policy regarding breaking changes
and minimum required versions of Rust.

Slog crates should pin minimum required version of Rust to the CI builds.
Bumping the minimum version of Rust is considered a minor breaking change,
meaning *at a minimum* the minor version will be bumped.

In order to keep from being surprised of breaking changes, it is **highly**
recommended to use the `~major.minor.patch` style in your `Cargo.toml` if you
wish to target a version of Rust that is *older* than current stable minus two
releases:

```toml
[dependencies]
slog = "~1.3.0"
```

This will cause *only* the patch version to be updated upon a `cargo update`
call, and therefore cannot break due to new features, or bumped minimum
versions of Rust.

#### Minimum Version of Rust

`slog` and it's ecosystem officially supports current stable Rust, minus
two releases, but may work with prior releases as well. For example, current
stable Rust at the time of this writing is 1.13.0, meaning `slog` is guaranteed
to compile with 1.11.0 and beyond.  At the 1.14.0 release, `slog` will be
guaranteed to compile with 1.12.0 and beyond, etc.

Upon bumping the minimum version of Rust (assuming it's within the stable-2
range), it *must* be clearly annotated in the `CHANGELOG.md`


## Slog community

### Overview

Slog related crates are hosted under [slog github
organization](https://github.com/slog-rs).

Dawid Ciężarkiewicz is the original author and current maintainer of `slog` and
therefore self-appointed benevolent dictator over the project. When working on
slog Dawid follows and expects everyone to follow his [Code of
Conduct](https://github.com/dpc/public/blob/master/COC.md).

Any particular repositories under slog ecosystem might be created, controlled,
maintained by other entities with various level of autonomy. Lets work together
toward a common goal in respectful and welcoming atmosphere!

## slog-related articles

* [24 days of Rust - structured logging](https://siciarz.net/24-days-rust-structured-logging/) - review and tutorial by Zbigniew Siciarz 2016-12-05
