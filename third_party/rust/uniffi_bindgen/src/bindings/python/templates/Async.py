# RustFuturePoll values
_UNIFFI_RUST_FUTURE_POLL_READY = 0
_UNIFFI_RUST_FUTURE_POLL_MAYBE_READY = 1

# Stores futures for _uniffi_continuation_callback
_UniffiContinuationPointerManager = _UniffiPointerManager()

# Continuation callback for async functions
# lift the return value or error and resolve the future, causing the async function to resume.
@_UNIFFI_FUTURE_CONTINUATION_T
def _uniffi_continuation_callback(future_ptr, poll_code):
    (eventloop, future) = _UniffiContinuationPointerManager.release_pointer(future_ptr)
    eventloop.call_soon_threadsafe(_uniffi_set_future_result, future, poll_code)

def _uniffi_set_future_result(future, poll_code):
    if not future.cancelled():
        future.set_result(poll_code)

async def _uniffi_rust_call_async(rust_future, ffi_poll, ffi_complete, ffi_free, lift_func, error_ffi_converter):
    try:
        eventloop = asyncio.get_running_loop()

        # Loop and poll until we see a _UNIFFI_RUST_FUTURE_POLL_READY value
        while True:
            future = eventloop.create_future()
            ffi_poll(
                rust_future,
                _UniffiContinuationPointerManager.new_pointer((eventloop, future)),
            )
            poll_code = await future
            if poll_code == _UNIFFI_RUST_FUTURE_POLL_READY:
                break

        return lift_func(
            _rust_call_with_error(error_ffi_converter, ffi_complete, rust_future)
        )
    finally:
        ffi_free(rust_future)

_UniffiLib.{{ ci.ffi_rust_future_continuation_callback_set().name() }}(_uniffi_continuation_callback)
