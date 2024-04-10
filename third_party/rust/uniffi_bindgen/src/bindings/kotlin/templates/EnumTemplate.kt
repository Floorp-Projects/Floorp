{#
// Kotlin's `enum class` construct doesn't support variants with associated data,
// but is a little nicer for consumers than its `sealed class` enum pattern.
// So, we switch here, using `enum class` for enums with no associated data
// and `sealed class` for the general case.
#}

{%- if e.is_flat() %}

{%- call kt::docstring(e, 0) %}
{% match e.variant_discr_type() %}
{% when None %}
enum class {{ type_name }} {
    {% for variant in e.variants() -%}
    {%- call kt::docstring(variant, 4) %}
    {{ variant|variant_name }}{% if loop.last %};{% else %},{% endif %}
    {%- endfor %}
    companion object
}
{% when Some with (variant_discr_type) %}
enum class {{ type_name }}(val value: {{ variant_discr_type|type_name(ci) }}) {
    {% for variant in e.variants() -%}
    {%- call kt::docstring(variant, 4) %}
    {{ variant|variant_name }}({{ e|variant_discr_literal(loop.index0) }}){% if loop.last %};{% else %},{% endif %}
    {%- endfor %}
    companion object
}
{% endmatch %}

public object {{ e|ffi_converter_name }}: FfiConverterRustBuffer<{{ type_name }}> {
    override fun read(buf: ByteBuffer) = try {
        {{ type_name }}.values()[buf.getInt() - 1]
    } catch (e: IndexOutOfBoundsException) {
        throw RuntimeException("invalid enum value, something is very wrong!!", e)
    }

    override fun allocationSize(value: {{ type_name }}) = 4UL

    override fun write(value: {{ type_name }}, buf: ByteBuffer) {
        buf.putInt(value.ordinal + 1)
    }
}

{% else %}

{%- call kt::docstring(e, 0) %}
sealed class {{ type_name }}{% if contains_object_references %}: Disposable {% endif %} {
    {% for variant in e.variants() -%}
    {%- call kt::docstring(variant, 4) %}
    {% if !variant.has_fields() -%}
    object {{ variant|type_name(ci) }} : {{ type_name }}()
    {% else -%}
    data class {{ variant|type_name(ci) }}(
        {%- for field in variant.fields() -%}
        {%- call kt::docstring(field, 8) %}
        val {% call kt::field_name(field, loop.index) %}: {{ field|type_name(ci) }}{% if loop.last %}{% else %}, {% endif %}
        {%- endfor -%}
    ) : {{ type_name }}() {
        companion object
    }
    {%- endif %}
    {% endfor %}

    {% if contains_object_references %}
    @Suppress("UNNECESSARY_SAFE_CALL") // codegen is much simpler if we unconditionally emit safe calls here
    override fun destroy() {
        when(this) {
            {%- for variant in e.variants() %}
            is {{ type_name }}.{{ variant|type_name(ci) }} -> {
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
    companion object
}

public object {{ e|ffi_converter_name }} : FfiConverterRustBuffer<{{ type_name }}>{
    override fun read(buf: ByteBuffer): {{ type_name }} {
        return when(buf.getInt()) {
            {%- for variant in e.variants() %}
            {{ loop.index }} -> {{ type_name }}.{{ variant|type_name(ci) }}{% if variant.has_fields() %}(
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
        is {{ type_name }}.{{ variant|type_name(ci) }} -> {
            // Add the size for the Int that specifies the variant plus the size needed for all fields
            (
                4UL
                {%- for field in variant.fields() %}
                + {{ field|allocation_size_fn }}(value.{%- call kt::field_name(field, loop.index) -%})
                {%- endfor %}
            )
        }
        {%- endfor %}
    }

    override fun write(value: {{ type_name }}, buf: ByteBuffer) {
        when(value) {
            {%- for variant in e.variants() %}
            is {{ type_name }}.{{ variant|type_name(ci) }} -> {
                buf.putInt({{ loop.index }})
                {%- for field in variant.fields() %}
                {{ field|write_fn }}(value.{%- call kt::field_name(field, loop.index) -%}, buf)
                {%- endfor %}
                Unit
            }
            {%- endfor %}
        }.let { /* this makes the `when` an expression, which ensures it is exhaustive */ }
    }
}

{% endif %}
