fileprivate struct FfiConverterUInt8: FfiConverterPrimitive {
    typealias FfiType = UInt8
    typealias SwiftType = UInt8

    static func read(from buf: Reader) throws -> UInt8 {
        return try lift(buf.readInt())
    }

    static func write(_ value: UInt8, into buf: Writer) {
        buf.writeInt(lower(value))
    }
}
