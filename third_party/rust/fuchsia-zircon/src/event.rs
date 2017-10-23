// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Type-safe bindings for Zircon event objects.

use {AsHandleRef, Cookied, HandleBased, Handle, HandleRef, Status};
use {sys, into_result};

/// An object representing a Zircon
/// [event object](https://fuchsia.googlesource.com/zircon/+/master/docs/objects/event.md).
///
/// As essentially a subtype of `Handle`, it can be freely interconverted.
#[derive(Debug, Eq, PartialEq)]
pub struct Event(Handle);
impl_handle_based!(Event);
impl Cookied for Event {}

impl Event {
    /// Create an event object, an object which is signalable but nothing else. Wraps the
    /// [zx_event_create](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/event_create.md)
    /// syscall.
    pub fn create(options: EventOpts) -> Result<Event, Status> {
        let mut out = 0;
        let status = unsafe { sys::zx_event_create(options as u32, &mut out) };
        into_result(status, || Self::from(Handle(out)))
    }
}

/// Options for creating an event object.
#[repr(u32)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum EventOpts {
    /// Default options.
    Default = 0,
}

impl Default for EventOpts {
    fn default() -> Self {
        EventOpts::Default
    }
}
