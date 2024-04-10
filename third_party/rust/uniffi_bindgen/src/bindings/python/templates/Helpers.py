# A handful of classes and functions to support the generated data structures.
# This would be a good candidate for isolating in its own ffi-support lib.

class InternalError(Exception):
    pass

class _UniffiRustCallStatus(ctypes.Structure):
    """
    Error runtime.
    """
    _fields_ = [
        ("code", ctypes.c_int8),
        ("error_buf", _UniffiRustBuffer),
    ]

    # These match the values from the uniffi::rustcalls module
    CALL_SUCCESS = 0
    CALL_ERROR = 1
    CALL_UNEXPECTED_ERROR = 2

    @staticmethod
    def default():
        return _UniffiRustCallStatus(code=_UniffiRustCallStatus.CALL_SUCCESS, error_buf=_UniffiRustBuffer.default())

    def __str__(self):
        if self.code == _UniffiRustCallStatus.CALL_SUCCESS:
            return "_UniffiRustCallStatus(CALL_SUCCESS)"
        elif self.code == _UniffiRustCallStatus.CALL_ERROR:
            return "_UniffiRustCallStatus(CALL_ERROR)"
        elif self.code == _UniffiRustCallStatus.CALL_UNEXPECTED_ERROR:
            return "_UniffiRustCallStatus(CALL_UNEXPECTED_ERROR)"
        else:
            return "_UniffiRustCallStatus(<invalid code>)"

def _rust_call(fn, *args):
    # Call a rust function
    return _rust_call_with_error(None, fn, *args)

def _rust_call_with_error(error_ffi_converter, fn, *args):
    # Call a rust function and handle any errors
    #
    # This function is used for rust calls that return Result<> and therefore can set the CALL_ERROR status code.
    # error_ffi_converter must be set to the _UniffiConverter for the error class that corresponds to the result.
    call_status = _UniffiRustCallStatus.default()

    args_with_error = args + (ctypes.byref(call_status),)
    result = fn(*args_with_error)
    _uniffi_check_call_status(error_ffi_converter, call_status)
    return result

def _uniffi_check_call_status(error_ffi_converter, call_status):
    if call_status.code == _UniffiRustCallStatus.CALL_SUCCESS:
        pass
    elif call_status.code == _UniffiRustCallStatus.CALL_ERROR:
        if error_ffi_converter is None:
            call_status.error_buf.free()
            raise InternalError("_rust_call_with_error: CALL_ERROR, but error_ffi_converter is None")
        else:
            raise error_ffi_converter.lift(call_status.error_buf)
    elif call_status.code == _UniffiRustCallStatus.CALL_UNEXPECTED_ERROR:
        # When the rust code sees a panic, it tries to construct a _UniffiRustBuffer
        # with the message.  But if that code panics, then it just sends back
        # an empty buffer.
        if call_status.error_buf.len > 0:
            msg = _UniffiConverterString.lift(call_status.error_buf)
        else:
            msg = "Unknown rust panic"
        raise InternalError(msg)
    else:
        raise InternalError("Invalid _UniffiRustCallStatus code: {}".format(
            call_status.code))

def _uniffi_trait_interface_call(call_status, make_call, write_return_value):
    try:
        return write_return_value(make_call())
    except Exception as e:
        call_status.code = _UniffiRustCallStatus.CALL_UNEXPECTED_ERROR
        call_status.error_buf = {{ Type::String.borrow()|lower_fn }}(repr(e))

def _uniffi_trait_interface_call_with_error(call_status, make_call, write_return_value, error_type, lower_error):
    try:
        try:
            return write_return_value(make_call())
        except error_type as e:
            call_status.code = _UniffiRustCallStatus.CALL_ERROR
            call_status.error_buf = lower_error(e)
    except Exception as e:
        call_status.code = _UniffiRustCallStatus.CALL_UNEXPECTED_ERROR
        call_status.error_buf = {{ Type::String.borrow()|lower_fn }}(repr(e))
