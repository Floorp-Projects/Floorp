// Copyright 2016 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! Type-safe bindings for Zircon kernel
//! [syscalls](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls.md).

extern crate fuchsia_zircon_sys as zircon_sys;

use std::io;
use std::marker::PhantomData;

macro_rules! impl_handle_based {
    ($type_name:path) => {
        impl AsHandleRef for $type_name {
            fn as_handle_ref(&self) -> HandleRef {
                self.0.as_handle_ref()
            }
        }

        impl From<Handle> for $type_name {
            fn from(handle: Handle) -> Self {
                $type_name(handle)
            }
        }

        impl From<$type_name> for Handle {
            fn from(x: $type_name) -> Handle {
                x.0
            }
        }

        impl HandleBased for $type_name {}
    }
}

mod channel;
mod event;
mod eventpair;
mod fifo;
mod job;
mod port;
mod process;
mod socket;
mod timer;
mod thread;
mod vmo;

pub use channel::{Channel, ChannelOpts, MessageBuf};
pub use event::{Event, EventOpts};
pub use eventpair::{EventPair, EventPairOpts};
pub use fifo::{Fifo, FifoOpts};
pub use job::Job;
pub use port::{Packet, PacketContents, Port, PortOpts, SignalPacket, UserPacket, WaitAsyncOpts};
pub use process::Process;
pub use socket::{Socket, SocketOpts, SocketReadOpts, SocketWriteOpts};
pub use timer::{Timer, TimerOpts};
pub use thread::Thread;
pub use vmo::{Vmo, VmoCloneOpts, VmoOp, VmoOpts};

use zircon_sys as sys;

type Duration = sys::zx_duration_t;
type Time = sys::zx_time_t;
pub use zircon_sys::ZX_TIME_INFINITE;

// A placeholder value used for handles that have been taken from the message buf.
// We rely on the kernel never to produce any actual handles with this value.
const INVALID_HANDLE: sys::zx_handle_t = 0;

/// A status code returned from the Zircon kernel.
///
/// See
/// [errors.md](https://fuchsia.googlesource.com/zircon/+/master/docs/errors.md)
/// in the Zircon documentation for more information about the meaning of these
/// codes.
#[derive(Debug, PartialEq, Eq, Clone, Copy)]
#[repr(i32)]
// Auto-generated using tools/gen_status.py
pub enum Status {
    NoError = 0,
    ErrInternal = -1,
    ErrNotSupported = -2,
    ErrNoResources = -3,
    ErrNoMemory = -4,
    ErrCallFailed = -5,
    ErrInterruptedRetry = -6,
    ErrInvalidArgs = -10,
    ErrBadHandle = -11,
    ErrWrongType = -12,
    ErrBadSyscall = -13,
    ErrOutOfRange = -14,
    ErrBufferTooSmall = -15,
    ErrBadState = -20,
    ErrTimedOut = -21,
    ErrShouldWait = -22,
    ErrCanceled = -23,
    ErrPeerClosed = -24,
    ErrNotFound = -25,
    ErrAlreadyExists = -26,
    ErrAlreadyBound = -27,
    ErrUnavailable = -28,
    ErrAccessDenied = -30,
    ErrIo = -40,
    ErrIoRefused = -41,
    ErrIoDataIntegrity = -42,
    ErrIoDataLoss = -43,
    ErrBadPath = -50,
    ErrNotDir = -51,
    ErrNotFile = -52,
    ErrFileBig = -53,
    ErrNoSpace = -54,
    ErrStop = -60,
    ErrNext = -61,

    /// Any zx_status_t not in the set above will map to the following:
    UnknownOther = -32768,

    // used to prevent exhaustive matching
    #[doc(hidden)]
    __Nonexhaustive = -32787,
}

