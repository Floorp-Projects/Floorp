// Note that we don't yet support `indirect` for enums.
// See https://github.com/mozilla/uniffi-rs/issues/396 for further discussion.
{%- let e = ci.get_enum_definition(name).unwrap() %}
public enum {{ type_name }} {
    {% for variant in e.variants() %}
    case {{ variant.name()|enum_variant_swift }}{% if variant.fields().len() > 0 %}({% call swift::field_list_decl(variant) %}){% endif -%}
    {% endfor %}
}

fileprivate struct {{ ffi_converter_name }}: FfiConverterRustBuffer {
    typealias SwiftType = {{ type_name }}

    static func read(from buf: Reader) throws -> {{ type_name }} {
        let variant: Int32 = try buf.readInt()
        switch variant {
        {% for variant in e.variants() %}
        case {{ loop.index }}: return .{{ variant.name()|enum_variant_swift }}{% if variant.has_fields() %}(
            {%- for field in variant.fields() %}
            {{ field.name()|var_name }}: try {{ field|read_fn }}(from: buf)
            {%- if !loop.last %}, {% endif %}
            {%- endfor %}
        ){%- endif %}
        {% endfor %}
        default: throw UniffiInternalError.unexpectedEnumCase
        }
    }

    static func write(_ value: {{ type_name }}, into buf: Writer) {
        switch value {
        {% for variant in e.variants() %}
        {% if variant.has_fields() %}
        case let .{{ variant.name()|enum_variant_swift }}({% for field in variant.fields() %}{{ field.name()|var_name }}{%- if loop.last -%}{%- else -%},{%- endif -%}{% endfor %}):
            buf.writeInt(Int32({{ loop.index }}))
            {% for field in variant.fields() -%}
            {{ field|write_fn }}({{ field.name()|var_name }}, into: buf)
            {% endfor -%}
        {% else %}
        case .{{ variant.name()|enum_variant_swift }}:
            buf.writeInt(Int32({{ loop.index }}))
        {% endif %}
        {%- endfor %}
        }
    }
}

{% if !contains_object_references %}
extension {{ type_name }}: Equatable, Hashable {}
{% endif %}
