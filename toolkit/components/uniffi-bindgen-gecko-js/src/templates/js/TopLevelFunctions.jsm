{%- for func in ci.function_definitions() %}
function {{ func.nm() }}({{ func.arg_names() }}) {
    {% call js::call_scaffolding_function(func) %}
}

EXPORTED_SYMBOLS.push("{{ func.nm() }}");

{%- endfor %}