impl Status {
    pub fn from_raw(raw: sys::zx_status_t) -> Self {
        match raw {
            // Auto-generated using tools/gen_status.py
            sys::ZX_OK => Status::NoError,
            sys::ZX_ERR_INTERNAL => Status::ErrInternal,
            sys::ZX_ERR_NOT_SUPPORTED => Status::ErrNotSupported,
            sys::ZX_ERR_NO_RESOURCES => Status::ErrNoResources,
            sys::ZX_ERR_NO_MEMORY => Status::ErrNoMemory,
            sys::ZX_ERR_CALL_FAILED => Status::ErrCallFailed,
            sys::ZX_ERR_INTERRUPTED_RETRY => Status::ErrInterruptedRetry,
            sys::ZX_ERR_INVALID_ARGS => Status::ErrInvalidArgs,
            sys::ZX_ERR_BAD_HANDLE => Status::ErrBadHandle,
            sys::ZX_ERR_WRONG_TYPE => Status::ErrWrongType,
            sys::ZX_ERR_BAD_SYSCALL => Status::ErrBadSyscall,
            sys::ZX_ERR_OUT_OF_RANGE => Status::ErrOutOfRange,
            sys::ZX_ERR_BUFFER_TOO_SMALL => Status::ErrBufferTooSmall,
            sys::ZX_ERR_BAD_STATE => Status::ErrBadState,
            sys::ZX_ERR_TIMED_OUT => Status::ErrTimedOut,
            sys::ZX_ERR_SHOULD_WAIT => Status::ErrShouldWait,
            sys::ZX_ERR_CANCELED => Status::ErrCanceled,
            sys::ZX_ERR_PEER_CLOSED => Status::ErrPeerClosed,
            sys::ZX_ERR_NOT_FOUND => Status::ErrNotFound,
            sys::ZX_ERR_ALREADY_EXISTS => Status::ErrAlreadyExists,
            sys::ZX_ERR_ALREADY_BOUND => Status::ErrAlreadyBound,
            sys::ZX_ERR_UNAVAILABLE => Status::ErrUnavailable,
            sys::ZX_ERR_ACCESS_DENIED => Status::ErrAccessDenied,
            sys::ZX_ERR_IO => Status::ErrIo,
            sys::ZX_ERR_IO_REFUSED => Status::ErrIoRefused,
            sys::ZX_ERR_IO_DATA_INTEGRITY => Status::ErrIoDataIntegrity,
            sys::ZX_ERR_IO_DATA_LOSS => Status::ErrIoDataLoss,
            sys::ZX_ERR_BAD_PATH => Status::ErrBadPath,
            sys::ZX_ERR_NOT_DIR => Status::ErrNotDir,
            sys::ZX_ERR_NOT_FILE => Status::ErrNotFile,
            sys::ZX_ERR_FILE_BIG => Status::ErrFileBig,
            sys::ZX_ERR_NO_SPACE => Status::ErrNoSpace,
            sys::ZX_ERR_STOP => Status::ErrStop,
            sys::ZX_ERR_NEXT => Status::ErrNext,
            _ => Status::UnknownOther,
        }
    }

    pub fn into_io_err(self) -> io::Error {
        self.into()
    }

    // Note: no to_raw, even though it's easy to implement, partly because
    // handling of UnknownOther would be tricky.
}

impl From<io::ErrorKind> for Status {
    fn from(kind: io::ErrorKind) -> Self {
        use io::ErrorKind::*;
        use Status::*;

        match kind {
            NotFound => ErrNotFound,
            PermissionDenied => ErrAccessDenied,
            ConnectionRefused => ErrIoRefused,
            ConnectionAborted => ErrPeerClosed,
            AddrInUse => ErrAlreadyBound,
            AddrNotAvailable => ErrUnavailable,
            BrokenPipe => ErrPeerClosed,
            AlreadyExists => ErrAlreadyExists,
            WouldBlock => ErrShouldWait,
            InvalidInput => ErrInvalidArgs,
            TimedOut => ErrTimedOut,
            Interrupted => ErrInterruptedRetry,

            UnexpectedEof |
            WriteZero |
            ConnectionReset |
            NotConnected |
            Other | _ => ErrIo,
        }
    }
}

impl From<Status> for io::ErrorKind {
    fn from(status: Status) -> io::ErrorKind {
        use io::ErrorKind::*;
        use Status::*;

        match status {
            ErrInterruptedRetry => Interrupted,
            ErrBadHandle => BrokenPipe,
            ErrTimedOut => TimedOut,
            ErrShouldWait => WouldBlock,
            ErrPeerClosed => ConnectionAborted,
            ErrNotFound => NotFound,
            ErrAlreadyExists => AlreadyExists,
            ErrAlreadyBound => AlreadyExists,
            ErrUnavailable => AddrNotAvailable,
            ErrAccessDenied => PermissionDenied,
            ErrIoRefused => ConnectionRefused,
            ErrIoDataIntegrity => InvalidData,

            ErrBadPath |
            ErrInvalidArgs |
            ErrOutOfRange |
            ErrWrongType => InvalidInput,

            Status::__Nonexhaustive |
            UnknownOther |
            NoError |
            ErrNext |
            ErrStop |
            ErrNoSpace |
            ErrFileBig |
            ErrNotFile |
            ErrNotDir |
            ErrIoDataLoss |
            ErrIo |
            ErrCanceled |
            ErrBadState |
            ErrBufferTooSmall |
            ErrBadSyscall |
            ErrInternal |
            ErrNotSupported |
            ErrNoResources |
            ErrNoMemory |
            ErrCallFailed => Other,
        }
    }
}

