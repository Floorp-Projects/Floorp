fileprivate struct FfiConverterFloat: FfiConverterPrimitive {
    typealias FfiType = Float
    typealias SwiftType = Float

    public static func read(from buf: inout (data: Data, offset: Data.Index)) throws -> Float {
        return try lift(readFloat(&buf))
    }

    public static func write(_ value: Float, into buf: inout [UInt8]) {
        writeFloat(&buf, lower(value))
    }
}
