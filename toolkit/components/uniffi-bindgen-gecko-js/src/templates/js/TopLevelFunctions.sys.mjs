{%- for func in ci.function_definitions() %}

export function {{ func.nm() }}({{ func.arg_names() }}) {
{% call js::call_scaffolding_function(func) %}
}
{%- endfor %}
