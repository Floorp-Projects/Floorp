class _UniffiConverterBytes(_UniffiConverterRustBuffer):
    @staticmethod
    def read(buf):
        size = buf.read_i32()
        if size < 0:
            raise InternalError("Unexpected negative byte string length")
        return buf.read(size)

    @staticmethod
    def check_lower(value):
        try:
            memoryview(value)
        except TypeError:
            raise TypeError("a bytes-like object is required, not {!r}".format(type(value).__name__))

    @staticmethod
    def write(value, buf):
        buf.write_i32(len(value))
        buf.write(value)
