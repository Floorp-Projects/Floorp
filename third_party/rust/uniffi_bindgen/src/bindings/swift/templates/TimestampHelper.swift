fileprivate struct FfiConverterTimestamp: FfiConverterRustBuffer {
    typealias SwiftType = Date

    static func read(from buf: Reader) throws -> Date {
        let seconds: Int64 = try buf.readInt()
        let nanoseconds: UInt32 = try buf.readInt()
        if seconds >= 0 {
            let delta = Double(seconds) + (Double(nanoseconds) / 1.0e9)
            return Date.init(timeIntervalSince1970: delta)
        } else {
            let delta = Double(seconds) - (Double(nanoseconds) / 1.0e9)
            return Date.init(timeIntervalSince1970: delta)
        }
    }

    static func write(_ value: Date, into buf: Writer) {
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
        buf.writeInt(sign * seconds)
        buf.writeInt(nanoseconds)
    }
}
