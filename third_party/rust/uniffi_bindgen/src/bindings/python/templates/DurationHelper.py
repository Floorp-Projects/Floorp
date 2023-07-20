# The Duration type.
# There is a loss of precision when converting from Rust durations,
# which are accurate to the nanosecond,
# to Python durations, which are only accurate to the microsecond.
class FfiConverterDuration(FfiConverterRustBuffer):
    @staticmethod
    def read(buf):
        seconds = buf.readU64()
        microseconds = buf.readU32() / 1.0e3
        return datetime.timedelta(seconds=seconds, microseconds=microseconds)

    @staticmethod
    def write(value, buf):
        seconds = value.seconds + value.days * 24 * 3600
        nanoseconds = value.microseconds * 1000
        if seconds < 0:
            raise ValueError("Invalid duration, must be non-negative")
        buf.writeI64(seconds)
        buf.writeU32(nanoseconds)
