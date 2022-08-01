{%- let rec = ci.get_record_definition(name).unwrap() %}
public struct {{ type_name }} {
    {%- for field in rec.fields() %}
    public var {{ field.name()|var_name }}: {{ field|type_name }}
    {%- endfor %}

    // Default memberwise initializers are never public by default, so we
    // declare one manually.
    public init({% call swift::field_list_decl(rec) %}) {
        {%- for field in rec.fields() %}
        self.{{ field.name()|var_name }} = {{ field.name()|var_name }}
        {%- endfor %}
    }
}

{% if !contains_object_references %}
extension {{ type_name }}: Equatable, Hashable {
    public static func ==(lhs: {{ type_name }}, rhs: {{ type_name }}) -> Bool {
        {%- for field in rec.fields() %}
        if lhs.{{ field.name()|var_name }} != rhs.{{ field.name()|var_name }} {
            return false
        }
        {%- endfor %}
        return true
    }

    public func hash(into hasher: inout Hasher) {
        {%- for field in rec.fields() %}
        hasher.combine({{ field.name()|var_name }})
        {%- endfor %}
    }
}
{% endif %}

fileprivate struct {{ ffi_converter_name }}: FfiConverterRustBuffer {
    fileprivate static func read(from buf: Reader) throws -> {{ type_name }} {
        return try {{ type_name }}(
            {%- for field in rec.fields() %}
            {{ field.name()|var_name }}: {{ field|read_fn }}(from: buf)
            {%- if !loop.last %}, {% endif %}
            {%- endfor %}
        )
    }

    fileprivate static func write(_ value: {{ type_name }}, into buf: Writer) {
        {%- for field in rec.fields() %}
        {{ field|write_fn }}(value.{{ field.name()|var_name }}, into: buf)
        {%- endfor %}
    }
}
