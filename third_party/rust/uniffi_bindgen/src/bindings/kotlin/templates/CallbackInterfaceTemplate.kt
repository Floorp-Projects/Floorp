{%- let cbi = ci|get_callback_interface_definition(name) %}
{%- let ffi_init_callback = cbi.ffi_init_callback() %}
{%- let interface_name = cbi|type_name(ci) %}
{%- let interface_docstring = cbi.docstring() %}
{%- let methods = cbi.methods() %}
{%- let vtable = cbi.vtable() %}
{%- let vtable_methods = cbi.vtable_methods() %}

{% include "Interface.kt" %}
{% include "CallbackInterfaceImpl.kt" %}

// The ffiConverter which transforms the Callbacks in to handles to pass to Rust.
public object {{ ffi_converter_name }}: FfiConverterCallbackInterface<{{ interface_name }}>()
