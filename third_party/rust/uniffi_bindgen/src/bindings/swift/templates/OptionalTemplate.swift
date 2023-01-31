fileprivate struct {{ ffi_converter_name }}: FfiConverterRustBuffer {
    typealias SwiftType = {{ type_name }}

    public static func write(_ value: SwiftType, into buf: inout [UInt8]) {
        guard let value = value else {
            writeInt(&buf, Int8(0))
            return
        }
        writeInt(&buf, Int8(1))
        {{ inner_type|write_fn }}(value, into: &buf)
    }

    public static func read(from buf: inout (data: Data, offset: Data.Index)) throws -> SwiftType {
        switch try readInt(&buf) as Int8 {
        case 0: return nil
        case 1: return try {{ inner_type|read_fn }}(from: &buf)
        default: throw UniffiInternalError.unexpectedOptionalTag
        }
    }
}
