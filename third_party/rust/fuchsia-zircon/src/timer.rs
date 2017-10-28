// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Type-safe bindings for Zircon timer objects.

use {AsHandleRef, ClockId, Duration, HandleBased, Handle, HandleRef, Status, Time};
use {sys, into_result};

/// An object representing a Zircon
/// [event pair](https://fuchsia.googlesource.com/zircon/+/master/docs/concepts.md#Other-IPC_Events_Event-Pairs_and-User-Signals).
///
/// As essentially a subtype of `Handle`, it can be freely interconverted.
#[derive(Debug, Eq, PartialEq)]
pub struct Timer(Handle);
impl_handle_based!(Timer);

impl Timer {
    /// Create a timer, an object that can signal when a specified point in time has been reached.
    /// Wraps the
    /// [zx_timer_create](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/timer_create.md)
    /// syscall.
    pub fn create(options: TimerOpts, clock_id: ClockId) -> Result<Timer, Status> {
        let mut out = 0;
        let status = unsafe { sys::zx_timer_create(options as u32, clock_id as u32, &mut out) };
        into_result(status, || Self::from(Handle(out)))
    }

    /// Start a one-shot timer that will fire when `deadline` passes. Wraps the
    /// [zx_timer_set](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/timer_set.md)
    /// syscall.
    pub fn set(&self, deadline: Time, slack: Duration) -> Result<(), Status> {
        let status = unsafe { sys::zx_timer_set(self.raw_handle(), deadline, slack) };
        into_result(status, || ())
    }

    /// Cancels a pending timer that was started with start(). Wraps the
    /// [zx_timer_cancel](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/timer_cancel.md)
    /// syscall.
    pub fn cancel(&self) -> Result<(), Status> {
        let status = unsafe { sys::zx_timer_cancel(self.raw_handle()) };
        into_result(status, || ())
    }
}

/// Options for creating a timer.
#[repr(u32)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum TimerOpts {
    /// Default options.
    Default = 0,
}

impl Default for TimerOpts {
    fn default() -> Self {
        TimerOpts::Default
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use {Duration, ZX_SIGNAL_LAST_HANDLE, ZX_TIMER_SIGNALED};
    use deadline_after;

    #[test]
    fn create_timer_invalid_clock() {
        assert_eq!(Timer::create(TimerOpts::Default, ClockId::UTC).unwrap_err(), Status::ErrInvalidArgs);
        assert_eq!(Timer::create(TimerOpts::Default, ClockId::Thread), Err(Status::ErrInvalidArgs));
    }

    #[test]
    fn timer_basic() {
        let ten_ms: Duration = 10_000_000;
        let twenty_ms: Duration = 20_000_000;

        // Create a timer
        let timer = Timer::create(TimerOpts::Default, ClockId::Monotonic).unwrap();

        // Should not signal yet.
        assert_eq!(timer.wait_handle(ZX_TIMER_SIGNALED, deadline_after(ten_ms)), Err(Status::ErrTimedOut));

        // Set it, and soon it should signal.
        assert_eq!(timer.set(ten_ms, 0), Ok(()));
        assert_eq!(timer.wait_handle(ZX_TIMER_SIGNALED, deadline_after(twenty_ms)).unwrap(),
            ZX_TIMER_SIGNALED | ZX_SIGNAL_LAST_HANDLE);

        // Cancel it, and it should stop signalling.
        assert_eq!(timer.cancel(), Ok(()));
        assert_eq!(timer.wait_handle(ZX_TIMER_SIGNALED, deadline_after(ten_ms)), Err(Status::ErrTimedOut));
    }
}
