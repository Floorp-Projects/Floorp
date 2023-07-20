{%- if func.is_async() %}

public func {{ func.name()|fn_name }}({%- call swift::arg_list_decl(func) -%}) async {% call swift::throws(func) %}{% match func.return_type() %}{% when Some with (return_type) %} -> {{ return_type|type_name }}{% when None %}{% endmatch %} {
    var continuation: {{ func.result_type().borrow()|future_continuation_type }}? = nil
    // Suspend the function and call the scaffolding function, passing it a callback handler from
    // `AsyncTypes.swift`
    //
    // Make sure to hold on to a reference to the continuation in the top-level scope so that
    // it's not freed before the callback is invoked.
    return {% call swift::try(func) %} await withCheckedThrowingContinuation {
        continuation = $0
        try! rustCall() {
            {{ func.ffi_func().name() }}(
                {% call swift::arg_list_lowered(func) %}
                FfiConverterForeignExecutor.lower(UniFfiForeignExecutor()),
                {{ func.result_type().borrow()|future_callback }},
                &continuation,
                $0
            )
        }
    }
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
