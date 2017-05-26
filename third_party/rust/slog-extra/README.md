# slog-extra - Standard [slog-rs] extensions

<p align="center">
  <a href="https://travis-ci.org/slog-rs/std">
      <img src="https://img.shields.io/travis/slog-rs/std/master.svg" alt="Travis CI Build Status">
  </a>

  <a href="https://crates.io/crates/slog-std">
      <img src="https://img.shields.io/crates/d/slog-std.svg" alt="slog-std on crates.io">
  </a>

  <a href="https://gitter.im/dpc/slog-std">
      <img src="https://img.shields.io/gitter/room/dpc/slog-rs.svg" alt="slog-rs Gitter Chat">
  </a>
</p>

[slog-rs]: //github.com/slog-rs/core

This crates contains slog-rs extensions that don't belong to the core `slog` crate due to:

* not supporting `no_std`
* having additional dependencies
* other problems
