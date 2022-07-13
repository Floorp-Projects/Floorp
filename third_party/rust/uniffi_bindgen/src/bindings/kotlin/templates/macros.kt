{#
// Template to call into rust. Used in several places.
// Variable names in `arg_list_decl` should match up with arg lists
// passed to rust via `_arg_list_ffi_call`
#}

{%- macro to_ffi_call(func) -%}
    {%- match func.throws() %}
    {%- when Some with (e) %}
    rustCallWithError({{ e|exception_name}})
    {%- else %}
    rustCall()
    {%- endmatch %} { _status ->
    _UniFFILib.INSTANCE.{{ func.ffi_func().name() }}({% call _arg_list_ffi_call(func) -%}{% if func.arguments().len() > 0 %},{% endif %} _status)
}
{%- endmacro -%}

{%- macro to_ffi_call_with_prefix(prefix, func) %}
    {%- match func.throws() %}
    {%- when Some with (e) %}
    rustCallWithError({{ e|exception_name}})
    {%- else %}
    rustCall()
    {%- endmatch %} { _status ->
    _UniFFILib.INSTANCE.{{ func.ffi_func().name() }}(
        {{- prefix }}, {% call _arg_list_ffi_call(func) %}{% if func.arguments().len() > 0 %}, {% endif %} _status)
}
{%- endmacro %}

{%- macro _arg_list_ffi_call(func) %}
    {%- for arg in func.arguments() %}
        {{- arg|lower_fn }}({{ arg.name()|var_name }})
        {%- if !loop.last %}, {% endif %}
    {%- endfor %}
{%- endmacro -%}

{#-
// Arglist as used in kotlin declarations of methods, functions and constructors.
// Note the var_name and type_name filters.
-#}

{% macro arg_list_decl(func) %}
    {%- for arg in func.arguments() -%}
        {{ arg.name()|var_name }}: {{ arg|type_name -}}
        {%- match arg.default_value() %}
        {%- when Some with(literal) %} = {{ literal|render_literal(arg) }}
        {%- else %}
        {%- endmatch %}
        {%- if !loop.last %}, {% endif -%}
    {%- endfor %}
{%- endmacro %}

{% macro arg_list_protocol(func) %}
    {%- for arg in func.arguments() -%}
        {{ arg.name()|var_name }}: {{ arg|type_name -}}
        {%- if !loop.last %}, {% endif -%}
    {%- endfor %}
{%- endmacro %}
{#-
// Arglist as used in the _UniFFILib function declations.
// Note unfiltered name but ffi_type_name filters.
-#}
{%- macro arg_list_ffi_decl(func) %}
    {%- for arg in func.arguments() %}
        {{- arg.name() }}: {{ arg.type_().borrow()|ffi_type_name -}},
    {%- endfor %}
    _uniffi_out_err: RustCallStatus
{%- endmacro -%}

// Macro for destroying fields
{%- macro destroy_fields(member) %}
    Disposable.destroy(
    {%- for field in member.fields() %}
        this.{{ field.name()|var_name }}{%- if !loop.last %}, {% endif -%}
    {% endfor -%})
{%- endmacro -%}

{%- macro ffi_function_definition(func) %}
fun {{ func.name() }}(
    {%- call arg_list_ffi_decl(func) %}
){%- match func.return_type() -%}{%- when Some with (type_) %}: {{ type_|ffi_type_name }}{% when None %}: Unit{% endmatch %}
{% endmacro %}
