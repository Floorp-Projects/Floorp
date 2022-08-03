{% let record = ci.get_record_definition(name).unwrap() %}

class {{ record.nm() }} {
    constructor({{ record.constructor_field_list() }}) {
        {%- for field in record.fields() %}
        {{ field.check_type() }};
        {%- endfor %}

        {%- for field in record.fields() %}
        this.{{field.nm()}} = {{ field.nm() }};
        {%- endfor %}
    }
    equals(other) {
        return (
            {%- for field in record.fields() %}
            {{ field.type_().equals("this.{}"|format(field.nm()), "other.{}"|format(field.nm())) }}{% if !loop.last %} &&{% endif %}
            {%- endfor %}
        )
    }
}

class {{ ffi_converter }} extends FfiConverter {
    static lift(buf) {
        return this.read(new ArrayBufferDataStream(buf));
    }
    static lower(value) {
        const buf = new ArrayBuffer(this.computeSize(value));
        const dataStream = new ArrayBufferDataStream(buf);
        this.write(dataStream, value);
        return buf;
    }
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
}

EXPORTED_SYMBOLS.push("{{ record.nm() }}");
