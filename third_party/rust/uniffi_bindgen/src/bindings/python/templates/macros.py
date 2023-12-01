{#
// Template to call into rust. Used in several places.
// Variable names in `arg_list_decl` should match up with arg lists
// passed to rust via `arg_list_lowered`
#}

{%- macro to_ffi_call(func) -%}
    {%- match func.throws_type() -%}
    {%- when Some with (e) -%}
_rust_call_with_error({{ e|ffi_converter_name }},
    {%- else -%}
_rust_call(
    {%- endmatch -%}
    _UniffiLib.{{ func.ffi_func().name() }},
    {%- call arg_list_lowered(func) -%}
)
{%- endmacro -%}

{%- macro to_ffi_call_with_prefix(prefix, func) -%}
    {%- match func.throws_type() -%}
    {%- when Some with (e) -%}
_rust_call_with_error(
    {{ e|ffi_converter_name }},
    {%- else -%}
_rust_call(
    {%- endmatch -%}
    _UniffiLib.{{ func.ffi_func().name() }},
    {{- prefix }},
    {%- call arg_list_lowered(func) -%}
)
{%- endmacro -%}

{%- macro arg_list_lowered(func) %}
    {%- for arg in func.arguments() %}
        {{ arg|lower_fn }}({{ arg.name()|var_name }})
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
        {%- when Some with(literal) %}: "typing.Union[object, {{ arg|type_name -}}]" = _DEFAULT
        {%- else %}: "{{ arg|type_name -}}"
        {%- endmatch %}
        {%- if !loop.last %},{% endif -%}
    {%- endfor %}
{%- endmacro %}

{#-
// Arglist as used in the _UniffiLib function declarations.
// Note unfiltered name but ffi_type_name filters.
-#}
{%- macro arg_list_ffi_decl(func) %}
    {%- for arg in func.arguments() %}
    {{ arg.type_().borrow()|ffi_type_name }},
    {%- endfor %}
    {%- if func.has_rust_call_status_arg() %}
    ctypes.POINTER(_UniffiRustCallStatus),{% endif %}
{% endmacro -%}

{#
 # Setup function arguments by initializing default values.
 #}
{%- macro setup_args(func) %}
    {%- for arg in func.arguments() %}
    {%- match arg.default_value() %}
    {%- when None %}
    {%- when Some with(literal) %}
    if {{ arg.name()|var_name }} is _DEFAULT:
        {{ arg.name()|var_name }} = {{ literal|literal_py(arg.as_type().borrow()) }}
    {%- endmatch %}
    {% endfor -%}
{%- endmacro -%}

{#
 # Exactly the same thing as `setup_args()` but with an extra 4 spaces of
 # indent so that it works with object methods.
 #}
{%- macro setup_args_extra_indent(func) %}
        {%- for arg in func.arguments() %}
        {%- match arg.default_value() %}
        {%- when None %}
        {%- when Some with(literal) %}
        if {{ arg.name()|var_name }} is _DEFAULT:
            {{ arg.name()|var_name }} = {{ literal|literal_py(arg.as_type().borrow()) }}
        {%- endmatch %}
        {% endfor -%}
{%- endmacro -%}

{#
 # Macro to call methods
 #}
{%- macro method_decl(py_method_name, meth) %}
{%  if meth.is_async() %}

    def {{ py_method_name }}(self, {% call arg_list_decl(meth) %}):
        {%- call setup_args_extra_indent(meth) %}
        return _uniffi_rust_call_async(
            _UniffiLib.{{ meth.ffi_func().name() }}(
                self._pointer, {% call arg_list_lowered(meth) %}
            ),
            _UniffiLib.{{ meth.ffi_rust_future_poll(ci) }},
            _UniffiLib.{{ meth.ffi_rust_future_complete(ci) }},
            _UniffiLib.{{ meth.ffi_rust_future_free(ci) }},
            # lift function
            {%- match meth.return_type() %}
            {%- when Some(return_type) %}
            {{ return_type|lift_fn }},
            {%- when None %}
            lambda val: None,
            {% endmatch %}
            # Error FFI converter
            {%- match meth.throws_type() %}
            {%- when Some(e) %}
            {{ e|ffi_converter_name }},
            {%- when None %}
            None,
            {%- endmatch %}
        )

{%- else -%}
{%-     match meth.return_type() %}

{%-         when Some with (return_type) %}

    def {{ py_method_name }}(self, {% call arg_list_decl(meth) %}) -> "{{ return_type|type_name }}":
        {%- call setup_args_extra_indent(meth) %}
        return {{ return_type|lift_fn }}(
            {% call to_ffi_call_with_prefix("self._pointer", meth) %}
        )

{%-         when None %}

    def {{ py_method_name }}(self, {% call arg_list_decl(meth) %}):
        {%- call setup_args_extra_indent(meth) %}
        {% call to_ffi_call_with_prefix("self._pointer", meth) %}
{%      endmatch %}
{%  endif %}

{% endmacro %}
