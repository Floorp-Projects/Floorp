fileprivate struct FfiConverterDouble: FfiConverterPrimitive {
    typealias FfiType = Double
    typealias SwiftType = Double

    static func read(from buf: Reader) throws -> Double {
        return try lift(buf.readDouble())
    }

    static func write(_ value: Double, into buf: Writer) {
        buf.writeDouble(lower(value))
    }
}
