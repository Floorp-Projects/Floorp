// Async return type handlers

{# add imports that we use #}
{{ self.add_import("kotlin.coroutines.Continuation") }}
{{ self.add_import("kotlin.coroutines.resume") }}
{{ self.add_import("kotlin.coroutines.resumeWithException") }}

{# We use these in the generated functions, which don't have access to add_import() -- might as well add it here #}
{{ self.add_import("kotlin.coroutines.suspendCoroutine") }}
{{ self.add_import("kotlinx.coroutines.coroutineScope") }}


// FFI type for callback handlers
{%- for callback_param in ci.iter_future_callback_params()|unique_ffi_types %}
internal interface UniFfiFutureCallback{{ callback_param|ffi_type_name }} : com.sun.jna.Callback {
    // Note: callbackData is always 0.  We could pass Rust a pointer/usize to represent the
    // continuation, but with JNA it's easier to just store it in the callback handler.
    fun invoke(_callbackData: USize, returnValue: {{ callback_param|ffi_type_name_by_value }}?, callStatus: RustCallStatus.ByValue);
}
{%- endfor %}

// Callback handlers for an async call.  These are invoked by Rust when the future is ready.  They
// lift the return value or error and resume the suspended function.
{%- for result_type in ci.iter_async_result_types() %}
{%- let callback_param = result_type.future_callback_param() %}

internal class {{ result_type|future_callback_handler }}(val continuation: {{ result_type|future_continuation_type }})
    : UniFfiFutureCallback{{ callback_param|ffi_type_name }} {
    override fun invoke(_callbackData: USize, returnValue: {{ callback_param|ffi_type_name_by_value }}?, callStatus: RustCallStatus.ByValue) {
        try {
            checkCallStatus({{ result_type|error_handler }}, callStatus)
            {%- match result_type.return_type %}
            {%- when Some(return_type) %}
            continuation.resume({{ return_type|lift_fn }}(returnValue!!))
            {%- when None %}
            continuation.resume(Unit)
            {%- endmatch %}
        } catch (e: Throwable) {
            continuation.resumeWithException(e)
        }
    }
}
{%- endfor %}
