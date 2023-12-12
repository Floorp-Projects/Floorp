class _UniffiConverterUInt16(_UniffiConverterPrimitiveInt):
    CLASS_NAME = "u16"
    VALUE_MIN = 0
    VALUE_MAX = 2**16

    @staticmethod
    def read(buf):
        return buf.read_u16()

    @staticmethod
    def write_unchecked(value, buf):
        buf.write_u16(value)
