# A handful of classes and functions to support the generated data structures.
# This would be a good candidate for isolating in its own ffi-support lib.

class InternalError(Exception):
    pass

class RustCallStatus(ctypes.Structure):
    """
    Error runtime.
    """
    _fields_ = [
        ("code", ctypes.c_int8),
        ("error_buf", RustBuffer),
    ]

    # These match the values from the uniffi::rustcalls module
    CALL_SUCCESS = 0
    CALL_ERROR = 1
    CALL_PANIC = 2

    def __str__(self):
        if self.code == RustCallStatus.CALL_SUCCESS:
            return "RustCallStatus(CALL_SUCCESS)"
        elif self.code == RustCallStatus.CALL_ERROR:
            return "RustCallStatus(CALL_ERROR)"
        elif self.code == RustCallStatus.CALL_PANIC:
            return "RustCallStatus(CALL_PANIC)"
        else:
            return "RustCallStatus(<invalid code>)"

def rust_call(fn, *args):
    # Call a rust function
    return rust_call_with_error(None, fn, *args)

def rust_call_with_error(error_ffi_converter, fn, *args):
    # Call a rust function and handle any errors
    #
    # This function is used for rust calls that return Result<> and therefore can set the CALL_ERROR status code.
    # error_ffi_converter must be set to the FfiConverter for the error class that corresponds to the result.
    call_status = RustCallStatus(code=RustCallStatus.CALL_SUCCESS, error_buf=RustBuffer(0, 0, None))

    args_with_error = args + (ctypes.byref(call_status),)
    result = fn(*args_with_error)
    uniffi_check_call_status(error_ffi_converter, call_status)
    return result

def rust_call_async(scaffolding_fn, callback_fn, *args):
    # Call the scaffolding function, passing it a callback handler for `AsyncTypes.py` and a pointer
    # to a python Future object.  The async function then awaits the Future.
    uniffi_eventloop = asyncio.get_running_loop()
    uniffi_py_future = uniffi_eventloop.create_future()
    uniffi_call_status = RustCallStatus(code=RustCallStatus.CALL_SUCCESS, error_buf=RustBuffer(0, 0, None))
    scaffolding_fn(*args,
       FfiConverterForeignExecutor._pointer_manager.new_pointer(uniffi_eventloop),
       callback_fn,
       # Note: It's tempting to skip the pointer manager and just use a `py_object` pointing to a
       # local variable like we do in Swift.  However, Python doesn't use cooperative cancellation
       # -- asyncio can cancel a task at anytime.  This means if we use a local variable, the Rust
       # callback could fire with a dangling pointer.
       UniFfiPyFuturePointerManager.new_pointer(uniffi_py_future),
       ctypes.byref(uniffi_call_status),
    )
    uniffi_check_call_status(None, uniffi_call_status)
    return uniffi_py_future

def uniffi_check_call_status(error_ffi_converter, call_status):
    if call_status.code == RustCallStatus.CALL_SUCCESS:
        pass
    elif call_status.code == RustCallStatus.CALL_ERROR:
        if error_ffi_converter is None:
            call_status.error_buf.free()
            raise InternalError("rust_call_with_error: CALL_ERROR, but error_ffi_converter is None")
        else:
            raise error_ffi_converter.lift(call_status.error_buf)
    elif call_status.code == RustCallStatus.CALL_PANIC:
        # When the rust code sees a panic, it tries to construct a RustBuffer
        # with the message.  But if that code panics, then it just sends back
        # an empty buffer.
        if call_status.error_buf.len > 0:
            msg = FfiConverterString.lift(call_status.error_buf)
        else:
            msg = "Unknown rust panic"
        raise InternalError(msg)
    else:
        raise InternalError("Invalid RustCallStatus code: {}".format(
            call_status.code))

# A function pointer for a callback as defined by UniFFI.
# Rust definition `fn(handle: u64, method: u32, args: RustBuffer, buf_ptr: *mut RustBuffer) -> int`
FOREIGN_CALLBACK_T = ctypes.CFUNCTYPE(ctypes.c_int, ctypes.c_ulonglong, ctypes.c_ulong, ctypes.POINTER(ctypes.c_char), ctypes.c_int, ctypes.POINTER(RustBuffer))
