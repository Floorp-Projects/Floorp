{%- let ns = namespace|fn_name %}

# External type {{name}} is in namespace "{{namespace}}", crate {{module_path}}
{%- let ffi_converter_name = "_UniffiConverterType{}"|format(name) %}
{{ self.add_import_of(ns, ffi_converter_name) }}
{{ self.add_import_of(ns, name) }} {#- import the type alias itself -#}

{%- let rustbuffer_local_name = "_UniffiRustBuffer{}"|format(name) %}
{{ self.add_import_of_as(ns, "_UniffiRustBuffer", rustbuffer_local_name) }}
