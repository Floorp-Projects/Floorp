{% import "macros.swift" as swift %}
{%- let e = self.inner() %}
public enum {{ e|type_name }} {

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

fileprivate struct {{ e|ffi_converter_name }}: FfiConverterRustBuffer {
    typealias SwiftType = {{ e|type_name }}

    static func read(from buf: Reader) throws -> {{ e|type_name }} {
        let variant: Int32 = try buf.readInt()
        switch variant {

        {% if e.is_flat() %}

        {% for variant in e.variants() %}
        case {{ loop.index }}: return .{{ variant.name()|class_name }}(
            message: try {{ Type::String.borrow()|read_fn }}(from: buf)
        )
        {% endfor %}

        {% else %}

        {% for variant in e.variants() %}
        case {{ loop.index }}: return .{{ variant.name()|class_name }}{% if variant.has_fields() -%}(
            {% for field in variant.fields() -%}
            {{ field.name()|var_name }}: try {{ field|read_fn }}(from: buf)
            {%- if !loop.last %}, {% endif %}
            {% endfor -%}
        ){% endif -%}
        {% endfor %}

         {% endif -%}
        default: throw UniffiInternalError.unexpectedEnumCase
        }
    }

    static func write(_ value: {{ e|type_name }}, into buf: Writer) {
        switch value {

        {% if e.is_flat() %}

        {% for variant in e.variants() %}
        case let .{{ variant.name()|class_name }}(message):
            buf.writeInt(Int32({{ loop.index }}))
            {{ Type::String.borrow()|write_fn }}(message, into: buf)
        {%- endfor %}

        {% else %}

        {% for variant in e.variants() %}
        {% if variant.has_fields() %}
        case let .{{ variant.name()|class_name }}({% for field in variant.fields() %}{{ field.name()|var_name }}{%- if loop.last -%}{%- else -%},{%- endif -%}{% endfor %}):
            buf.writeInt(Int32({{ loop.index }}))
            {% for field in variant.fields() -%}
            {{ field|write_fn }}({{ field.name()|var_name }}, into: buf)
            {% endfor -%}
        {% else %}
        case .{{ variant.name()|class_name }}:
            buf.writeInt(Int32({{ loop.index }}))
        {% endif %}
        {%- endfor %}

        {%- endif %}
        }
    }
}

{% if !self.contains_object_references() %}
extension {{ e|type_name }}: Equatable, Hashable {}
{% endif %}
extension {{ e|type_name }}: Error { }
