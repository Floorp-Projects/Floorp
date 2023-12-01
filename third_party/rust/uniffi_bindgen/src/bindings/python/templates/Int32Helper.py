class FfiConverterInt32(FfiConverterPrimitiveInt):
    CLASS_NAME = "i32"
    VALUE_MIN = -2**31
    VALUE_MAX = 2**31

    @staticmethod
    def read(buf):
        return buf.readI32()

    @staticmethod
    def writeUnchecked(value, buf):
        buf.writeI32(value)
