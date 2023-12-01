# The Timestamp type.
Timestamp = datetime.datetime

# There is a loss of precision when converting from Rust timestamps,
# which are accurate to the nanosecond,
# to Python datetimes, which have a variable precision due to the use of float as representation.
class FfiConverterTimestamp(FfiConverterRustBuffer):
    @staticmethod
    def read(buf):
        seconds = buf.readI64()
        microseconds = buf.readU32() / 1000
        # Use fromtimestamp(0) then add the seconds using a timedelta.  This
        # ensures that we get OverflowError rather than ValueError when
        # seconds is too large.
        if seconds >= 0:
            return datetime.datetime.fromtimestamp(0, tz=datetime.timezone.utc) + datetime.timedelta(seconds=seconds, microseconds=microseconds)
        else:
            return datetime.datetime.fromtimestamp(0, tz=datetime.timezone.utc) - datetime.timedelta(seconds=-seconds, microseconds=microseconds)

    @staticmethod
    def write(value, buf):
        if value >= datetime.datetime.fromtimestamp(0, datetime.timezone.utc):
            sign = 1
            delta = value - datetime.datetime.fromtimestamp(0, datetime.timezone.utc)
        else:
            sign = -1
            delta = datetime.datetime.fromtimestamp(0, datetime.timezone.utc) - value

        seconds = delta.seconds + delta.days * 24 * 3600
        nanoseconds = delta.microseconds * 1000
        buf.writeI64(sign * seconds)
        buf.writeU32(nanoseconds)
