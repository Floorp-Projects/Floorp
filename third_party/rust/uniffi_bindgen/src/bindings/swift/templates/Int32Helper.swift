fileprivate struct FfiConverterInt32: FfiConverterPrimitive {
    typealias FfiType = Int32
    typealias SwiftType = Int32

    static func read(from buf: Reader) throws -> Int32 {
        return try lift(buf.readInt())
    }

    static func write(_ value: Int32, into buf: Writer) {
        buf.writeInt(lower(value))
    }
}
