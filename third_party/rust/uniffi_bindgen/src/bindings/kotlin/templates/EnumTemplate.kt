{#
// Kotlin's `enum class` construct doesn't support variants with associated data,
// but is a little nicer for consumers than its `sealed class` enum pattern.
// So, we switch here, using `enum class` for enums with no associated data
// and `sealed class` for the general case.
#}
{%- let e = ci.get_enum_definition(name).unwrap() %}

{%- if e.is_flat() %}

enum class {{ type_name }} {
    {% for variant in e.variants() -%}
    {{ variant.name()|enum_variant }}{% if loop.last %};{% else %},{% endif %}
    {%- endfor %}
}

public object {{ e|ffi_converter_name }}: FfiConverterRustBuffer<{{ type_name }}> {
    override fun read(buf: ByteBuffer) = try {
        {{ type_name }}.values()[buf.getInt() - 1]
    } catch (e: IndexOutOfBoundsException) {
        throw RuntimeException("invalid enum value, something is very wrong!!", e)
    }

    override fun allocationSize(value: {{ type_name }}) = 4

    override fun write(value: {{ type_name }}, buf: ByteBuffer) {
        buf.putInt(value.ordinal + 1)
    }
}

{% else %}

sealed class {{ type_name }}{% if contains_object_references %}: Disposable {% endif %} {
    {% for variant in e.variants() -%}
    {% if !variant.has_fields() -%}
    object {{ variant.name()|class_name }} : {{ type_name }}()
    {% else -%}
    data class {{ variant.name()|class_name }}(
        {% for field in variant.fields() -%}
        val {{ field.name()|var_name }}: {{ field|type_name}}{% if loop.last %}{% else %}, {% endif %}
        {% endfor -%}
    ) : {{ type_name }}()
    {%- endif %}
    {% endfor %}

    {% if contains_object_references %}
    @Suppress("UNNECESSARY_SAFE_CALL") // codegen is much simpler if we unconditionally emit safe calls here
    override fun destroy() {
        when(this) {
            {%- for variant in e.variants() %}
            is {{ type_name }}.{{ variant.name()|class_name }} -> {
                {%- if variant.has_fields() %}
                {% call kt::destroy_fields(variant) %}
                {% else -%}
                // Nothing to destroy
                {%- endif %}
            }
            {%- endfor %}
        }.let { /* this makes the `when` an expression, which ensures it is exhaustive */ }
    }
    {% endif %}
}

public object {{ e|ffi_converter_name }} : FfiConverterRustBuffer<{{ type_name }}>{
    override fun read(buf: ByteBuffer): {{ type_name }} {
        return when(buf.getInt()) {
            {%- for variant in e.variants() %}
            {{ loop.index }} -> {{ type_name }}.{{ variant.name()|class_name }}{% if variant.has_fields() %}(
                {% for field in variant.fields() -%}
                {{ field|read_fn }}(buf),
                {% endfor -%}
            ){%- endif -%}
            {%- endfor %}
            else -> throw RuntimeException("invalid enum value, something is very wrong!!")
        }
    }

    override fun allocationSize(value: {{ type_name }}) = when(value) {
        {%- for variant in e.variants() %}
        is {{ type_name }}.{{ variant.name()|class_name }} -> {
            // Add the size for the Int that specifies the variant plus the size needed for all fields
            (
                4
                {%- for field in variant.fields() %}
                + {{ field|allocation_size_fn }}(value.{{ field.name()|var_name }})
                {%- endfor %}
            )
        }
        {%- endfor %}
    }

    override fun write(value: {{ type_name }}, buf: ByteBuffer) {
        when(value) {
            {%- for variant in e.variants() %}
            is {{ type_name }}.{{ variant.name()|class_name }} -> {
                buf.putInt({{ loop.index }})
                {%- for field in variant.fields() %}
                {{ field|write_fn }}(value.{{ field.name()|var_name }}, buf)
                {%- endfor %}
                Unit
            }
            {%- endfor %}
        }.let { /* this makes the `when` an expression, which ensures it is exhaustive */ }
    }
}

{% endif %}
