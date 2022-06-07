fileprivate struct FfiConverterInt16: FfiConverterPrimitive {
    typealias FfiType = Int16
    typealias SwiftType = Int16

    static func read(from buf: Reader) throws -> Int16 {
        return try lift(buf.readInt())
    }

    static func write(_ value: Int16, into buf: Writer) {
        buf.writeInt(lower(value))
    }
}
