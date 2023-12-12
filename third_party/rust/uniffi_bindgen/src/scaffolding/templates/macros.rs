{#
// Template to receive calls into rust.

// Arglist as used in the _UniFFILib function declarations.
// Note unfiltered name but type_ffi filters.
-#}
{%- macro arg_list_ffi_decl(func) %}
    {%- for arg in func.arguments() %}
        r#{{- arg.name() }}: {{ arg.type_().borrow()|type_ffi -}},
    {%- endfor %}
    call_status: &mut uniffi::RustCallStatus
{%- endmacro -%}

{%- macro arg_list_decl_with_prefix(prefix, meth) %}
    {{- prefix -}}
    {%- if meth.arguments().len() > 0 %}, {# whitespace #}
        {%- for arg in meth.arguments() %}
            r#{{- arg.name() }}: {{ arg.as_type().borrow()|type_rs -}}{% if loop.last %}{% else %},{% endif %}
        {%- endfor %}
    {%- endif %}
{%- endmacro -%}

{% macro return_signature(func) %}
{%- match func.return_type() %}
{%- when Some with (return_type) %} -> {{ return_type|ffi_trait("LowerReturn") }}::ReturnType
{%- else -%}
{%- endmatch -%}
{%- endmacro -%}
