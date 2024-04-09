class _UniffiConverterUInt32(_UniffiConverterPrimitiveInt):
    CLASS_NAME = "u32"
    VALUE_MIN = 0
    VALUE_MAX = 2**32

    @staticmethod
    def read(buf):
        return buf.read_u32()

    @staticmethod
    def write(value, buf):
        buf.write_u32(value)
