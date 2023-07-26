class FfiConverterUInt16(FfiConverterPrimitiveInt):
    CLASS_NAME = "u16"
    VALUE_MIN = 0
    VALUE_MAX = 2**16

    @staticmethod
    def read(buf):
        return buf.readU16()

    @staticmethod
    def writeUnchecked(value, buf):
        buf.writeU16(value)
