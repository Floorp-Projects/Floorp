# Callback handlers for async returns

UniFfiPyFuturePointerManager = UniFfiPointerManager()

# Callback handlers for an async calls.  These are invoked by Rust when the future is ready.  They
# lift the return value or error and resolve the future, causing the async function to resume.
{%- for result_type in ci.iter_async_result_types() %}
@uniffi_future_callback_t(
        {%- match result_type.return_type -%}
        {%- when Some(return_type) -%}
        {{ return_type.ffi_type().borrow()|ffi_type_name }}
        {%- when None -%}
        ctypes.c_uint8
        {%- endmatch -%}
)
def {{ result_type|async_callback_fn }}(future_ptr, result, call_status):
    future = UniFfiPyFuturePointerManager.release_pointer(future_ptr)
    if future.cancelled():
        return
    try:
        {%- match result_type.throws_type %}
        {%- when Some(throws_type) %}
        uniffi_check_call_status({{ throws_type|ffi_converter_name }}, call_status)
        {%- when None %}
        uniffi_check_call_status(None, call_status)
        {%- endmatch %}

        {%- match result_type.return_type %}
        {%- when Some(return_type) %}
        future.set_result({{ return_type|lift_fn }}(result))
        {%- when None %}
        future.set_result(None)
        {%- endmatch %}
    except BaseException as e:
        future.set_exception(e)
{%- endfor %}
