fileprivate struct {{ ffi_converter_name }}: FfiConverterRustBuffer {
    public static func write(_ value: {{ type_name }}, into buf: inout [UInt8]) {
        let len = Int32(value.count)
        writeInt(&buf, len)
        for (key, value) in value {
            {{ key_type|write_fn }}(key, into: &buf)
            {{ value_type|write_fn }}(value, into: &buf)
        }
    }

    public static func read(from buf: inout (data: Data, offset: Data.Index)) throws -> {{ type_name }} {
        let len: Int32 = try readInt(&buf)
        var dict = {{ type_name }}()
        dict.reserveCapacity(Int(len))
        for _ in 0..<len {
            let key = try {{ key_type|read_fn }}(from: &buf)
            let value = try {{ value_type|read_fn }}(from: &buf)
            dict[key] = value
        }
        return dict
    }
}
