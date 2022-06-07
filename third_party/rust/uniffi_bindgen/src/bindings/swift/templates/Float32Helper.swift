fileprivate struct FfiConverterFloat: FfiConverterPrimitive {
    typealias FfiType = Float
    typealias SwiftType = Float

    static func read(from buf: Reader) throws -> Float {
        return try lift(buf.readFloat())
    }

    static func write(_ value: Float, into buf: Writer) {
        buf.writeFloat(lower(value))
    }
}
