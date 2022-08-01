{%- let ffi_type_name=builtin.ffi_type().borrow()|ffi_type_name %}
{%- match config.custom_types.get(name.as_str())  %}
{%- when None %}
{#- No config, just forward all methods to our builtin type #}
/**
 * Typealias from the type name used in the UDL file to the builtin type.  This
 * is needed because the UDL type name is used in function/method signatures.
 */
public typealias {{ name }} = {{ builtin|type_name }}
fileprivate typealias FfiConverterType{{ name }} = {{ builtin|ffi_converter_name }}

{%- when Some with (config) %}

{# When the config specifies a different type name, create a typealias for it #}
{%- match config.type_name %}
{%- when Some with (concrete_type_name) %}
/**
 * Typealias from the type name used in the UDL file to the custom type.  This
 * is needed because the UDL type name is used in function/method signatures.
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

fileprivate struct FfiConverterType{{ name }} {
    {#- Custom type config supplied, use it to convert the builtin type #}

    static func read(from buf: Reader) throws -> {{ name }} {
        let builtinValue = try {{ builtin|read_fn }}(from: buf)
        return {{ config.into_custom.render("builtinValue") }}
    }

    static func write(_ value: {{ name }}, into buf: Writer) {
        let builtinValue = {{ config.from_custom.render("value") }}
        return {{ builtin|write_fn }}(builtinValue, into: buf)
    }

    static func lift(_ value: {{ ffi_type_name }}) throws -> {{ name }} {
        let builtinValue = try {{ builtin|lift_fn }}(value)
        return {{ config.into_custom.render("builtinValue") }}
    }

    static func lower(_ value: {{ name }}) -> {{ ffi_type_name }} {
        let builtinValue = {{ config.from_custom.render("value") }}
        return {{ builtin|lower_fn }}(builtinValue)
    }

}
{%- endmatch %}

