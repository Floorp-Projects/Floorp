# midir [![crates.io](https://img.shields.io/crates/v/midir.svg)](https://crates.io/crates/midir) [![Build Status](https://dev.azure.com/Boddlnagg/midir/_apis/build/status/Boddlnagg.midir?branchName=master)](https://dev.azure.com/Boddlnagg/midir/_build/latest?definitionId=1)

Cross-platform, realtime MIDI processing in Rust. This is a friendly fork with
small changes required for vendoring the crate in Firefox. It will go away as
soon as we will be able to vendor the upstream crate.

## Features
**midir** is inspired by [RtMidi](https://github.com/thestk/rtmidi) and supports the same features*, including virtual ports (except on Windows) and full SysEx support – but with a rust-y API!

<sup>* With the exception of message queues, but these can be implemented on top of callbacks using e.g. Rust's channels.</sup>

**midir** currently supports the following platforms/backends: 
- [x] ALSA (Linux)
- [x] WinMM (Windows)
- [x] CoreMIDI (macOS, iOS (untested))
- [x] WinRT (Windows 8+), enable the `winrt` feature
- [x] Jack (Linux, macOS), enable the `jack` feature
- [x] Web MIDI (Chrome, Opera, perhaps others browsers)

A higher-level API for parsing and assembling MIDI messages might be added in the future.

## Documentation & Example
API docs can be found at [docs.rs](https://docs.rs/crate/midir/). You can find some examples in the [`examples`](examples/) directory. Or simply run `cargo run --example test_play` after cloning this repository.
