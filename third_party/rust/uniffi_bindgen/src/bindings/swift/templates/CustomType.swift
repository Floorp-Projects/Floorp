{%- let ffi_type_name=builtin|ffi_type|ffi_type_name %}
{%- match config.custom_types.get(name.as_str())  %}
{%- when None %}
{#- No config, just forward all methods to our builtin type #}
/**
 * Typealias from the type name used in the UDL file to the builtin type.  This
 * is needed because the UDL type name is used in function/method signatures.
 */
public typealias {{ name }} = {{ builtin|type_name }}
public struct FfiConverterType{{ name }}: FfiConverter {
    public static func read(from buf: inout (data: Data, offset: Data.Index)) throws -> {{ name }} {
        return try {{ builtin|read_fn }}(from: &buf)
    }

    public static func write(_ value: {{ name }}, into buf: inout [UInt8]) {
        return {{ builtin|write_fn }}(value, into: &buf)
    }

    public static func lift(_ value: {{ ffi_type_name }}) throws -> {{ name }} {
        return try {{ builtin|lift_fn }}(value)
    }

    public static func lower(_ value: {{ name }}) -> {{ ffi_type_name }} {
        return {{ builtin|lower_fn }}(value)
    }
}

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

public struct FfiConverterType{{ name }}: FfiConverter {
    {#- Custom type config supplied, use it to convert the builtin type #}

    public static func read(from buf: inout (data: Data, offset: Data.Index)) throws -> {{ name }} {
        let builtinValue = try {{ builtin|read_fn }}(from: &buf)
        return {{ config.into_custom.render("builtinValue") }}
    }

    public static func write(_ value: {{ name }}, into buf: inout [UInt8]) {
        let builtinValue = {{ config.from_custom.render("value") }}
        return {{ builtin|write_fn }}(builtinValue, into: &buf)
    }

    public static func lift(_ value: {{ ffi_type_name }}) throws -> {{ name }} {
        let builtinValue = try {{ builtin|lift_fn }}(value)
        return {{ config.into_custom.render("builtinValue") }}
    }

    public static func lower(_ value: {{ name }}) -> {{ ffi_type_name }} {
        let builtinValue = {{ config.from_custom.render("value") }}
        return {{ builtin|lower_fn }}(builtinValue)
    }
}

{%- endmatch %}

{#
We always write these public functions just incase the type is used as
an external type by another crate.
#}
public func FfiConverterType{{ name }}_lift(_ value: {{ ffi_type_name }}) throws -> {{ name }} {
    return try FfiConverterType{{ name }}.lift(value)
}

public func FfiConverterType{{ name }}_lower(_ value: {{ name }}) -> {{ ffi_type_name }} {
    return FfiConverterType{{ name }}.lower(value)
}

