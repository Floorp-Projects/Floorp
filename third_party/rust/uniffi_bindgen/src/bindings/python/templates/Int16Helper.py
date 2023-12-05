class FfiConverterInt16(FfiConverterPrimitiveInt):
    CLASS_NAME = "i16"
    VALUE_MIN = -2**15
    VALUE_MAX = 2**15

    @staticmethod
    def read(buf):
        return buf.readI16()

    @staticmethod
    def writeUnchecked(value, buf):
        buf.writeI16(value)
