<p align="center">

  <a href="https://github.com/slog-rs/slog">
  <img src="https://cdn.rawgit.com/slog-rs/misc/master/media/slog.svg" alt="slog-rs logo">
  </a>
  <br>

  <a href="https://travis-ci.org/slog-rs/atomic">
      <img src="https://img.shields.io/travis/slog-rs/atomic/master.svg" alt="Travis CI Build Status">
  </a>

  <a href="https://crates.io/crates/slog-atomic">
      <img src="https://img.shields.io/crates/d/slog-atomic.svg" alt="slog-atomic on crates.io">
  </a>

  <a href="https://gitter.im/slog-rs/slog">
      <img src="https://img.shields.io/gitter/room/slog-rs/slog.svg" alt="slog-rs Gitter Chat">
  </a>
</p>

# slog-atomic - Atomic run-time controllable drain for [slog-rs]

[slog-rs]: //github.com/slog-rs/slog

Using `slog-atomic` you can create a `slog::Drain` that can change behaviour
in a thread-safe way, in runtime. This is useful eg. for triggering different
logging levels from a signal handler.
