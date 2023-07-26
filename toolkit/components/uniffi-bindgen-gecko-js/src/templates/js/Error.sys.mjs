{%- let string_type = Type::String %}
{%- let string_ffi_converter = string_type.ffi_converter() %}

export class {{ error.nm() }} extends Error {}
{% for variant in error.variants() %}

export class {{ variant.name().to_upper_camel_case() }} extends {{ error.nm() }} {
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
    toString() {
        return `{{ variant.name().to_upper_camel_case() }}: ${super.toString()}`
    }
}
{%- endfor %}

// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverterArrayBuffer {
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
                throw new Error("Unknown {{ error.nm() }} variant");
        }
    }
    static computeSize(value) {
        // Size of the Int indicating the variant
        let totalSize = 4;
        {%- for variant in error.variants() %}
        if (value instanceof {{ variant.name().to_upper_camel_case() }}) {
            {%- for field in variant.fields() %}
            totalSize += {{ field.ffi_converter() }}.computeSize(value.{{ field.nm() }});
            {%- endfor %}
            return totalSize;
        }
        {%- endfor %}
        throw new Error("Unknown {{ error.nm() }} variant");
    }
    static write(dataStream, value) {
        {%- for variant in error.variants() %}
        if (value instanceof {{ variant.name().to_upper_camel_case() }}) {
            dataStream.writeInt32({{ loop.index }});
            {%- for field in variant.fields() %}
            {{ field.ffi_converter() }}.write(dataStream, value.{{ field.nm() }});
            {%- endfor %}
            return;
        }
        {%- endfor %}
        throw new Error("Unknown {{ error.nm() }} variant");
    }

    static errorClass = {{ error.nm() }};
}
