{#
// Template to call into rust. Used in several places.
// Variable names in `arg_list_decl` should match up with arg lists
// passed to rust via `_arg_list_ffi_call` (we use  `var_name_rb` in `lower_rb`)
#}

{%- macro to_ffi_call(func) -%}
    {%- match func.throws_name() -%}
    {%- when Some with (e) -%}
      {{ ci.namespace()|class_name_rb }}.rust_call_with_error({{ e|class_name_rb }},
    {%- else -%}
      {{ ci.namespace()|class_name_rb }}.rust_call(
    {%- endmatch -%}
    :{{ func.ffi_func().name() }},
    {%- call _arg_list_ffi_call(func) -%}
)
{%- endmacro -%}

{%- macro to_ffi_call_with_prefix(prefix, func) -%}
    {%- match func.throws_name() -%}
    {%- when Some with (e) -%}
      {{ ci.namespace()|class_name_rb }}.rust_call_with_error({{ e|class_name_rb }},
    {%- else -%}
      {{ ci.namespace()|class_name_rb }}.rust_call(
    {%- endmatch -%}
    :{{ func.ffi_func().name() }},
    {{- prefix }},
    {%- call _arg_list_ffi_call(func) -%}
)
{%- endmacro -%}

{%- macro _arg_list_ffi_call(func) %}
    {%- for arg in func.arguments() %}
        {{- arg.name()|lower_rb(arg.as_type().borrow()) }}
        {%- if !loop.last %},{% endif %}
    {%- endfor %}
{%- endmacro -%}

{#-
// Arglist as used in Ruby declarations of methods, functions and constructors.
// Note the var_name_rb and type_rb filters.
-#}

{% macro arg_list_decl(func) %}
    {%- for arg in func.arguments() -%}
        {{ arg.name()|var_name_rb }}
        {%- match arg.default_value() %}
        {%- when Some with(literal) %} = {{ literal|literal_rb }}
        {%- else %}
        {%- endmatch %}
        {%- if !loop.last %}, {% endif -%}
    {%- endfor %}
{%- endmacro %}

{#-
// Arglist as used in the UniFFILib function declarations.
// Note unfiltered name but type_ffi filters.
-#}
{%- macro arg_list_ffi_decl(func) %}
    [{%- for arg in func.arguments() -%}{{ arg.type_().borrow()|type_ffi }}, {% endfor -%} RustCallStatus.by_ref]
{%- endmacro -%}

{%- macro coerce_args(func) %}
    {%- for arg in func.arguments() %}
    {{ arg.name() }} = {{ arg.name()|coerce_rb(ci.namespace()|class_name_rb, arg.as_type().borrow()) -}}
    {% endfor -%}
{%- endmacro -%}

{%- macro coerce_args_extra_indent(func) %}
        {%- for arg in func.arguments() %}
        {{ arg.name() }} = {{ arg.name()|coerce_rb(ci.namespace()|class_name_rb, arg.as_type().borrow()) }}
        {%- endfor %}
{%- endmacro -%}