impl From<io::Error> for Status {
    fn from(err: io::Error) -> Status {
        err.kind().into()
    }
}

impl From<Status> for io::Error {
    fn from(status: Status) -> io::Error {
        io::Error::from(io::ErrorKind::from(status))
    }
}

/// Rights associated with a handle.
///
/// See [rights.md](https://fuchsia.googlesource.com/zircon/+/master/docs/rights.md)
/// for more information.
pub type Rights = sys::zx_rights_t;
pub use zircon_sys::{
    ZX_RIGHT_NONE,
    ZX_RIGHT_DUPLICATE,
    ZX_RIGHT_TRANSFER,
    ZX_RIGHT_READ,
    ZX_RIGHT_WRITE,
    ZX_RIGHT_EXECUTE,
    ZX_RIGHT_MAP,
    ZX_RIGHT_GET_PROPERTY,
    ZX_RIGHT_SET_PROPERTY,
    ZX_RIGHT_DEBUG,
    ZX_RIGHT_SAME_RIGHTS,
};

/// Signals that can be waited upon.
///
/// See
/// [Objects and signals](https://fuchsia.googlesource.com/zircon/+/master/docs/concepts.md#Objects-and-Signals)
/// in the Zircon kernel documentation. Note: the names of signals are still in flux.
pub type Signals = sys::zx_signals_t;

pub use zircon_sys::{
        ZX_SIGNAL_NONE,

        ZX_SIGNAL_HANDLE_CLOSED,
        ZX_SIGNAL_LAST_HANDLE,

        ZX_USER_SIGNAL_0,
        ZX_USER_SIGNAL_1,
        ZX_USER_SIGNAL_2,
        ZX_USER_SIGNAL_3,
        ZX_USER_SIGNAL_4,
        ZX_USER_SIGNAL_5,
        ZX_USER_SIGNAL_6,
        ZX_USER_SIGNAL_7,

        // Event
        ZX_EVENT_SIGNALED,

        // EventPair
        ZX_EPAIR_SIGNALED,
        ZX_EPAIR_CLOSED,

        // Task signals (process, thread, job)
        ZX_TASK_TERMINATED,

        // Channel
        ZX_CHANNEL_READABLE,
        ZX_CHANNEL_WRITABLE,
        ZX_CHANNEL_PEER_CLOSED,

        // Socket
        ZX_SOCKET_READABLE,
        ZX_SOCKET_WRITABLE,
        ZX_SOCKET_PEER_CLOSED,

        // Timer
        ZX_TIMER_SIGNALED,
};

/// A "wait item" containing a handle reference and information about what signals
/// to wait on, and, on return from `object_wait_many`, which are pending.
#[repr(C)]
#[derive(Debug)]
pub struct WaitItem<'a> {
    /// The handle to wait on.
    pub handle: HandleRef<'a>,
    /// A set of signals to wait for.
    pub waitfor: Signals,
    /// The set of signals pending, on return of `object_wait_many`.
    pub pending: Signals,
}


/// An identifier to select a particular clock. See
/// [zx_time_get](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/time_get.md)
/// for more information about the possible values.
#[repr(u32)]
#[derive(Debug, Copy, Clone, Eq, PartialEq)]
pub enum ClockId {
    /// The number of nanoseconds since the system was powered on. Corresponds to
    /// `ZX_CLOCK_MONOTONIC`.
    Monotonic = 0,
    /// The number of wall clock nanoseconds since the Unix epoch (midnight on January 1 1970) in
    /// UTC. Corresponds to ZX_CLOCK_UTC.
    UTC = 1,
    /// The number of nanoseconds the current thread has been running for. Corresponds to
    /// ZX_CLOCK_THREAD.
    Thread = 2,
}

/// Get the current time, from the specific clock id.
///
/// Wraps the
/// [zx_time_get](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/time_get.md)
/// syscall.
pub fn time_get(clock_id: ClockId) -> Time {
    unsafe { sys::zx_time_get(clock_id as u32) }
}

