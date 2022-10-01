//! # libcubeb bindings for rust
//!
//! This library contains bindings to the [cubeb][1] C library which
//! is used to interact with system audio.  The library itself is a
//! work in progress and is likely lacking documentation and test.
//!
//! [1]: https://github.com/kinetiknz/cubeb/
//!
//! The cubeb-rs library exposes the user API of libcubeb.  It doesn't
//! expose the internal interfaces, so isn't suitable for extending
//! libcubeb. See [cubeb-pulse-rs][2] for an example of extending
//! libcubeb via implementing a cubeb backend in rust.
//!
//! To get started, have a look at the [`StreamBuilder`]

// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

extern crate cubeb_core;

mod context;
mod frame;
mod sample;
mod stream;

pub use context::*;
// Re-export cubeb_core types
pub use cubeb_core::{
    ffi, ChannelLayout, Context, ContextRef, Device, DeviceCollection, DeviceCollectionRef,
    DeviceFormat, DeviceId, DeviceInfo, DeviceInfoRef, DeviceRef, DeviceState, DeviceType, Error,
    ErrorCode, LogLevel, Result, SampleFormat, State, StreamParams, StreamParamsBuilder,
    StreamParamsRef, StreamPrefs, StreamRef,
};
pub use frame::*;
pub use sample::*;
pub use stream::*;
