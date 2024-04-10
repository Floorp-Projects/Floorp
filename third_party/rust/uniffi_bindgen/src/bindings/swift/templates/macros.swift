{#
// Template to call into rust. Used in several places.
// Variable names in `arg_list_decl` should match up with arg lists
// passed to rust via `arg_list_lowered`
#}

{%- macro to_ffi_call(func) -%}
    {%- call try(func) -%}
    {%- match func.throws_type() -%}
    {%- when Some with (e) -%}
        rustCallWithError({{ e|ffi_error_converter_name }}.lift) {
    {%- else -%}
        rustCall() {
    {%- endmatch %}
    {{ func.ffi_func().name() }}(
        {%- if func.takes_self() %}self.uniffiClonePointer(),{% endif %}
        {%- call arg_list_lowered(func) -%} $0
    )
}
{%- endmacro -%}

// eg, `public func foo_bar() { body }`
{%- macro func_decl(func_decl, callable, indent) %}
{%- call docstring(callable, indent) %}
{{ func_decl }} {{ callable.name()|fn_name }}(
    {%- call arg_list_decl(callable) -%})
    {%- call async(callable) %}
    {%- call throws(callable) %}
    {%- match callable.return_type() %}
    {%-  when Some with (return_type) %} -> {{ return_type|type_name }}
    {%-  when None %}
    {%- endmatch %} {
    {%- call call_body(callable) %}
}
{%- endmacro %}

// primary ctor - no name, no return-type.
{%- macro ctor_decl(callable, indent) %}
{%- call docstring(callable, indent) %}
public convenience init(
    {%- call arg_list_decl(callable) -%}) {%- call async(callable) %} {%- call throws(callable) %} {
    {%- if callable.is_async() %}
    let pointer =
        {%- call call_async(callable) %}
        {# The async mechanism returns an already constructed self.
           We work around that by cloning the pointer from that object, then
           assune the old object dies as there are no other references possible.
        #}
        .uniffiClonePointer()
    {%- else %}
    let pointer =
        {% call to_ffi_call(callable) %}
    {%- endif %}
    self.init(unsafeFromRawPointer: pointer)
}
{%- endmacro %}

{%- macro call_body(callable) %}
{%- if callable.is_async() %}
    return {%- call call_async(callable) %}
{%- else %}
{%-     match callable.return_type() -%}
{%-         when Some with (return_type) %}
    return {% call try(callable) %} {{ return_type|lift_fn }}({% call to_ffi_call(callable) %})
{%-         when None %}
{%-             call to_ffi_call(callable) %}
{%-     endmatch %}
{%- endif %}

{%- endmacro %}

{%- macro call_async(callable) %}
        {% call try(callable) %} await uniffiRustCallAsync(
            rustFutureFunc: {
                {{ callable.ffi_func().name() }}(
                    {%- if callable.takes_self() %}
                    self.uniffiClonePointer(){% if !callable.arguments().is_empty() %},{% endif %}
                    {% endif %}
                    {%- for arg in callable.arguments() -%}
                    {{ arg|lower_fn }}({{ arg.name()|var_name }}){% if !loop.last %},{% endif %}
                    {%- endfor %}
                )
            },
            pollFunc: {{ callable.ffi_rust_future_poll(ci) }},
            completeFunc: {{ callable.ffi_rust_future_complete(ci) }},
            freeFunc: {{ callable.ffi_rust_future_free(ci) }},
            {%- match callable.return_type() %}
            {%- when Some(return_type) %}
            liftFunc: {{ return_type|lift_fn }},
            {%- when None %}
            liftFunc: { $0 },
            {%- endmatch %}
            {%- match callable.throws_type() %}
            {%- when Some with (e) %}
            errorHandler: {{ e|ffi_error_converter_name }}.lift
            {%- else %}
            errorHandler: nil
            {% endmatch %}
        )
{%- endmacro %}

{%- macro arg_list_lowered(func) %}
    {%- for arg in func.arguments() %}
        {{ arg|lower_fn }}({{ arg.name()|var_name }}),
    {%- endfor %}
{%- endmacro -%}

{#-
// Arglist as used in Swift declarations of methods, functions and constructors.
// Note the var_name and type_name filters.
-#}

{% macro arg_list_decl(func) %}
    {%- for arg in func.arguments() -%}
        {% if config.omit_argument_labels() %}_ {% endif %}{{ arg.name()|var_name }}: {{ arg|type_name -}}
        {%- match arg.default_value() %}
        {%- when Some with(literal) %} = {{ literal|literal_swift(arg) }}
        {%- else %}
        {%- endmatch %}
        {%- if !loop.last %}, {% endif -%}
    {%- endfor %}
{%- endmacro %}

{#-
// Field lists as used in Swift declarations of Records and Enums.
// Note the var_name and type_name filters.
-#}
{% macro field_list_decl(item, has_nameless_fields) %}
    {%- for field in item.fields() -%}
        {%- call docstring(field, 8) %}
        {%- if has_nameless_fields %}
        {{- field|type_name -}}
        {%- if !loop.last -%}, {%- endif -%}
        {%- else -%}
        {{ field.name()|var_name }}: {{ field|type_name -}}
        {%- match field.default_value() %}
            {%- when Some with(literal) %} = {{ literal|literal_swift(field) }}
            {%- else %}
        {%- endmatch -%}
        {% if !loop.last %}, {% endif %}
        {%- endif -%}
    {%- endfor %}
{%- endmacro %}

{% macro field_name(field, field_num) %}
{%- if field.name().is_empty() -%}
v{{- field_num -}}
{%- else -%}
{{ field.name()|var_name }}
{%- endif -%}
{%- endmacro %}

{% macro arg_list_protocol(func) %}
    {%- for arg in func.arguments() -%}
        {% if config.omit_argument_labels() %}_ {% endif %}{{ arg.name()|var_name }}: {{ arg|type_name -}}
        {%- if !loop.last %}, {% endif -%}
    {%- endfor %}
{%- endmacro %}

{%- macro async(func) %}
{%- if func.is_async() %}async {% endif %}
{%- endmacro -%}

{%- macro throws(func) %}
{%- if func.throws() %}throws {% endif %}
{%- endmacro -%}

{%- macro try(func) %}
{%- if func.throws() %}try {% else %}try! {% endif %}
{%- endmacro -%}

{%- macro docstring_value(maybe_docstring, indent_spaces) %}
{%- match maybe_docstring %}
{%- when Some(docstring) %}
{{ docstring|docstring(indent_spaces) }}
{%- else %}
{%- endmatch %}
{%- endmacro %}

{%- macro docstring(defn, indent_spaces) %}
{%- call docstring_value(defn.docstring(), indent_spaces) %}
{%- endmacro %}