/// Read the number of high-precision timer ticks since boot. These ticks may be processor cycles,
/// high speed timer, profiling timer, etc. They are not guaranteed to continue advancing when the
/// system is asleep.
///
/// Wraps the
/// [zx_ticks_get](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/ticks_get.md)
/// syscall.
pub fn ticks_get() -> u64 {
    unsafe { sys::zx_ticks_get() }
}

/// Compute a deadline for the time in the future that is the given `Duration` away.
///
/// Wraps the
/// [zx_deadline_after](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/deadline_after.md)
/// syscall.
pub fn deadline_after(nanos: Duration) -> Time {
    unsafe { sys::zx_deadline_after(nanos) }
}

/// Sleep until the given deadline.
///
/// Wraps the
/// [zx_nanosleep](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/nanosleep.md)
/// syscall.
pub fn nanosleep(deadline: Time) {
    unsafe { sys::zx_nanosleep(deadline); }
}

/// Return the number of high-precision timer ticks in a second.
///
/// Wraps the
/// [zx_ticks_per_second](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/ticks_per_second.md)
/// syscall.
pub fn ticks_per_second() -> u64 {
    unsafe { sys::zx_ticks_per_second() }
}

pub use zircon_sys::{
    ZX_CPRNG_DRAW_MAX_LEN,
    ZX_CPRNG_ADD_ENTROPY_MAX_LEN,
};

/// Draw random bytes from the kernel's CPRNG to fill the given buffer. Returns the actual number of
/// bytes drawn, which may sometimes be less than the size of the buffer provided.
///
/// The buffer must have length less than `ZX_CPRNG_DRAW_MAX_LEN`.
///
/// Wraps the
/// [zx_cprng_draw](https://fuchsia.googlesource.com/zircon/+/HEAD/docs/syscalls/cprng_draw.md)
/// syscall.
pub fn cprng_draw(buffer: &mut [u8]) -> Result<usize, Status> {
    let mut actual = 0;
    let status = unsafe { sys::zx_cprng_draw(buffer.as_mut_ptr(), buffer.len(), &mut actual) };
    into_result(status, || actual)
}

/// Mix the given entropy into the kernel CPRNG.
///
/// The buffer must have length less than `ZX_CPRNG_ADD_ENTROPY_MAX_LEN`.
///
/// Wraps the
/// [zx_cprng_add_entropy](https://fuchsia.googlesource.com/zircon/+/HEAD/docs/syscalls/cprng_add_entropy.md)
/// syscall.
pub fn cprng_add_entropy(buffer: &[u8]) -> Result<(), Status> {
    let status = unsafe { sys::zx_cprng_add_entropy(buffer.as_ptr(), buffer.len()) };
    into_result(status, || ())
}

fn into_result<T, F>(status: sys::zx_status_t, f: F) -> Result<T, Status>
    where F: FnOnce() -> T {
    // All non-negative values are assumed successful. Note: calls that don't try
    // to multiplex success values into status return could be more strict here.
    if status >= 0 {
        Ok(f())
    } else {
        Err(Status::from_raw(status))
    }
}

// Handles

/// A borrowed reference to a `Handle`.
///
/// Mostly useful as part of a `WaitItem`.
#[derive(Debug, Copy, Clone, Eq, PartialEq, Hash)]
pub struct HandleRef<'a> {
    handle: sys::zx_handle_t,
    phantom: PhantomData<&'a sys::zx_handle_t>,
}

impl<'a> HandleRef<'a> {
    pub fn raw_handle(&self) -> sys::zx_handle_t {
        self.handle
    }

    pub fn duplicate(&self, rights: Rights) -> Result<Handle, Status> {
        let handle = self.handle;
        let mut out = 0;
        let status = unsafe { sys::zx_handle_duplicate(handle, rights, &mut out) };
        into_result(status, || Handle(out))
    }

    pub fn signal(&self, clear_mask: Signals, set_mask: Signals) -> Result<(), Status> {
        let handle = self.handle;
        let status = unsafe { sys::zx_object_signal(handle, clear_mask.bits(), set_mask.bits()) };
        into_result(status, || ())
    }

    pub fn wait(&self, signals: Signals, deadline: Time) -> Result<Signals, Status> {
        let handle = self.handle;
        let mut pending = sys::zx_signals_t::empty();
        let status = unsafe {
            sys::zx_object_wait_one(handle, signals, deadline, &mut pending)
        };
        into_result(status, || pending)
    }

