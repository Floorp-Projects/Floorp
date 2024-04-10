{%- let rec = ci|get_record_definition(name) %}

data class {{ type_name }} (
    {%- for field in rec.fields() %}
    var {{ field.name()|var_name }}: {{ field|type_name(ci) -}}
    {%- match field.default_value() %}
        {%- when Some with(literal) %} = {{ literal|render_literal(field, ci) }}
        {%- else %}
    {%- endmatch -%}
    {% if !loop.last %}, {% endif %}
    {%- endfor %}
) {% if contains_object_references %}: Disposable {% endif %}{
    {% if contains_object_references %}
    @Suppress("UNNECESSARY_SAFE_CALL") // codegen is much simpler if we unconditionally emit safe calls here
    override fun destroy() {
        {% call kt::destroy_fields(rec) %}
    }
    {% endif %}
    companion object
}

public object {{ rec|ffi_converter_name }}: FfiConverterRustBuffer<{{ type_name }}> {
    override fun read(buf: ByteBuffer): {{ type_name }} {
        return {{ type_name }}(
        {%- for field in rec.fields() %}
            {{ field|read_fn }}(buf),
        {%- endfor %}
        )
    }

    override fun allocationSize(value: {{ type_name }}) = (
        {%- for field in rec.fields() %}
            {{ field|allocation_size_fn }}(value.{{ field.name()|var_name }}){% if !loop.last %} +{% endif%}
        {%- endfor %}
    )

    override fun write(value: {{ type_name }}, buf: ByteBuffer) {
        {%- for field in rec.fields() %}
            {{ field|write_fn }}(value.{{ field.name()|var_name }}, buf)
        {%- endfor %}
    }
}
