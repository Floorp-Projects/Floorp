fileprivate struct FfiConverterBool : FfiConverter {
    typealias FfiType = Int8
    typealias SwiftType = Bool

    public static func lift(_ value: Int8) throws -> Bool {
        return value != 0
    }

    public static func lower(_ value: Bool) -> Int8 {
        return value ? 1 : 0
    }

    public static func read(from buf: inout (data: Data, offset: Data.Index)) throws -> Bool {
        return try lift(readInt(&buf))
    }

    public static func write(_ value: Bool, into buf: inout [UInt8]) {
        writeInt(&buf, lower(value))
    }
}
