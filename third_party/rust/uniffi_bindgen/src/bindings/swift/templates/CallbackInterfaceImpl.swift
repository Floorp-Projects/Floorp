{%- if self.include_once_check("CallbackInterfaceRuntime.swift") %}{%- include "CallbackInterfaceRuntime.swift" %}{%- endif %}
{%- let trait_impl=format!("UniffiCallbackInterface{}", name) %}

// Put the implementation in a struct so we don't pollute the top-level namespace
fileprivate struct {{ trait_impl }} {

    // Create the VTable using a series of closures.
    // Swift automatically converts these into C callback functions.
    static var vtable: {{ vtable|ffi_type_name }} = {{ vtable|ffi_type_name }}(
        {%- for (ffi_callback, meth) in vtable_methods %}
        {{ meth.name()|fn_name }}: { (
            {%- for arg in ffi_callback.arguments() %}
            {{ arg.name()|var_name }}: {{ arg.type_().borrow()|ffi_type_name }}{% if !loop.last || ffi_callback.has_rust_call_status_arg() %},{% endif %}
            {%- endfor -%}
            {%- if ffi_callback.has_rust_call_status_arg() %}
            uniffiCallStatus: UnsafeMutablePointer<RustCallStatus>
            {%- endif %}
        ) in
            let makeCall = {
                () {% if meth.is_async() %}async {% endif %}throws -> {% match meth.return_type() %}{% when Some(t) %}{{ t|type_name }}{% when None %}(){% endmatch %} in
                guard let uniffiObj = try? {{ ffi_converter_name }}.handleMap.get(handle: uniffiHandle) else {
                    throw UniffiInternalError.unexpectedStaleHandle
                }
                return {% if meth.throws() %}try {% endif %}{% if meth.is_async() %}await {% endif %}uniffiObj.{{ meth.name()|fn_name }}(
                    {%- for arg in meth.arguments() %}
                    {% if !config.omit_argument_labels() %} {{ arg.name()|arg_name }}: {% endif %}try {{ arg|lift_fn }}({{ arg.name()|var_name }}){% if !loop.last %},{% endif %}
                    {%- endfor %}
                )
            }
            {%- if !meth.is_async() %}

            {% match meth.return_type() %}
            {%- when Some(t) %}
            let writeReturn = { uniffiOutReturn.pointee = {{ t|lower_fn }}($0) }
            {%- when None %}
            let writeReturn = { () }
            {%- endmatch %}

            {%- match meth.throws_type() %}
            {%- when None %}
            uniffiTraitInterfaceCall(
                callStatus: uniffiCallStatus,
                makeCall: makeCall,
                writeReturn: writeReturn
            )
            {%- when Some(error_type) %}
            uniffiTraitInterfaceCallWithError(
                callStatus: uniffiCallStatus,
                makeCall: makeCall,
                writeReturn: writeReturn,
                lowerError: {{ error_type|lower_fn }}
            )
            {%- endmatch %}
            {%- else %}

            let uniffiHandleSuccess = { (returnValue: {{ meth.return_type()|return_type_name }}) in
                uniffiFutureCallback(
                    uniffiCallbackData,
                    {{ meth.foreign_future_ffi_result_struct().name()|ffi_struct_name }}(
                        {%- match meth.return_type() %}
                        {%- when Some(return_type) %}
                        returnValue: {{ return_type|lower_fn }}(returnValue),
                        {%- when None %}
                        {%- endmatch %}
                        callStatus: RustCallStatus()
                    )
                )
            }
            let uniffiHandleError = { (statusCode, errorBuf) in
                uniffiFutureCallback(
                    uniffiCallbackData,
                    {{ meth.foreign_future_ffi_result_struct().name()|ffi_struct_name }}(
                        {%- match meth.return_type() %}
                        {%- when Some(return_type) %}
                        returnValue: {{ meth.return_type().map(FfiType::from)|ffi_default_value }},
                        {%- when None %}
                        {%- endmatch %}
                        callStatus: RustCallStatus(code: statusCode, errorBuf: errorBuf)
                    )
                )
            }

            {%- match meth.throws_type() %}
            {%- when None %}
            let uniffiForeignFuture = uniffiTraitInterfaceCallAsync(
                makeCall: makeCall,
                handleSuccess: uniffiHandleSuccess,
                handleError: uniffiHandleError
            )
            {%- when Some(error_type) %}
            let uniffiForeignFuture = uniffiTraitInterfaceCallAsyncWithError(
                makeCall: makeCall,
                handleSuccess: uniffiHandleSuccess,
                handleError: uniffiHandleError,
                lowerError: {{ error_type|lower_fn }}
            )
            {%- endmatch %}
            uniffiOutReturn.pointee = uniffiForeignFuture
            {%- endif %}
        },
        {%- endfor %}
        uniffiFree: { (uniffiHandle: UInt64) -> () in
            let result = try? {{ ffi_converter_name }}.handleMap.remove(handle: uniffiHandle)
            if result == nil {
                print("Uniffi callback interface {{ name }}: handle missing in uniffiFree")
            }
        }
    )
}

private func {{ callback_init }}() {
    {{ ffi_init_callback.name() }}(&{{ trait_impl }}.vtable)
}
