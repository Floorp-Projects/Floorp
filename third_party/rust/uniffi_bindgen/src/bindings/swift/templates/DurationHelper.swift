fileprivate struct FfiConverterDuration: FfiConverterRustBuffer {
    typealias SwiftType = TimeInterval

    public static func read(from buf: inout (data: Data, offset: Data.Index)) throws -> TimeInterval {
        let seconds: UInt64 = try readInt(&buf)
        let nanoseconds: UInt32 = try readInt(&buf)
        return Double(seconds) + (Double(nanoseconds) / 1.0e9)
    }

    public static func write(_ value: TimeInterval, into buf: inout [UInt8]) {
        if value.rounded(.down) > Double(Int64.max) {
            fatalError("Duration overflow, exceeds max bounds supported by Uniffi")
        }

        if value < 0 {
            fatalError("Invalid duration, must be non-negative")
        }

        let seconds = UInt64(value)
        let nanoseconds = UInt32((value - Double(seconds)) * 1.0e9)
        writeInt(&buf, seconds)
        writeInt(&buf, nanoseconds)
    }
}
