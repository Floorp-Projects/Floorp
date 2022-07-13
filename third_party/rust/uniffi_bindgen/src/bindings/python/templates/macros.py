{#
// Template to call into rust. Used in several places.
// Variable names in `arg_list_decl` should match up with arg lists
// passed to rust via `_arg_list_ffi_call`
#}

{%- macro to_ffi_call(func) -%}
    {%- match func.throws_type() -%}
    {%- when Some with (e) -%}
rust_call_with_error({{ e|ffi_converter_name }},
    {%- else -%}
rust_call(
    {%- endmatch -%}
    _UniFFILib.{{ func.ffi_func().name() }},
    {%- call _arg_list_ffi_call(func) -%}
)
{%- endmacro -%}

{%- macro to_ffi_call_with_prefix(prefix, func) -%}
    {%- match func.throws_type() -%}
    {%- when Some with (e) -%}
rust_call_with_error(
    {{ e|ffi_converter_name }},
    {%- else -%}
rust_call(
    {%- endmatch -%}
    _UniFFILib.{{ func.ffi_func().name() }},
    {{- prefix }},
    {%- call _arg_list_ffi_call(func) -%}
)
{%- endmacro -%}

{%- macro _arg_list_ffi_call(func) %}
    {%- for arg in func.arguments() %}
        {{ arg|lower_fn }}({{ arg.name() }})
        {%- if !loop.last %},{% endif %}
    {%- endfor %}
{%- endmacro -%}

{#-
// Arglist as used in Python declarations of methods, functions and constructors.
// Note the var_name and type_name filters.
-#}

{% macro arg_list_decl(func) %}
    {%- for arg in func.arguments() -%}
        {{ arg.name()|var_name }}
        {%- match arg.default_value() %}
        {%- when Some with(literal) %} = {{ literal|literal_py(arg.type_().borrow()) }}
        {%- else %}
        {%- endmatch %}
        {%- if !loop.last %},{% endif -%}
    {%- endfor %}
{%- endmacro %}

{#-
// Field lists as used in Python declarations of Records.
// Note the var_name.
-#}
{%- macro field_list_decl(item) %}
    {%- for field in item.fields() -%}
        {{ field.name()|var_name }}
        {%- match field.default_value() %}
            {%- when Some with(literal) %} = {{ literal|literal_py(field) }}
            {%- else %}
        {%- endmatch -%}
        {% if !loop.last %}, {% endif %}
    {%- endfor %}
{%- endmacro %}

{#-
// Arglist as used in the _UniFFILib function declations.
// Note unfiltered name but ffi_type_name filters.
-#}
{%- macro arg_list_ffi_decl(func) %}
    {%- for arg in func.arguments() %}
    {{ arg.type_().borrow()|ffi_type_name }},
    {%- endfor %}
    ctypes.POINTER(RustCallStatus),
{% endmacro -%}

{%- macro coerce_args(func) %}
    {%- for arg in func.arguments() %}
    {{ arg.name() }} = {{ arg.name()|coerce_py(arg.type_().borrow()) -}}
    {% endfor -%}
{%- endmacro -%}

{%- macro coerce_args_extra_indent(func) %}
        {%- for arg in func.arguments() %}
        {{ arg.name() }} = {{ arg.name()|coerce_py(arg.type_().borrow()) }}
        {%- endfor %}
{%- endmacro -%}
