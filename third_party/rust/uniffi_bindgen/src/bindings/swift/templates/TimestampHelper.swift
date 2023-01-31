fileprivate struct FfiConverterTimestamp: FfiConverterRustBuffer {
    typealias SwiftType = Date

    public static func read(from buf: inout (data: Data, offset: Data.Index)) throws -> Date {
        let seconds: Int64 = try readInt(&buf)
        let nanoseconds: UInt32 = try readInt(&buf)
        if seconds >= 0 {
            let delta = Double(seconds) + (Double(nanoseconds) / 1.0e9)
            return Date.init(timeIntervalSince1970: delta)
        } else {
            let delta = Double(seconds) - (Double(nanoseconds) / 1.0e9)
            return Date.init(timeIntervalSince1970: delta)
        }
    }

    public static func write(_ value: Date, into buf: inout [UInt8]) {
        var delta = value.timeIntervalSince1970
        var sign: Int64 = 1
        if delta < 0 {
            // The nanoseconds portion of the epoch offset must always be
            // positive, to simplify the calculation we will use the absolute
            // value of the offset.
            sign = -1
            delta = -delta
        }
        if delta.rounded(.down) > Double(Int64.max) {
            fatalError("Timestamp overflow, exceeds max bounds supported by Uniffi")
        }
        let seconds = Int64(delta)
        let nanoseconds = UInt32((delta - Double(seconds)) * 1.0e9)
        writeInt(&buf, sign * seconds)
        writeInt(&buf, nanoseconds)
    }
}
