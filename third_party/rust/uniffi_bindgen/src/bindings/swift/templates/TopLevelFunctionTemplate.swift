{%- if func.is_async() %}

public func {{ func.name()|fn_name }}({%- call swift::arg_list_decl(func) -%}) async {% call swift::throws(func) %}{% match func.return_type() %}{% when Some with (return_type) %} -> {{ return_type|type_name }}{% when None %}{% endmatch %} {
    return {% call swift::try(func) %} await uniffiRustCallAsync(
        rustFutureFunc: {
            {{ func.ffi_func().name() }}(
                {%- for arg in func.arguments() %}
                {{ arg|lower_fn }}({{ arg.name()|var_name }}){% if !loop.last %},{% endif %}
                {%- endfor %}
            )
        },
        pollFunc: {{ func.ffi_rust_future_poll(ci) }},
        completeFunc: {{ func.ffi_rust_future_complete(ci) }},
        freeFunc: {{ func.ffi_rust_future_free(ci) }},
        {%- match func.return_type() %}
        {%- when Some(return_type) %}
        liftFunc: {{ return_type|lift_fn }},
        {%- when None %}
        liftFunc: { $0 },
        {%- endmatch %}
        {%- match func.throws_type() %}
        {%- when Some with (e) %}
        errorHandler: {{ e|ffi_converter_name }}.lift
        {%- else %}
        errorHandler: nil
        {% endmatch %}
    )
}

{% else %}

{%- match func.return_type() -%}
{%- when Some with (return_type) %}

public func {{ func.name()|fn_name }}({%- call swift::arg_list_decl(func) -%}) {% call swift::throws(func) %} -> {{ return_type|type_name }} {
    return {% call swift::try(func) %} {{ return_type|lift_fn }}(
        {% call swift::to_ffi_call(func) %}
    )
}

{%- when None %}

public func {{ func.name()|fn_name }}({% call swift::arg_list_decl(func) %}) {% call swift::throws(func) %} {
    {% call swift::to_ffi_call(func) %}
}

{% endmatch %}
{%- endif %}
