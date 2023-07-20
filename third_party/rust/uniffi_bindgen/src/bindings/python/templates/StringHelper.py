class FfiConverterString:
    @staticmethod
    def check(value):
        if not isinstance(value, str):
            raise TypeError("argument must be str, not {}".format(type(value).__name__))
        return value

    @staticmethod
    def read(buf):
        size = buf.readI32()
        if size < 0:
            raise InternalError("Unexpected negative string length")
        utf8Bytes = buf.read(size)
        return utf8Bytes.decode("utf-8")

    @staticmethod
    def write(value, buf):
        value = FfiConverterString.check(value)
        utf8Bytes = value.encode("utf-8")
        buf.writeI32(len(utf8Bytes))
        buf.write(utf8Bytes)

    @staticmethod
    def lift(buf):
        with buf.consumeWithStream() as stream:
            return stream.read(stream.remaining()).decode("utf-8")

    @staticmethod
    def lower(value):
        value = FfiConverterString.check(value)
        with RustBuffer.allocWithBuilder() as builder:
            builder.write(value.encode("utf-8"))
            return builder.finalize()
