use std::fmt;

use std::os::raw::c_void;

/// Inspects the current call-stack, passing all active frames into the closure
/// provided to calculate a stack trace.
///
/// This function is the workhorse of this library in calculating the stack
/// traces for a program. The given closure `cb` is yielded instances of a
/// `Frame` which represent information about that call frame on the stack. The
/// closure is yielded frames in a top-down fashion (most recently called
/// functions first).
///
/// The closure's return value is an indication of whether the backtrace should
/// continue. A return value of `false` will terminate the backtrace and return
/// immediately.
///
/// Once a `Frame` is acquired you will likely want to call `backtrace::resolve`
/// to convert the `ip` (instruction pointer) or symbol address to a `Symbol`
/// through which the name and/or filename/line number can be learned.
///
/// Note that this is a relatively low-level function and if you'd like to, for
/// example, capture a backtrace to be inspected later, then the `Backtrace`
/// type may be more appropriate.
///
/// # Example
///
/// ```
/// extern crate backtrace;
///
/// fn main() {
///     backtrace::trace(|frame| {
///         // ...
///
///         true // continue the backtrace
///     });
/// }
/// ```
#[inline(never)] // if this is never inlined then the first frame can be konwn
                 // to be skipped
pub fn trace<F: FnMut(&Frame) -> bool>(mut cb: F) {
    trace_imp(&mut cb)
}

/// A trait representing one frame of a backtrace, yielded to the `trace`
/// function of this crate.
///
/// The tracing function's closure will be yielded frames, and the frame is
/// virtually dispatched as the underlying implementation is not always known
/// until runtime.
pub struct Frame {
    inner: FrameImp,
}

impl Frame {
    /// Returns the current instruction pointer of this frame.
    ///
    /// This is normally the next instruction to execute in the frame, but not
    /// all implementations list this with 100% accuracy (but it's generally
    /// pretty close).
    ///
    /// It is recommended to pass this value to `backtrace::resolve` to turn it
    /// into a symbol name.
    pub fn ip(&self) -> *mut c_void {
        self.inner.ip()
    }

    /// Returns the starting symbol address of the frame of this function.
    ///
    /// This will attempt to rewind the instruction pointer returned by `ip` to
    /// the start of the function, returning that value. In some cases, however,
    /// backends will just return `ip` from this function.
    ///
    /// The returned value can sometimes be used if `backtrace::resolve` failed
    /// on the `ip` given above.
    pub fn symbol_address(&self) -> *mut c_void {
        self.inner.symbol_address()
    }
}

impl fmt::Debug for Frame {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("Frame")
            .field("ip", &self.ip())
            .field("symbol_address", &self.symbol_address())
            .finish()
    }
}

cfg_if! {
    if #[cfg(all(unix,
                 not(target_os = "emscripten"),
                 feature = "libunwind"))] {
        mod libunwind;
        use self::libunwind::trace as trace_imp;
        use self::libunwind::Frame as FrameImp;
    } else if #[cfg(all(unix,
                        not(target_os = "emscripten"),
                        feature = "unix-backtrace"))] {
        mod unix_backtrace;
        use self::unix_backtrace::trace as trace_imp;
        use self::unix_backtrace::Frame as FrameImp;
    } else if #[cfg(all(windows, feature = "dbghelp"))] {
        mod dbghelp;
        use self::dbghelp::trace as trace_imp;
        use self::dbghelp::Frame as FrameImp;
    } else {
        mod noop;
        use self::noop::trace as trace_imp;
        use self::noop::Frame as FrameImp;
    }
}
