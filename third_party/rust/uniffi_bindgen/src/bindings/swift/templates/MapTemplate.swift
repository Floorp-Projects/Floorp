{%- import "macros.swift" as swift -%}
{%- let outer_type = self.outer() %}
{%- let dict_type = outer_type|type_name %}
{%- let key_type = self.key() %}
{%- let value_type = self.value() %}

fileprivate struct {{ outer_type|ffi_converter_name }}: FfiConverterRustBuffer {
    fileprivate static func write(_ value: {{ dict_type }}, into buf: Writer) {
        let len = Int32(value.count)
        buf.writeInt(len)
        for (key, value) in value {
            {{ key_type|write_fn }}(key, into: buf)
            {{ value_type|write_fn }}(value, into: buf)
        }
    }

    fileprivate static func read(from buf: Reader) throws -> {{ dict_type }} {
        let len: Int32 = try buf.readInt()
        var dict = {{ dict_type }}()
        dict.reserveCapacity(Int(len))
        for _ in 0..<len {
            let key = try {{ key_type|read_fn }}(from: buf)
            let value = try {{ value_type|read_fn }}(from: buf)
            dict[key] = value
        }
        return dict
    }
}
