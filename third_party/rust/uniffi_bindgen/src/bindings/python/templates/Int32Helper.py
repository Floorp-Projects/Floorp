class _UniffiConverterInt32(_UniffiConverterPrimitiveInt):
    CLASS_NAME = "i32"
    VALUE_MIN = -2**31
    VALUE_MAX = 2**31

    @staticmethod
    def read(buf):
        return buf.read_i32()

    @staticmethod
    def write_unchecked(value, buf):
        buf.write_i32(value)
