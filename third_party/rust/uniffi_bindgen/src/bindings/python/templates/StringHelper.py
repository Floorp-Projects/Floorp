class _UniffiConverterString:
    @staticmethod
    def check(value):
        if not isinstance(value, str):
            raise TypeError("argument must be str, not {}".format(type(value).__name__))
        return value

    @staticmethod
    def read(buf):
        size = buf.read_i32()
        if size < 0:
            raise InternalError("Unexpected negative string length")
        utf8_bytes = buf.read(size)
        return utf8_bytes.decode("utf-8")

    @staticmethod
    def write(value, buf):
        value = _UniffiConverterString.check(value)
        utf8_bytes = value.encode("utf-8")
        buf.write_i32(len(utf8_bytes))
        buf.write(utf8_bytes)

    @staticmethod
    def lift(buf):
        with buf.consume_with_stream() as stream:
            return stream.read(stream.remaining()).decode("utf-8")

    @staticmethod
    def lower(value):
        value = _UniffiConverterString.check(value)
        with _UniffiRustBuffer.alloc_with_builder() as builder:
            builder.write(value.encode("utf-8"))
            return builder.finalize()
