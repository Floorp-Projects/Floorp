fileprivate struct FfiConverterBool : FfiConverter {
    typealias FfiType = Int8
    typealias SwiftType = Bool

    static func lift(_ value: Int8) throws -> Bool {
        return value != 0
    }

    static func lower(_ value: Bool) -> Int8 {
        return value ? 1 : 0
    }

    static func read(from buf: Reader) throws -> Bool {
        return try lift(buf.readInt())
    }

    static func write(_ value: Bool, into buf: Writer) {
        buf.writeInt(lower(value))
    }
}
