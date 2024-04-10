{%- let cbi = ci|get_callback_interface_definition(name) %}
{%- let ffi_init_callback = cbi.ffi_init_callback() %}
{%- let protocol_name = type_name.clone() %}
{%- let protocol_docstring = cbi.docstring() %}
{%- let vtable = cbi.vtable() %}
{%- let methods = cbi.methods() %}
{%- let vtable_methods = cbi.vtable_methods() %}

{% include "Protocol.py" %}
{% include "CallbackInterfaceImpl.py" %}

# The _UniffiConverter which transforms the Callbacks in to Handles to pass to Rust.
{{ ffi_converter_name }} = UniffiCallbackInterfaceFfiConverter()