    pub fn wait_async(&self, port: &Port, key: u64, signals: Signals, options: WaitAsyncOpts)
        -> Result<(), Status>
    {
        let handle = self.handle;
        let status = unsafe {
            sys::zx_object_wait_async(handle, port.raw_handle(), key, signals, options as u32)
        };
        into_result(status, || ())
    }
}

/// A trait to get a reference to the underlying handle of an object.
pub trait AsHandleRef {
    /// Get a reference to the handle. One important use of such a reference is
    /// for `object_wait_many`.
    fn as_handle_ref(&self) -> HandleRef;

    /// Interpret the reference as a raw handle (an integer type). Two distinct
    /// handles will have different raw values (so it can perhaps be used as a
    /// key in a data structure).
    fn raw_handle(&self) -> sys::zx_handle_t {
        self.as_handle_ref().raw_handle()
    }

    /// Set and clear userspace-accessible signal bits on an object. Wraps the
    /// [zx_object_signal](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/object_signal.md)
    /// syscall.
    fn signal_handle(&self, clear_mask: Signals, set_mask: Signals) -> Result<(), Status> {
        self.as_handle_ref().signal(clear_mask, set_mask)
    }

    /// Waits on a handle. Wraps the
    /// [zx_object_wait_one](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/object_wait_one.md)
    /// syscall.
    fn wait_handle(&self, signals: Signals, deadline: Time) -> Result<Signals, Status> {
        self.as_handle_ref().wait(signals, deadline)
    }

    /// Causes packet delivery on the given port when the object changes state and matches signals.
    /// [zx_object_wait_async](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/object_wait_async.md)
    /// syscall.
    fn wait_async_handle(&self, port: &Port, key: u64, signals: Signals, options: WaitAsyncOpts)
        -> Result<(), Status>
    {
        self.as_handle_ref().wait_async(port, key, signals, options)
    }
}

impl<'a> AsHandleRef for HandleRef<'a> {
    fn as_handle_ref(&self) -> HandleRef { *self }
}

/// A trait implemented by all handle-based types.
///
/// Note: it is reasonable for user-defined objects wrapping a handle to implement
/// this trait. For example, a specific interface in some protocol might be
/// represented as a newtype of `Channel`, and implement the `as_handle_ref`
/// method and the `From<Handle>` trait to facilitate conversion from and to the
/// interface.
pub trait HandleBased: AsHandleRef + From<Handle> + Into<Handle> {
    /// Duplicate a handle, possibly reducing the rights available. Wraps the
    /// [zx_handle_duplicate](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/handle_duplicate.md)
    /// syscall.
    fn duplicate_handle(&self, rights: Rights) -> Result<Self, Status> {
        self.as_handle_ref().duplicate(rights).map(|handle| Self::from(handle))
    }

    /// Create a replacement for a handle, possibly reducing the rights available. This invalidates
    /// the original handle. Wraps the
    /// [zx_handle_replace](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/handle_replace.md)
    /// syscall.
    fn replace_handle(self, rights: Rights) -> Result<Self, Status> {
        <Self as Into<Handle>>::into(self)
            .replace(rights).map(|handle| Self::from(handle))
    }

    /// Converts the value into its inner handle.
    ///
    /// This is a convenience function which simply forwards to the `Into` trait.
    fn into_handle(self) -> Handle {
        self.into()
    }

    /// Creates an instance of this type from a handle.
    ///
    /// This is a convenience function which simply forwards to the `From` trait.
    fn from_handle(handle: Handle) -> Self {
        Self::from(handle)
    }

    /// Creates an instance of another handle-based type from this value's inner handle.
    fn into_handle_based<H: HandleBased>(self) -> H {
        H::from_handle(self.into_handle())
    }

    /// Creates an instance of this type from the inner handle of another
    /// handle-based type.
    fn from_handle_based<H: HandleBased>(h: H) -> Self {
        Self::from_handle(h.into_handle())
    }
}

/// A trait implemented by all handles for objects which have a peer.
pub trait Peered: HandleBased {
    /// Set and clear userspace-accessible signal bits on the object's peer. Wraps the
    /// [zx_object_signal_peer](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/object_signal.md)
    /// syscall.
    fn signal_peer(&self, clear_mask: Signals, set_mask: Signals) -> Result<(), Status> {
        let handle = self.as_handle_ref().handle;
        let status = unsafe {
            sys::zx_object_signal_peer(handle, clear_mask.bits(), set_mask.bits())
        };
        into_result(status, || ())
    }
}

