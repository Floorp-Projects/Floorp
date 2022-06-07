public object FfiConverterDuration: FfiConverterRustBuffer<java.time.Duration> {
    override fun read(buf: ByteBuffer): java.time.Duration {
        // Type mismatch (should be u64) but we check for overflow/underflow below
        val seconds = buf.getLong()
        // Type mismatch (should be u32) but we check for overflow/underflow below
        val nanoseconds = buf.getInt().toLong()
        if (seconds < 0) {
            throw java.time.DateTimeException("Duration exceeds minimum or maximum value supported by uniffi")
        }
        if (nanoseconds < 0) {
            throw java.time.DateTimeException("Duration nanoseconds exceed minimum or maximum supported by uniffi")
        }
        return java.time.Duration.ofSeconds(seconds, nanoseconds)
    }

    // 8 bytes for seconds, 4 bytes for nanoseconds
    override fun allocationSize(value: java.time.Duration) = 12

    override fun write(value: java.time.Duration, buf: ByteBuffer) {
        if (value.seconds < 0) {
            // Rust does not support negative Durations
            throw IllegalArgumentException("Invalid duration, must be non-negative")
        }

        if (value.nano < 0) {
            // Java docs provide guarantee that nano will always be positive, so this should be impossible
            // See: https://docs.oracle.com/javase/8/docs/api/java/time/Duration.html
            throw IllegalArgumentException("Invalid duration, nano value must be non-negative")
        }

        // Type mismatch (should be u64) but since Rust doesn't support negative durations we should be OK
        buf.putLong(value.seconds)
        // Type mismatch (should be u32) but since values will always be between 0 and 999,999,999 it should be OK
        buf.putInt(value.nano)
    }
}
