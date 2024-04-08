class _UniffiConverterUInt8(_UniffiConverterPrimitiveInt):
    CLASS_NAME = "u8"
    VALUE_MIN = 0
    VALUE_MAX = 2**8

    @staticmethod
    def read(buf):
        return buf.read_u8()

    @staticmethod
    def write(value, buf):
        buf.write_u8(value)
