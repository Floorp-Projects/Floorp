fileprivate struct FfiConverterUInt32: FfiConverterPrimitive {
    typealias FfiType = UInt32
    typealias SwiftType = UInt32

    static func read(from buf: Reader) throws -> UInt32 {
        return try lift(buf.readInt())
    }

    static func write(_ value: SwiftType, into buf: Writer) {
        buf.writeInt(lower(value))
    }
}
