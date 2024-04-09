{% if self.include_once_check("CallbackInterfaceRuntime.py") %}{% include "CallbackInterfaceRuntime.py" %}{% endif %}
{%- let trait_impl=format!("UniffiTraitImpl{}", name) %}

# Put all the bits inside a class to keep the top-level namespace clean
class {{ trait_impl }}:
    # For each method, generate a callback function to pass to Rust
    {%- for (ffi_callback, meth) in vtable_methods.iter() %}

    @{{ ffi_callback.name()|ffi_callback_name }}
    def {{ meth.name()|fn_name }}(
            {%- for arg in ffi_callback.arguments() %}
            {{ arg.name()|var_name }},
            {%- endfor -%}
            {%- if ffi_callback.has_rust_call_status_arg() %}
            uniffi_call_status_ptr,
            {%- endif %}
        ):
        uniffi_obj = {{ ffi_converter_name }}._handle_map.get(uniffi_handle)
        def make_call():
            args = ({% for arg in meth.arguments() %}{{ arg|lift_fn }}({{ arg.name()|var_name }}), {% endfor %})
            method = uniffi_obj.{{ meth.name()|fn_name }}
            return method(*args)

        {% if !meth.is_async() %}
        {%- match meth.return_type() %}
        {%- when Some(return_type) %}
        def write_return_value(v):
            uniffi_out_return[0] = {{ return_type|lower_fn }}(v)
        {%- when None %}
        write_return_value = lambda v: None
        {%- endmatch %}

        {%- match meth.throws_type() %}
        {%- when None %}
        _uniffi_trait_interface_call(
                uniffi_call_status_ptr.contents,
                make_call,
                write_return_value,
        )
        {%- when Some(error) %}
        _uniffi_trait_interface_call_with_error(
                uniffi_call_status_ptr.contents,
                make_call,
                write_return_value,
                {{ error|type_name }},
                {{ error|lower_fn }},
        )
        {%- endmatch %}
        {%- else %}
        def handle_success(return_value):
            uniffi_future_callback(
                uniffi_callback_data,
                {{ meth.foreign_future_ffi_result_struct().name()|ffi_struct_name }}(
                    {%- match meth.return_type() %}
                    {%- when Some(return_type) %}
                    {{ return_type|lower_fn }}(return_value),
                    {%- when None %}
                    {%- endmatch %}
                    _UniffiRustCallStatus.default()
                )
            )

        def handle_error(status_code, rust_buffer):
            uniffi_future_callback(
                uniffi_callback_data,
                {{ meth.foreign_future_ffi_result_struct().name()|ffi_struct_name }}(
                    {%- match meth.return_type() %}
                    {%- when Some(return_type) %}
                    {{ meth.return_type().map(FfiType::from)|ffi_default_value }},
                    {%- when None %}
                    {%- endmatch %}
                    _UniffiRustCallStatus(status_code, rust_buffer),
                )
            )

        {%- match meth.throws_type() %}
        {%- when None %}
        uniffi_out_return[0] = uniffi_trait_interface_call_async(make_call, handle_success, handle_error)
        {%- when Some(error) %}
        uniffi_out_return[0] = uniffi_trait_interface_call_async_with_error(make_call, handle_success, handle_error, {{ error|type_name }}, {{ error|lower_fn }})
        {%- endmatch %}
        {%- endif %}
    {%- endfor %}

    @{{ "CallbackInterfaceFree"|ffi_callback_name }}
    def uniffi_free(uniffi_handle):
        {{ ffi_converter_name }}._handle_map.remove(uniffi_handle)

    # Generate the FFI VTable.  This has a field for each callback interface method.
    uniffi_vtable = {{ vtable|ffi_type_name }}(
        {%- for (_, meth) in vtable_methods.iter() %}
        {{ meth.name()|fn_name }},
        {%- endfor %}
        uniffi_free
    )
    # Send Rust a pointer to the VTable.  Note: this means we need to keep the struct alive forever,
    # or else bad things will happen when Rust tries to access it.
    _UniffiLib.{{ ffi_init_callback.name() }}(ctypes.byref(uniffi_vtable))
