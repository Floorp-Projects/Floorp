private let UNIFFI_RUST_FUTURE_POLL_READY: Int8 = 0
private let UNIFFI_RUST_FUTURE_POLL_MAYBE_READY: Int8 = 1

fileprivate func uniffiRustCallAsync<F, T>(
    rustFutureFunc: () -> UnsafeMutableRawPointer,
    pollFunc: (UnsafeMutableRawPointer, UnsafeMutableRawPointer) -> (),
    completeFunc: (UnsafeMutableRawPointer, UnsafeMutablePointer<RustCallStatus>) -> F,
    freeFunc: (UnsafeMutableRawPointer) -> (),
    liftFunc: (F) throws -> T,
    errorHandler: ((RustBuffer) throws -> Error)?
) async throws -> T {
    // Make sure to call uniffiEnsureInitialized() since future creation doesn't have a
    // RustCallStatus param, so doesn't use makeRustCall()
    uniffiEnsureInitialized()
    let rustFuture = rustFutureFunc()
    defer {
        freeFunc(rustFuture)
    }
    var pollResult: Int8;
    repeat {
        pollResult = await withUnsafeContinuation {
            pollFunc(rustFuture, ContinuationHolder($0).toOpaque())
        }
    } while pollResult != UNIFFI_RUST_FUTURE_POLL_READY

    return try liftFunc(makeRustCall(
        { completeFunc(rustFuture, $0) },
        errorHandler: errorHandler
    ))
}

// Callback handlers for an async calls.  These are invoked by Rust when the future is ready.  They
// lift the return value or error and resume the suspended function.
fileprivate func uniffiFutureContinuationCallback(ptr: UnsafeMutableRawPointer, pollResult: Int8) {
    ContinuationHolder.fromOpaque(ptr).resume(pollResult)
}

// Wraps UnsafeContinuation in a class so that we can use reference counting when passing it across
// the FFI
fileprivate class ContinuationHolder {
    let continuation: UnsafeContinuation<Int8, Never>

    init(_ continuation: UnsafeContinuation<Int8, Never>) {
        self.continuation = continuation
    }

    func resume(_ pollResult: Int8) {
        self.continuation.resume(returning: pollResult)
    }

    func toOpaque() -> UnsafeMutableRawPointer {
        return Unmanaged<ContinuationHolder>.passRetained(self).toOpaque()
    }

    static func fromOpaque(_ ptr: UnsafeRawPointer) -> ContinuationHolder {
        return Unmanaged<ContinuationHolder>.fromOpaque(ptr).takeRetainedValue()
    }
}

fileprivate func uniffiInitContinuationCallback() {
    {{ ci.ffi_rust_future_continuation_callback_set().name() }}(uniffiFutureContinuationCallback)
}
