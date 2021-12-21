# coremidi

This is a [CoreMIDI](https://developer.apple.com/reference/coremidi) library for Rust built on top of the low-level bindings [coremidi-sys](https://github.com/jonas-k/coremidi-sys).
CoreMIDI is a macOS framework that provides APIs for communicating with MIDI (Musical Instrument Digital Interface) devices, including hardware keyboards and synthesizers.

This library preserves the fundamental concepts behind the CoreMIDI framework, while being Rust idiomatic. This means that if you already know CoreMIDI, you will find very easy to start using it.

The **documentation** for the master branch can be found here: https://chris-zen.github.io/coremidi/coremidi/

Please see the [examples](examples) for an idea on how it looks like, but if you are eager to see some code, this is how you would send some note:

```rust
extern crate coremidi;
use coremidi::{Client, Destinations, PacketBuffer};
use std::time::Duration;
use std::thread;
let client = Client::new("example-client").unwrap();
let output_port = client.output_port("example-port").unwrap();
let destination = Destinations::from_index(0).unwrap();
let note_on = PacketBuffer::new(0, &[0x90, 0x40, 0x7f]);
let note_off = PacketBuffer::new(0, &[0x80, 0x40, 0x7f]);
output_port.send(&destination, &note_on).unwrap();
thread::sleep(Duration::from_millis(1000));
output_port.send(&destination, &note_off).unwrap();
```

If you are looking for a portable MIDI library then you can look into:
- [midir](https://github.com/Boddlnagg/midir) (which is using this lib)
- [portmidi-rs](https://github.com/musitdev/portmidi-rs)

For handling low level MIDI data you may look into:
- [rimd](https://github.com/RustAudio/rimd)
- [midi-rs](https://github.com/samdoshi/midi-rs)

[![Build Status](https://travis-ci.org/chris-zen/coremidi.svg?branch=master)](https://travis-ci.org/chris-zen/coremidi)
[![Crates.io](https://img.shields.io/crates/v/coremidi.svg)](https://crates.io/crates/coremidi)
[![Crates.io](https://img.shields.io/crates/d/coremidi.svg)](https://crates.io/crates/coremidi)
[![Crates.io](https://img.shields.io/crates/dv/coremidi.svg)](https://crates.io/crates/coremidi)
[![GitHub tag](https://img.shields.io/github/tag/chris-zen/coremidi.svg)](https://travis-ci.org/chris-zen/coremidi)
[![Minimum rustc version](https://img.shields.io/badge/rustc-1.36+-blue.svg)](https://blog.rust-lang.org/2019/07/04/Rust-1.36.0.html)
[![Join the chat at https://gitter.im/coremidi/Lobby](https://badges.gitter.im/coremidi/Lobby.svg)](https://gitter.im/coremidi/Lobby?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

# Installation

The library is published into [crates.io](https://crates.io/crates/coremidi), so it can be used by adding the following lines into your `Cargo.toml` file (but remember to update the version number accordingly):

```toml
[dependencies]
coremidi = "^0.6.0"
```

If you prefer to live in the edge ;-) you can use the master branch by including this instead:

```toml
[dependencies]
coremidi = { git = "https://github.com/chris-zen/coremidi", branch="master" }
```

To play with the source code yourself you can clone the repo and build the code and documentation with the following commands:

```sh
git clone https://github.com/chris-zen/coremidi.git
cd coremidi
cargo build
cargo test
cargo doc
open target/doc/coremidi/index.html
```

# Examples

The examples can be run with:

```sh
cargo run --example send
```

These are the provided examples:

- [endpoints](examples/endpoints.rs): how to enumerate sources and destinations.
- [send](examples/send.rs): how to create an output port and send MIDI messages.
- [receive](examples/receive.rs): how to create an input port and receive MIDI messages.
- [virtual-source](examples/virtual-source.rs): how to create a virtual source and generate MIDI messages.
- [virtual-destination](examples/virtual-destination.rs): how to create a virtual destination and receive MIDI messages.
- [properties](examples/properties.rs): how to set and get properties on MIDI objects.
- [notifications](examples/notifications.rs): how to receive MIDI client notifications.

# Roadmap

- [x] Enumerate destinations
- [x] Create output ports
- [x] Create a PacketList from MIDI bytes
- [x] Send a PacketList into an output port
- [x] Create virtual sources
- [x] Support a virtual source receiving a PacketList
- [x] Flush output
- [x] Enumerate sources
- [x] Create input ports
- [x] Support callbacks from input messages
- [x] Connect and disconnect sources
- [x] Add support to build PacketList (PacketBuffer)
- [x] Create virtual destinations with callback
- [x] Stop and restart MIDI I/O
- [x] MIDI Objects and properties
- [x] Client notifications
- [ ] Support Sysex
- [ ] Support devices
- [ ] Support entities
- [ ] MIDIThru connections
