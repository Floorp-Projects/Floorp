fileprivate struct FfiConverterDuration: FfiConverterRustBuffer {
    typealias SwiftType = TimeInterval

    static func read(from buf: Reader) throws -> TimeInterval {
        let seconds: UInt64 = try buf.readInt()
        let nanoseconds: UInt32 = try buf.readInt()
        return Double(seconds) + (Double(nanoseconds) / 1.0e9)
    }

    static func write(_ value: TimeInterval, into buf: Writer) {
        if value.rounded(.down) > Double(Int64.max) {
            fatalError("Duration overflow, exceeds max bounds supported by Uniffi")
        }

        if value < 0 {
            fatalError("Invalid duration, must be non-negative")
        }

        let seconds = UInt64(value)
        let nanoseconds = UInt32((value - Double(seconds)) * 1.0e9)
        buf.writeInt(seconds)
        buf.writeInt(nanoseconds)
    }
}
