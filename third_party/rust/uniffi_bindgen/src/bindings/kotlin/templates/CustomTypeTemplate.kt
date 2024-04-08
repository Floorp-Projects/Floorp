{%- match config.custom_types.get(name.as_str())  %}
{%- when None %}
{#- Define the type using typealiases to the builtin #}
/**
 * Typealias from the type name used in the UDL file to the builtin type.  This
 * is needed because the UDL type name is used in function/method signatures.
 * It's also what we have an external type that references a custom type.
 */
public typealias {{ name }} = {{ builtin|type_name(ci) }}
public typealias {{ ffi_converter_name }} = {{ builtin|ffi_converter_name }}

{%- when Some with (config) %}

{%- let ffi_type_name=builtin|ffi_type|ffi_type_name_by_value %}

{# When the config specifies a different type name, create a typealias for it #}
{%- match config.type_name %}
{%- when Some(concrete_type_name) %}
/**
 * Typealias from the type name used in the UDL file to the custom type.  This
 * is needed because the UDL type name is used in function/method signatures.
 * It's also what we have an external type that references a custom type.
 */
public typealias {{ name }} = {{ concrete_type_name }}
{%- else %}
{%- endmatch %}

{%- match config.imports %}
{%- when Some(imports) %}
{%- for import_name in imports %}
{{ self.add_import(import_name) }}
{%- endfor %}
{%- else %}
{%- endmatch %}

public object {{ ffi_converter_name }}: FfiConverter<{{ name }}, {{ ffi_type_name }}> {
    override fun lift(value: {{ ffi_type_name }}): {{ name }} {
        val builtinValue = {{ builtin|lift_fn }}(value)
        return {{ config.into_custom.render("builtinValue") }}
    }

    override fun lower(value: {{ name }}): {{ ffi_type_name }} {
        val builtinValue = {{ config.from_custom.render("value") }}
        return {{ builtin|lower_fn }}(builtinValue)
    }

    override fun read(buf: ByteBuffer): {{ name }} {
        val builtinValue = {{ builtin|read_fn }}(buf)
        return {{ config.into_custom.render("builtinValue") }}
    }

    override fun allocationSize(value: {{ name }}): Int {
        val builtinValue = {{ config.from_custom.render("value") }}
        return {{ builtin|allocation_size_fn }}(builtinValue)
    }

    override fun write(value: {{ name }}, buf: ByteBuffer) {
        val builtinValue = {{ config.from_custom.render("value") }}
        {{ builtin|write_fn }}(builtinValue, buf)
    }
}
{%- endmatch %}
