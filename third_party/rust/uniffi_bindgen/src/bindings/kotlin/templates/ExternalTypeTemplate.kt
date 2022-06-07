{%- let package_name=self.external_type_package_name(crate_name) %}
{%- let fully_qualified_type_name = "{}.{}"|format(package_name, name) %}
{%- let fully_qualified_ffi_converter_name = "{}.FfiConverterType{}"|format(package_name, name) %}

{{- self.add_import(fully_qualified_type_name) }}
{{ self.add_import(fully_qualified_ffi_converter_name) }}
