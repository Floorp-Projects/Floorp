{%- let error = ci.get_error_definition(name).unwrap() %}
{%- let string_type = Type::String %}
{%- let string_ffi_converter = string_type.ffi_converter() %}

class {{ error.nm() }} extends Error {}
EXPORTED_SYMBOLS.push("{{ error.nm() }}");

{% for variant in error.variants() %}
class {{ variant.name().to_upper_camel_case() }} extends {{ error.nm() }} {
    {% if error.is_flat() %}
    constructor(message, ...params) {
        super(...params);
        this.message = message;
    }
    {%- else %}
    constructor(
        {% for field in variant.fields() -%}
        {{field.nm()}},
        {% endfor -%}
        ...params
        ) {
            super(...params);
            {%- for field in variant.fields() %}
            this.{{field.nm()}} = {{ field.nm() }};
            {%- endfor %}
        }
    {%- endif %}
}
EXPORTED_SYMBOLS.push("{{ variant.name().to_upper_camel_case() }}");
{%-endfor %}

class {{ ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        switch (dataStream.readInt32()) {
            {%- for variant in error.variants() %}
            case {{ loop.index }}:
                {%- if error.is_flat() %}
                return new {{ variant.name().to_upper_camel_case()  }}({{ string_ffi_converter }}.read(dataStream));
                {%- else %}
                return new {{ variant.name().to_upper_camel_case()  }}(
                    {%- for field in variant.fields() %}
                    {{ field.ffi_converter() }}.read(dataStream){%- if loop.last %}{% else %}, {%- endif %}
                    {%- endfor %}
                    );
                {%- endif %}
            {%- endfor %}
            default:
                return new Error("Unknown {{ error.nm() }} variant");
        }
    }
}