/// A trait implemented by all handles for objects which can have a cookie attached.
pub trait Cookied: HandleBased {
    /// Get the cookie attached to this object, if any. Wraps the
    /// [zx_object_get_cookie](https://fuchsia.googlesource.com/zircon/+/HEAD/docs/syscalls/object_get_cookie.md)
    /// syscall.
    fn get_cookie(&self, scope: &HandleRef) -> Result<u64, Status> {
        let handle = self.as_handle_ref().handle;
        let mut cookie = 0;
        let status = unsafe { sys::zx_object_get_cookie(handle, scope.handle, &mut cookie) };
        into_result(status, || cookie)
    }

    /// Attach an opaque cookie to this object with the given scope. The cookie may be read or
    /// changed in future only with the same scope. Wraps the
    /// [zx_object_set_cookie](https://fuchsia.googlesource.com/zircon/+/HEAD/docs/syscalls/object_set_cookie.md)
    /// syscall.
    fn set_cookie(&self, scope: &HandleRef, cookie: u64) -> Result<(), Status> {
        let handle = self.as_handle_ref().handle;
        let status = unsafe { sys::zx_object_set_cookie(handle, scope.handle, cookie) };
        into_result(status, || ())
    }
}

fn handle_drop(handle: sys::zx_handle_t) {
    let _ = unsafe { sys::zx_handle_close(handle) };
}

/// Wait on multiple handles.
/// The success return value is a bool indicating whether one or more of the
/// provided handle references was closed during the wait.
///
/// Wraps the
/// [zx_object_wait_many](https://fuchsia.googlesource.com/zircon/+/master/docs/syscalls/object_wait_many.md)
/// syscall.
pub fn object_wait_many(items: &mut [WaitItem], deadline: Time) -> Result<bool, Status>
{
    let len = try!(usize_into_u32(items.len()).map_err(|_| Status::ErrOutOfRange));
    let items_ptr = items.as_mut_ptr() as *mut sys::zx_wait_item_t;
    let status = unsafe { sys::zx_object_wait_many( items_ptr, len, deadline) };
    if status == sys::ZX_ERR_CANCELED {
        return Ok((true))
    }
    into_result(status, || false)
}

// An untyped handle

/// An object representing a Zircon
/// [handle](https://fuchsia.googlesource.com/zircon/+/master/docs/handles.md).
///
/// Internally, it is represented as a 32-bit integer, but this wrapper enforces
/// strict ownership semantics. The `Drop` implementation closes the handle.
///
/// This type represents the most general reference to a kernel object, and can
/// be interconverted to and from more specific types. Those conversions are not
/// enforced in the type system; attempting to use them will result in errors
/// returned by the kernel. These conversions don't change the underlying
/// representation, but do change the type and thus what operations are available.
#[derive(Debug, Eq, PartialEq, Hash)]
pub struct Handle(sys::zx_handle_t);

impl AsHandleRef for Handle {
    fn as_handle_ref(&self) -> HandleRef {
        HandleRef { handle: self.0, phantom: Default::default() }
    }
}

impl HandleBased for Handle {}

impl Drop for Handle {
    fn drop(&mut self) {
        handle_drop(self.0)
    }
}

impl Handle {
    /// If a raw handle is obtained from some other source, this method converts
    /// it into a type-safe owned handle.
    pub unsafe fn from_raw(raw: sys::zx_handle_t) -> Handle {
        Handle(raw)
    }

