fileprivate struct FfiConverterUInt64: FfiConverterPrimitive {
    typealias FfiType = UInt64
    typealias SwiftType = UInt64

    static func read(from buf: Reader) throws -> UInt64 {
        return try lift(buf.readInt())
    }

    static func write(_ value: SwiftType, into buf: Writer) {
        buf.writeInt(lower(value))
    }
}
