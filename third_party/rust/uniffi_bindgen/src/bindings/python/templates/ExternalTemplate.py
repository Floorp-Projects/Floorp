{%- let module = python_config.module_for_namespace(namespace) -%}

# External type {{name}} is in namespace "{{namespace}}", crate {{module_path}}
{%- let ffi_converter_name = "_UniffiConverterType{}"|format(name) %}
{{ self.add_import_of(module, ffi_converter_name) }}
{{ self.add_import_of(module, name|class_name) }} {#- import the type alias itself -#}

{%- let rustbuffer_local_name = "_UniffiRustBuffer{}"|format(name) %}
{{ self.add_import_of_as(module, "_UniffiRustBuffer", rustbuffer_local_name) }}
