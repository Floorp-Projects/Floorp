fileprivate struct FfiConverterInt8: FfiConverterPrimitive {
    typealias FfiType = Int8
    typealias SwiftType = Int8

    public static func read(from buf: inout (data: Data, offset: Data.Index)) throws -> Int8 {
        return try lift(readInt(&buf))
    }

    public static func write(_ value: Int8, into buf: inout [UInt8]) {
        writeInt(&buf, lower(value))
    }
}
