{%- let mod_name = crate_name|fn_name %}

{%- let ffi_converter_name = "FfiConverterType{}"|format(name) %}
{{ self.add_import_of(mod_name, ffi_converter_name) }}

{%- let rustbuffer_local_name = "RustBuffer{}"|format(name) %}
{{ self.add_import_of_as(mod_name, "RustBuffer", rustbuffer_local_name) }}
