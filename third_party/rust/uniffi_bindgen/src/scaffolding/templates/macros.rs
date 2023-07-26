{#
// Template to receive calls into rust.
#}

{%- macro to_rs_call(func) -%}
r#{{ func.name() }}({% call _arg_list_rs_call(func) -%})
{%- endmacro -%}

{%- macro _arg_list_rs_call(func) %}
    {%- for arg in func.full_arguments() %}
        match {{- arg.as_type().borrow()|ffi_converter }}::try_lift(r#{{ arg.name() }}) {
        {%- if arg.by_ref() %}
        {#  args passed by reference get special treatment for traits and their Box<> #}
        {%-     if arg.is_trait_ref() %}
            Ok(ref val) => &**val,
        {%-     else %}
            Ok(ref val) => val,
        {%-     endif %}
        {%- else %}
        {#  args not passed by reference get passed directly #}
            Ok(val) => val,
        {%- endif %}

        {#- If this function returns an error, we attempt to downcast errors doing arg
            conversions to this error. If the downcast fails or the function doesn't
            return an error, we just panic.
        -#}
        {%- match func.throws_type() -%}
        {%- when Some with (e) %}
            Err(err) => return Err(uniffi::lower_anyhow_error_or_panic::<crate::UniFfiTag, {{ e|type_rs }}>(err, "{{ arg.name() }}")),
        {%- else %}
            Err(err) => panic!("Failed to convert arg '{}': {}", "{{ arg.name() }}", err),
        {%- endmatch %}
        }
        {%- if !loop.last %},{% endif %}
    {%- endfor %}
{%- endmacro -%}

{#-
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
{%- when Some with (return_type) %} -> {{ return_type|ffi_converter }}::ReturnType
{%- else -%}
{%- endmatch -%}
{%- endmacro -%}

{%- macro method_decl_prelude(meth) %}
#[doc(hidden)]
#[no_mangle]
#[allow(clippy::let_unit_value,clippy::unit_arg)] // The generated code uses the unit type like other types to keep things uniform
pub extern "C" fn r#{{ meth.ffi_func().name() }}(
    {%- call arg_list_ffi_decl(meth.ffi_func()) %}
) {% call return_signature(meth) %} {
    uniffi::deps::log::debug!("{{ meth.ffi_func().name() }}");
    uniffi::rust_call(call_status, || {
        {{ meth|return_ffi_converter }}::lower_return(
{%- endmacro %}

{%- macro method_decl_postscript(meth) %}
            {% if meth.throws() %}.map_err(Into::into){% endif %}
        )
    })
}
{% endmacro %}
