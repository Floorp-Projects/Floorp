class _UniffiConverterInt8(_UniffiConverterPrimitiveInt):
    CLASS_NAME = "i8"
    VALUE_MIN = -2**7
    VALUE_MAX = 2**7

    @staticmethod
    def read(buf):
        return buf.read_i8()

    @staticmethod
    def write(value, buf):
        buf.write_i8(value)
