{%- match config %}
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

    static func lift(_ value: {{ self.builtin_ffi_type().borrow()|type_ffi_lowered }}) throws -> {{ name }} {
        let builtinValue = try {{ builtin|lift_fn }}(value)
        return {{ config.into_custom.render("builtinValue") }}
    }

    static func lower(_ value: {{ name }}) -> {{ self.builtin_ffi_type().borrow()|type_ffi_lowered }} {
        let builtinValue = {{ config.from_custom.render("value") }}
        return {{ builtin|lower_fn }}(builtinValue)
    }

}
{%- endmatch %}

