class FfiConverterInt8(FfiConverterPrimitiveInt):
    CLASS_NAME = "i8"
    VALUE_MIN = -2**7
    VALUE_MAX = 2**7

    @staticmethod
    def read(buf):
        return buf.readI8()

    @staticmethod
    def writeUnchecked(value, buf):
        buf.writeI8(value)
