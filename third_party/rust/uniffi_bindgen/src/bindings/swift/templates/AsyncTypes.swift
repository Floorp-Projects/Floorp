// Callbacks for async functions

// Callback handlers for an async calls.  These are invoked by Rust when the future is ready.  They
// lift the return value or error and resume the suspended function.
{%- for result_type in ci.iter_async_result_types() %}
fileprivate func {{ result_type|future_callback }}(
    rawContinutation: UnsafeRawPointer,
    returnValue: {{ result_type.future_callback_param().borrow()|ffi_type_name }},
    callStatus: RustCallStatus) {

    let continuation = rawContinutation.bindMemory(
        to: {{ result_type|future_continuation_type }}.self,
        capacity: 1
    )

    do {
        try uniffiCheckCallStatus(callStatus: callStatus, errorHandler: {{ result_type|error_handler }})
        {%- match result_type.return_type %}
        {%- when Some(return_type) %}
        continuation.pointee.resume(returning: try {{ return_type|lift_fn }}(returnValue))
        {%- when None %}
        continuation.pointee.resume(returning: ())
        {%- endmatch %}
    } catch let error {
        continuation.pointee.resume(throwing: error)
    }
}

{%- endfor %}
