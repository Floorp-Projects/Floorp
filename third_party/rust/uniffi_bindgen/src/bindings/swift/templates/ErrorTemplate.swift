{%- let e = ci.get_error_definition(name).unwrap() %}
public enum {{ type_name }} {

    {% if e.is_flat() %}
    {% for variant in e.variants() %}
    // Simple error enums only carry a message
    case {{ variant.name()|class_name }}(message: String)
    {% endfor %}

    {%- else %}
    {% for variant in e.variants() %}
    case {{ variant.name()|class_name }}{% if variant.fields().len() > 0 %}({% call swift::field_list_decl(variant) %}){% endif -%}
    {% endfor %}

    {%- endif %}
}

public struct {{ ffi_converter_name }}: FfiConverterRustBuffer {
    typealias SwiftType = {{ type_name }}

    public static func read(from buf: inout (data: Data, offset: Data.Index)) throws -> {{ type_name }} {
        let variant: Int32 = try readInt(&buf)
        switch variant {

        {% if e.is_flat() %}

        {% for variant in e.variants() %}
        case {{ loop.index }}: return .{{ variant.name()|class_name }}(
            message: try {{ Type::String.borrow()|read_fn }}(from: &buf)
        )
        {% endfor %}

        {% else %}

        {% for variant in e.variants() %}
        case {{ loop.index }}: return .{{ variant.name()|class_name }}{% if variant.has_fields() -%}(
            {% for field in variant.fields() -%}
            {{ field.name()|var_name }}: try {{ field|read_fn }}(from: &buf)
            {%- if !loop.last %}, {% endif %}
            {% endfor -%}
        ){% endif -%}
        {% endfor %}

         {% endif -%}
        default: throw UniffiInternalError.unexpectedEnumCase
        }
    }

    public static func write(_ value: {{ type_name }}, into buf: inout [UInt8]) {
        switch value {

        {% if e.is_flat() %}

        {% for variant in e.variants() %}
        case let .{{ variant.name()|class_name }}(message):
            writeInt(&buf, Int32({{ loop.index }}))
            {{ Type::String.borrow()|write_fn }}(message, into: &buf)
        {%- endfor %}

        {% else %}

        {% for variant in e.variants() %}
        {% if variant.has_fields() %}
        case let .{{ variant.name()|class_name }}({% for field in variant.fields() %}{{ field.name()|var_name }}{%- if loop.last -%}{%- else -%},{%- endif -%}{% endfor %}):
            writeInt(&buf, Int32({{ loop.index }}))
            {% for field in variant.fields() -%}
            {{ field|write_fn }}({{ field.name()|var_name }}, into: &buf)
            {% endfor -%}
        {% else %}
        case .{{ variant.name()|class_name }}:
            writeInt(&buf, Int32({{ loop.index }}))
        {% endif %}
        {%- endfor %}

        {%- endif %}
        }
    }
}

{% if !contains_object_references %}
extension {{ type_name }}: Equatable, Hashable {}
{% endif %}
extension {{ type_name }}: Error { }
