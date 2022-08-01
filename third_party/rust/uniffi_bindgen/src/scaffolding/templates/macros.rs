{#
// Template to receive calls into rust.
#}

{%- macro to_rs_call(func) -%}
r#{{ func.name() }}({% call _arg_list_rs_call(func) -%})
{%- endmacro -%}

{%- macro _arg_list_rs_call(func) %}
    {%- for arg in func.full_arguments() %}
        match {{- arg.type_().borrow()|ffi_converter }}::try_lift(r#{{ arg.name() }}) {
        {%- if arg.by_ref() %}
            Ok(ref val) => val,
        {% else %}
            Ok(val) => val,
        {% endif %}

        {# If this function returns an error, we attempt to downcast errors doing arg
            conversions to this error. If the downcast fails or the function doesn't
            return an error, we just panic.
        #}
        {%- match func.throws_type() -%}
        {%- when Some with (e) %}
            Err(err) => return Err(uniffi::lower_anyhow_error_or_panic::<{{ e|ffi_converter_name }}>(err, "{{ arg.name() }}")),
        {%- else %}
            Err(err) => panic!("Failed to convert arg '{}': {}", "{{ arg.name() }}", err),
        {%- endmatch %}
        }
        {%- if !loop.last %}, {% endif %}
    {%- endfor %}
{%- endmacro -%}

{#-
// Arglist as used in the _UniFFILib function declations.
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
            r#{{- arg.name() }}: {{ arg.type_().borrow()|type_rs -}}{% if loop.last %}{% else %},{% endif %}
        {%- endfor %}
    {%- endif %}
{%- endmacro -%}

{% macro return_signature(func) %}{% match func.ffi_func().return_type() %}{% when Some with (return_type) %} -> {% call return_type_func(func) %}{%- else -%}{%- endmatch -%}{%- endmacro -%}

{% macro return_type_func(func) %}{% match func.ffi_func().return_type() %}{% when Some with (return_type) %}{{ return_type|type_ffi }}{%- else -%}(){%- endmatch -%}{%- endmacro -%}

{% macro ret(func) %}{% match func.return_type() %}{% when Some with (return_type) %}{{ return_type|ffi_converter }}::lower(_retval){% else %}_retval{% endmatch %}{% endmacro %}

{% macro construct(obj, cons) %}
    r#{{- obj.name() }}::{% call to_rs_call(cons) -%}
{% endmacro %}

{% macro to_rs_constructor_call(obj, cons) %}
{% match cons.throws_type() %}
{% when Some with (e) %}
    uniffi::call_with_result(call_status, || {
        let _new = {% call construct(obj, cons) %}.map_err(Into::into).map_err({{ e|ffi_converter }}::lower)?;
        let _arc = std::sync::Arc::new(_new);
        Ok({{ obj.type_().borrow()|ffi_converter }}::lower(_arc))
    })
{% else %}
    uniffi::call_with_output(call_status, || {
        let _new = {% call construct(obj, cons) %};
        let _arc = std::sync::Arc::new(_new);
        {{ obj.type_().borrow()|ffi_converter }}::lower(_arc)
    })
{% endmatch %}
{% endmacro %}

{% macro to_rs_method_call(obj, meth) -%}
{% match meth.throws_type() -%}
{% when Some with (e) -%}
uniffi::call_with_result(call_status, || {
    let _retval =  r#{{ obj.name() }}::{% call to_rs_call(meth) %}.map_err(Into::into).map_err({{ e|ffi_converter }}::lower)?;
    Ok({% call ret(meth) %})
})
{% else %}
uniffi::call_with_output(call_status, || {
    {% match meth.return_type() -%}
    {% when Some with (return_type) -%}
    let retval = r#{{ obj.name() }}::{% call to_rs_call(meth) %};
    {{ return_type|ffi_converter }}::lower(retval)
    {% else -%}
    r#{{ obj.name() }}::{% call to_rs_call(meth) %}
    {% endmatch -%}
})
{% endmatch -%}
{% endmacro -%}

{% macro to_rs_function_call(func) %}
{% match func.throws_type() %}
{% when Some with (e) %}
uniffi::call_with_result(call_status, || {
    let _retval = {% call to_rs_call(func) %}.map_err(Into::into).map_err({{ e|ffi_converter }}::lower)?;
    Ok({% call ret(func) %})
})
{% else %}
uniffi::call_with_output(call_status, || {
    {% match func.return_type() -%}
    {% when Some with (return_type) -%}
    {{ return_type|ffi_converter }}::lower({% call to_rs_call(func) %})
    {% else -%}
    {% if func.full_arguments().is_empty() %}#[allow(clippy::redundant_closure)]{% endif %}
    {% call to_rs_call(func) %}
    {% endmatch -%}
})
{% endmatch %}
{% endmacro %}
