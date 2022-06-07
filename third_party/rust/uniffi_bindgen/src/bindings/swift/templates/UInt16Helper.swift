fileprivate struct FfiConverterUInt16: FfiConverterPrimitive {
    typealias FfiType = UInt16
    typealias SwiftType = UInt16

    static func read(from buf: Reader) throws -> UInt16 {
        return try lift(buf.readInt())
    }

    static func write(_ value: SwiftType, into buf: Writer) {
        buf.writeInt(lower(value))
    }
}
