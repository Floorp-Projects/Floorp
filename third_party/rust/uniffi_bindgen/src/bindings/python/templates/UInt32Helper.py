class FfiConverterUInt32(FfiConverterPrimitiveInt):
    CLASS_NAME = "u32"
    VALUE_MIN = 0
    VALUE_MAX = 2**32

    @staticmethod
    def read(buf):
        return buf.readU32()

    @staticmethod
    def writeUnchecked(value, buf):
        buf.writeU32(value)
