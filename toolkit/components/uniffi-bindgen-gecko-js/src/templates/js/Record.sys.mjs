{%- let record = ci.get_record_definition(name).unwrap() -%}
export class {{ record.nm() }} {
    constructor({{ record.constructor_field_list() }}) {
        {%- for field in record.fields() %}
        try {
            {{ field.ffi_converter() }}.checkType({{ field.nm() }})
        } catch (e) {
            if (e instanceof UniFFITypeError) {
                e.addItemDescriptionPart("{{ field.nm() }}");
            }
            throw e;
        }
        {%- endfor %}

        {%- for field in record.fields() %}
        this.{{field.nm()}} = {{ field.nm() }};
        {%- endfor %}
    }
    equals(other) {
        return (
            {%- for field in record.fields() %}
            {{ field.as_type().equals("this.{}"|format(field.nm()), "other.{}"|format(field.nm())) }}{% if !loop.last %} &&{% endif %}
            {%- endfor %}
        )
    }
}

// Export the FFIConverter object to make external types work.
export class {{ ffi_converter }} extends FfiConverterArrayBuffer {
    static read(dataStream) {
        return new {{record.nm()}}(
            {%- for field in record.fields() %}
            {{ field.read_datastream_fn() }}(dataStream)
            {%- if !loop.last %}, {% endif %}
            {%- endfor %}
        );
    }
    static write(dataStream, value) {
        {%- for field in record.fields() %}
        {{ field.write_datastream_fn() }}(dataStream, value.{{field.nm()}});
        {%- endfor %}
    }

    static computeSize(value) {
        let totalSize = 0;
        {%- for field in record.fields() %}
        totalSize += {{ field.ffi_converter() }}.computeSize(value.{{ field.nm() }});
        {%- endfor %}
        return totalSize
    }

    static checkType(value) {
        super.checkType(value);
        {%- for field in record.fields() %}
        try {
            {{ field.ffi_converter() }}.checkType(value.{{ field.nm() }});
        } catch (e) {
            if (e instanceof UniFFITypeError) {
                e.addItemDescriptionPart(".{{ field.nm() }}");
            }
            throw e;
        }
        {%- endfor %}
    }
}
