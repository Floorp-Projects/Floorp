class FfiConverterUInt8(FfiConverterPrimitiveInt):
    CLASS_NAME = "u8"
    VALUE_MIN = 0
    VALUE_MAX = 2**8

    @staticmethod
    def read(buf):
        return buf.readU8()

    @staticmethod
    def writeUnchecked(value, buf):
        buf.writeU8(value)