    pub fn replace(self, rights: Rights) -> Result<Handle, Status> {
        let handle = self.0;
        let mut out = 0;
        let status = unsafe { sys::zx_handle_replace(handle, rights, &mut out) };
        into_result(status, || Handle(out))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn monotonic_time_increases() {
        let time1 = time_get(ClockId::Monotonic);
        nanosleep(deadline_after(1_000));
        let time2 = time_get(ClockId::Monotonic);
        assert!(time2 > time1);
    }

    #[test]
    fn utc_time_increases() {
        let time1 = time_get(ClockId::UTC);
        nanosleep(deadline_after(1_000));
        let time2 = time_get(ClockId::UTC);
        assert!(time2 > time1);
    }

    #[test]
    fn thread_time_increases() {
        let time1 = time_get(ClockId::Thread);
        nanosleep(deadline_after(1_000));
        let time2 = time_get(ClockId::Thread);
        assert!(time2 > time1);
    }

    #[test]
    fn ticks_increases() {
        let ticks1 = ticks_get();
        nanosleep(deadline_after(1_000));
        let ticks2 = ticks_get();
        assert!(ticks2 > ticks1);
    }

    #[test]
    fn tick_length() {
        let sleep_ns = 1_000_000;  // 1ms
        let one_second_ns = 1_000_000_000; // 1 second in ns
        let ticks1 = ticks_get();
        nanosleep(deadline_after(sleep_ns));
        let ticks2 = ticks_get();
        // The number of ticks should have increased by at least 1 ms worth
        assert!(ticks2 > ticks1 + sleep_ns * ticks_per_second() / one_second_ns);
    }

    #[test]
    fn sleep() {
        let sleep_ns = 1_000_000;  // 1ms
        let time1 = time_get(ClockId::Monotonic);
        nanosleep(deadline_after(sleep_ns));
        let time2 = time_get(ClockId::Monotonic);
        assert!(time2 > time1 + sleep_ns);
    }

    /// Test duplication by means of a VMO
    #[test]
    fn duplicate() {
        let hello_length: usize = 5;

        // Create a VMO and write some data to it.
        let vmo = Vmo::create(hello_length as u64, VmoOpts::Default).unwrap();
        assert!(vmo.write(b"hello", 0).is_ok());

        // Replace, reducing rights to read.
        let readonly_vmo = vmo.duplicate_handle(ZX_RIGHT_READ).unwrap();
        // Make sure we can read but not write.
        let mut read_vec = vec![0; hello_length];
        assert_eq!(readonly_vmo.read(&mut read_vec, 0).unwrap(), hello_length);
        assert_eq!(read_vec, b"hello");
        assert_eq!(readonly_vmo.write(b"", 0), Err(Status::ErrAccessDenied));

        // Write new data to the original handle, and read it from the new handle
        assert!(vmo.write(b"bye", 0).is_ok());
        assert_eq!(readonly_vmo.read(&mut read_vec, 0).unwrap(), hello_length);
        assert_eq!(read_vec, b"byelo");
    }

    // Test replace by means of a VMO
    #[test]
    fn replace() {
        let hello_length: usize = 5;

        // Create a VMO and write some data to it.
        let vmo = Vmo::create(hello_length as u64, VmoOpts::Default).unwrap();
        assert!(vmo.write(b"hello", 0).is_ok());

        // Replace, reducing rights to read.
        let readonly_vmo = vmo.replace_handle(ZX_RIGHT_READ).unwrap();
        // Make sure we can read but not write.
        let mut read_vec = vec![0; hello_length];
        assert_eq!(readonly_vmo.read(&mut read_vec, 0).unwrap(), hello_length);
        assert_eq!(read_vec, b"hello");
        assert_eq!(readonly_vmo.write(b"", 0), Err(Status::ErrAccessDenied));
    }

    #[test]
    fn wait_and_signal() {
        let event = Event::create(EventOpts::Default).unwrap();
        let ten_ms: Duration = 10_000_000;

        // Waiting on it without setting any signal should time out.
        assert_eq!(event.wait_handle(
            ZX_USER_SIGNAL_0, deadline_after(ten_ms)), Err(Status::ErrTimedOut));

        // If we set a signal, we should be able to wait for it.
        assert!(event.signal_handle(ZX_SIGNAL_NONE, ZX_USER_SIGNAL_0).is_ok());
        assert_eq!(event.wait_handle(ZX_USER_SIGNAL_0, deadline_after(ten_ms)).unwrap(),
            ZX_USER_SIGNAL_0 | ZX_SIGNAL_LAST_HANDLE);

        // Should still work, signals aren't automatically cleared.
        assert_eq!(event.wait_handle(ZX_USER_SIGNAL_0, deadline_after(ten_ms)).unwrap(),
            ZX_USER_SIGNAL_0 | ZX_SIGNAL_LAST_HANDLE);

        // Now clear it, and waiting should time out again.
        assert!(event.signal_handle(ZX_USER_SIGNAL_0, ZX_SIGNAL_NONE).is_ok());
        assert_eq!(event.wait_handle(
            ZX_USER_SIGNAL_0, deadline_after(ten_ms)), Err(Status::ErrTimedOut));
    }

    #[test]
    fn wait_many_and_signal() {
        let ten_ms: Duration = 10_000_000;
        let e1 = Event::create(EventOpts::Default).unwrap();
        let e2 = Event::create(EventOpts::Default).unwrap();

        // Waiting on them now should time out.
        let mut items = vec![
          WaitItem { handle: e1.as_handle_ref(), waitfor: ZX_USER_SIGNAL_0, pending: ZX_SIGNAL_NONE },
          WaitItem { handle: e2.as_handle_ref(), waitfor: ZX_USER_SIGNAL_1, pending: ZX_SIGNAL_NONE },
        ];
        assert_eq!(object_wait_many(&mut items, deadline_after(ten_ms)), Err(Status::ErrTimedOut));
        assert_eq!(items[0].pending, ZX_SIGNAL_LAST_HANDLE);
        assert_eq!(items[1].pending, ZX_SIGNAL_LAST_HANDLE);

        // Signal one object and it should return success.
        assert!(e1.signal_handle(ZX_SIGNAL_NONE, ZX_USER_SIGNAL_0).is_ok());
        assert!(object_wait_many(&mut items, deadline_after(ten_ms)).is_ok());
        assert_eq!(items[0].pending, ZX_USER_SIGNAL_0 | ZX_SIGNAL_LAST_HANDLE);
        assert_eq!(items[1].pending, ZX_SIGNAL_LAST_HANDLE);

        // Signal the other and it should return both.
        assert!(e2.signal_handle(ZX_SIGNAL_NONE, ZX_USER_SIGNAL_1).is_ok());
        assert!(object_wait_many(&mut items, deadline_after(ten_ms)).is_ok());
        assert_eq!(items[0].pending, ZX_USER_SIGNAL_0 | ZX_SIGNAL_LAST_HANDLE);
        assert_eq!(items[1].pending, ZX_USER_SIGNAL_1 | ZX_SIGNAL_LAST_HANDLE);

        // Clear signals on both; now it should time out again.
        assert!(e1.signal_handle(ZX_USER_SIGNAL_0, ZX_SIGNAL_NONE).is_ok());
        assert!(e2.signal_handle(ZX_USER_SIGNAL_1, ZX_SIGNAL_NONE).is_ok());
        assert_eq!(object_wait_many(&mut items, deadline_after(ten_ms)), Err(Status::ErrTimedOut));
        assert_eq!(items[0].pending, ZX_SIGNAL_LAST_HANDLE);
        assert_eq!(items[1].pending, ZX_SIGNAL_LAST_HANDLE);
    }

    #[test]
    fn cookies() {
        let event = Event::create(EventOpts::Default).unwrap();
        let scope = Event::create(EventOpts::Default).unwrap();

        // Getting a cookie when none has been set should fail.
        assert_eq!(event.get_cookie(&scope.as_handle_ref()), Err(Status::ErrAccessDenied));

        // Set a cookie.
        assert_eq!(event.set_cookie(&scope.as_handle_ref(), 42), Ok(()));

        // Should get it back....
        assert_eq!(event.get_cookie(&scope.as_handle_ref()), Ok(42));

        // but not with the wrong scope!
        assert_eq!(event.get_cookie(&event.as_handle_ref()), Err(Status::ErrAccessDenied));

        // Can change it, with the same scope...
        assert_eq!(event.set_cookie(&scope.as_handle_ref(), 123), Ok(()));

        // but not with a different scope.
        assert_eq!(event.set_cookie(&event.as_handle_ref(), 123), Err(Status::ErrAccessDenied));
    }

    #[test]
    fn cprng() {
        let mut buffer = [0; 20];
        assert_eq!(cprng_draw(&mut buffer), Ok(20));
        assert_ne!(buffer[0], 0);
        assert_ne!(buffer[19], 0);
    }

    #[test]
    fn cprng_too_large() {
        let mut buffer = [0; ZX_CPRNG_DRAW_MAX_LEN + 1];
        assert_eq!(cprng_draw(&mut buffer), Err(Status::ErrInvalidArgs));

        for mut s in buffer.chunks_mut(ZX_CPRNG_DRAW_MAX_LEN) {
            assert_eq!(cprng_draw(&mut s), Ok(s.len()));
        }
    }

    #[test]
    fn cprng_add() {
        let buffer = [0, 1, 2];
        assert_eq!(cprng_add_entropy(&buffer), Ok(()));
    }
}

pub fn usize_into_u32(n: usize) -> Result<u32, ()> {
    if n > ::std::u32::MAX as usize || n < ::std::u32::MIN as usize {
        return Err(())
    }
    Ok(n as u32)
}

pub fn size_to_u32_sat(n: usize) -> u32 {
    if n > ::std::u32::MAX as usize {
        return ::std::u32::MAX;
    }
    if n < ::std::u32::MIN as usize {
        return ::std::u32::MIN;
    }
    n as u32
}
