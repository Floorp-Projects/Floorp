// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Type-safe bindings for Zircon event pairs.

use {AsHandleRef, Cookied, HandleBased, Handle, HandleRef, Peered, Status};
use {sys, into_result};

/// An object representing a Zircon
/// [event pair](https://fuchsia.googlesource.com/zircon/+/master/docs/concepts.md#Other-IPC_Events_Event-Pairs_and-User-Signals).
///
/// As essentially a subtype of `Handle`, it can be freely interconverted.
#[derive(Debug, Eq, PartialEq)]
pub struct EventPair(Handle);
impl_handle_based!(EventPair);
impl Peered for EventPair {}
impl Cookied for EventPair {}

impl EventPair {
    /// Create an event pair, a pair of objects which can signal each other. Wraps the
    /// [zx_eventpair_create](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/eventpair_create.md)
    /// syscall.
    pub fn create(options: EventPairOpts) -> Result<(EventPair, EventPair), Status> {
        let mut out0 = 0;
        let mut out1 = 0;
        let status = unsafe { sys::zx_eventpair_create(options as u32, &mut out0, &mut out1) };
        into_result(status, ||
            (Self::from(Handle(out0)),
                Self::from(Handle(out1))))
    }
}

/// Options for creating an event pair.
#[repr(u32)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum EventPairOpts {
    /// Default options.
    Default = 0,
}

impl Default for EventPairOpts {
    fn default() -> Self {
        EventPairOpts::Default
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use {Duration, ZX_SIGNAL_LAST_HANDLE, ZX_SIGNAL_NONE, ZX_USER_SIGNAL_0};
    use deadline_after;

    #[test]
    fn wait_and_signal_peer() {
        let (p1, p2) = EventPair::create(EventPairOpts::Default).unwrap();
        let eighty_ms: Duration = 80_000_000;

        // Waiting on one without setting any signal should time out.
        assert_eq!(p2.wait_handle(ZX_USER_SIGNAL_0, deadline_after(eighty_ms)), Err(Status::ErrTimedOut));

        // If we set a signal, we should be able to wait for it.
        assert!(p1.signal_peer(ZX_SIGNAL_NONE, ZX_USER_SIGNAL_0).is_ok());
        assert_eq!(p2.wait_handle(ZX_USER_SIGNAL_0, deadline_after(eighty_ms)).unwrap(),
            ZX_USER_SIGNAL_0 | ZX_SIGNAL_LAST_HANDLE);

        // Should still work, signals aren't automatically cleared.
        assert_eq!(p2.wait_handle(ZX_USER_SIGNAL_0, deadline_after(eighty_ms)).unwrap(),
            ZX_USER_SIGNAL_0 | ZX_SIGNAL_LAST_HANDLE);

        // Now clear it, and waiting should time out again.
        assert!(p1.signal_peer(ZX_USER_SIGNAL_0, ZX_SIGNAL_NONE).is_ok());
        assert_eq!(p2.wait_handle(ZX_USER_SIGNAL_0, deadline_after(eighty_ms)), Err(Status::ErrTimedOut));
    }
}
