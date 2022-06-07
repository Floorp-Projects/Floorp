fileprivate struct FfiConverterInt64: FfiConverterPrimitive {
    typealias FfiType = Int64
    typealias SwiftType = Int64

    static func read(from buf: Reader) throws -> Int64 {
        return try lift(buf.readInt())
    }

    static func write(_ value: Int64, into buf: Writer) {
        buf.writeInt(lower(value))
    }
}
