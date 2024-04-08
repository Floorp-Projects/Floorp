class _UniffiConverterInt16(_UniffiConverterPrimitiveInt):
    CLASS_NAME = "i16"
    VALUE_MIN = -2**15
    VALUE_MAX = 2**15

    @staticmethod
    def read(buf):
        return buf.read_i16()

    @staticmethod
    def write_unchecked(value, buf):
        buf.write_i16(value)
