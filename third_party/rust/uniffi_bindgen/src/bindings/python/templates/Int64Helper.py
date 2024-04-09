class _UniffiConverterInt64(_UniffiConverterPrimitiveInt):
    CLASS_NAME = "i64"
    VALUE_MIN = -2**63
    VALUE_MAX = 2**63

    @staticmethod
    def read(buf):
        return buf.read_i64()

    @staticmethod
    def write(value, buf):
        buf.write_i64(value)
