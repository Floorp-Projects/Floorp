fileprivate struct FfiConverterInt32: FfiConverterPrimitive {
    typealias FfiType = Int32
    typealias SwiftType = Int32

    public static func read(from buf: inout (data: Data, offset: Data.Index)) throws -> Int32 {
        return try lift(readInt(&buf))
    }

    public static func write(_ value: Int32, into buf: inout [UInt8]) {
        writeInt(&buf, lower(value))
    }
}
