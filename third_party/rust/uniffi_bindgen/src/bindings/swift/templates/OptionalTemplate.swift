{%- let inner_type = self.inner() %}
{%- let outer_type = self.outer() %}
fileprivate struct {{ outer_type|ffi_converter_name }}: FfiConverterRustBuffer {
    typealias SwiftType = {{ outer_type|type_name }}

    static func write(_ value: SwiftType, into buf: Writer) {
        guard let value = value else {
            buf.writeInt(Int8(0))
            return
        }
        buf.writeInt(Int8(1))
        {{ inner_type|write_fn }}(value, into: buf)
    }

    static func read(from buf: Reader) throws -> SwiftType {
        switch try buf.readInt() as Int8 {
        case 0: return nil
        case 1: return try {{ inner_type|read_fn }}(from: buf)
        default: throw UniffiInternalError.unexpectedOptionalTag
        }
    }
}
