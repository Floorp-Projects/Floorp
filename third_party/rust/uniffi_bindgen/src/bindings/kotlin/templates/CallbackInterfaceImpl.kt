{% if self.include_once_check("CallbackInterfaceRuntime.kt") %}{% include "CallbackInterfaceRuntime.kt" %}{% endif %}

{%- let trait_impl=format!("uniffiCallbackInterface{}", name) %}

// Put the implementation in an object so we don't pollute the top-level namespace
internal object {{ trait_impl }} {
    {%- for (ffi_callback, meth) in vtable_methods.iter() %}
    internal object {{ meth.name()|var_name }}: {{ ffi_callback.name()|ffi_callback_name }} {
        override fun callback(
            {%- for arg in ffi_callback.arguments() -%}
            {{ arg.name().borrow()|var_name }}: {{ arg.type_().borrow()|ffi_type_name_by_value }},
            {%- endfor -%}
            {%- if ffi_callback.has_rust_call_status_arg() -%}
            uniffiCallStatus: UniffiRustCallStatus,
            {%- endif -%}
        )
        {%- match ffi_callback.return_type() %}
        {%- when Some(return_type) %}: {{ return_type|ffi_type_name_by_value }},
        {%- when None %}
        {%- endmatch %} {
            val uniffiObj = {{ ffi_converter_name }}.handleMap.get(uniffiHandle)
            val makeCall = {% if meth.is_async() %}suspend {% endif %}{ ->
                uniffiObj.{{ meth.name()|fn_name() }}(
                    {%- for arg in meth.arguments() %}
                    {{ arg|lift_fn }}({{ arg.name()|var_name }}),
                    {%- endfor %}
                )
            }
            {%- if !meth.is_async() %}

            {%- match meth.return_type() %}
            {%- when Some(return_type) %}
            val writeReturn = { value: {{ return_type|type_name(ci) }} -> uniffiOutReturn.setValue({{ return_type|lower_fn }}(value)) }
            {%- when None %}
            val writeReturn = { _: Unit -> Unit }
            {%- endmatch %}

            {%- match meth.throws_type() %}
            {%- when None %}
            uniffiTraitInterfaceCall(uniffiCallStatus, makeCall, writeReturn)
            {%- when Some(error_type) %}
            uniffiTraitInterfaceCallWithError(
                uniffiCallStatus,
                makeCall,
                writeReturn,
                { e: {{error_type|type_name(ci) }} -> {{ error_type|lower_fn }}(e) }
            )
            {%- endmatch %}

            {%- else %}
            val uniffiHandleSuccess = { {% if meth.return_type().is_some() %}returnValue{% else %}_{% endif %}: {% match meth.return_type() %}{%- when Some(return_type) %}{{ return_type|type_name(ci) }}{%- when None %}Unit{% endmatch %} ->
                val uniffiResult = {{ meth.foreign_future_ffi_result_struct().name()|ffi_struct_name }}.UniffiByValue(
                    {%- match meth.return_type() %}
                    {%- when Some(return_type) %}
                    {{ return_type|lower_fn }}(returnValue),
                    {%- when None %}
                    {%- endmatch %}
                    UniffiRustCallStatus.ByValue()
                )
                uniffiResult.write()
                uniffiFutureCallback.callback(uniffiCallbackData, uniffiResult)
            }
            val uniffiHandleError = { callStatus: UniffiRustCallStatus.ByValue ->
                uniffiFutureCallback.callback(
                    uniffiCallbackData,
                    {{ meth.foreign_future_ffi_result_struct().name()|ffi_struct_name }}.UniffiByValue(
                        {%- match meth.return_type() %}
                        {%- when Some(return_type) %}
                        {{ return_type.into()|ffi_default_value }},
                        {%- when None %}
                        {%- endmatch %}
                        callStatus,
                    ),
                )
            }

            uniffiOutReturn.uniffiSetValue(
                {%- match meth.throws_type() %}
                {%- when None %}
                uniffiTraitInterfaceCallAsync(
                    makeCall,
                    uniffiHandleSuccess,
                    uniffiHandleError
                )
                {%- when Some(error_type) %}
                uniffiTraitInterfaceCallAsyncWithError(
                    makeCall,
                    uniffiHandleSuccess,
                    uniffiHandleError,
                    { e: {{error_type|type_name(ci) }} -> {{ error_type|lower_fn }}(e) }
                )
                {%- endmatch %}
            )
            {%- endif %}
        }
    }
    {%- endfor %}

    internal object uniffiFree: {{ "CallbackInterfaceFree"|ffi_callback_name }} {
        override fun callback(handle: Long) {
            {{ ffi_converter_name }}.handleMap.remove(handle)
        }
    }

    internal var vtable = {{ vtable|ffi_type_name_by_value }}(
        {%- for (ffi_callback, meth) in vtable_methods.iter() %}
        {{ meth.name()|var_name() }},
        {%- endfor %}
        uniffiFree,
    )

    // Registers the foreign callback with the Rust side.
    // This method is generated for each callback interface.
    internal fun register(lib: UniffiLib) {
        lib.{{ ffi_init_callback.name() }}(vtable)
    }
}
