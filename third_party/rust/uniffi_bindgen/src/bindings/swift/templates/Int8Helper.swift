fileprivate struct FfiConverterInt8: FfiConverterPrimitive {
    typealias FfiType = Int8
    typealias SwiftType = Int8

    static func read(from buf: Reader) throws -> Int8 {
        return try lift(buf.readInt())
    }

    static func write(_ value: Int8, into buf: Writer) {
        buf.writeInt(lower(value))
    }
}
