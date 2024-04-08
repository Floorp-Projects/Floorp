{#
// Template to call into rust. Used in several places.
// Variable names in `arg_list_decl` should match up with arg lists
// passed to rust via `arg_list_lowered`
#}

{%- macro to_ffi_call(func) -%}
    {%- match func.throws_type() %}
    {%- when Some with (e) %}
    rustCallWithError({{ e|type_name(ci) }})
    {%- else %}
    rustCall()
    {%- endmatch %} { _status ->
    _UniFFILib.INSTANCE.{{ func.ffi_func().name() }}({% call arg_list_lowered(func) -%} _status)
}
{%- endmacro -%}

{%- macro to_ffi_call_with_prefix(prefix, func) %}
    {%- match func.throws_type() %}
    {%- when Some with (e) %}
    rustCallWithError({{ e|type_name(ci) }})
    {%- else %}
    rustCall()
    {%- endmatch %} { _status ->
    _UniFFILib.INSTANCE.{{ func.ffi_func().name() }}(
        {{- prefix }},
        {% call arg_list_lowered(func) %}
        _status)
}
{%- endmacro %}

{%- macro arg_list_lowered(func) %}
    {%- for arg in func.arguments() %}
        {{- arg|lower_fn }}({{ arg.name()|var_name }}),
    {%- endfor %}
{%- endmacro -%}

{#-
// Arglist as used in kotlin declarations of methods, functions and constructors.
// Note the var_name and type_name filters.
-#}

{% macro arg_list_decl(func) %}
    {%- for arg in func.arguments() -%}
        {{ arg.name()|var_name }}: {{ arg|type_name(ci) }}
        {%- match arg.default_value() %}
        {%- when Some with(literal) %} = {{ literal|render_literal(arg, ci) }}
        {%- else %}
        {%- endmatch %}
        {%- if !loop.last %}, {% endif -%}
    {%- endfor %}
{%- endmacro %}

{% macro arg_list_protocol(func) %}
    {%- for arg in func.arguments() -%}
        {{ arg.name()|var_name }}: {{ arg|type_name(ci) }}
        {%- if !loop.last %}, {% endif -%}
    {%- endfor %}
{%- endmacro %}
{#-
// Arglist as used in the _UniFFILib function declarations.
// Note unfiltered name but ffi_type_name filters.
-#}
{%- macro arg_list_ffi_decl(func) %}
    {%- for arg in func.arguments() %}
        {{- arg.name()|var_name }}: {{ arg.type_().borrow()|ffi_type_name_by_value -}},
    {%- endfor %}
    {%- if func.has_rust_call_status_arg() %}_uniffi_out_err: RustCallStatus, {% endif %}
{%- endmacro -%}

// Macro for destroying fields
{%- macro destroy_fields(member) %}
    Disposable.destroy(
    {%- for field in member.fields() %}
        this.{{ field.name()|var_name }}{%- if !loop.last %}, {% endif -%}
    {% endfor -%})
{%- endmacro -%}
