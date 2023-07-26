class FfiConverterUInt64(FfiConverterPrimitiveInt):
    CLASS_NAME = "u64"
    VALUE_MIN = 0
    VALUE_MAX = 2**64

    @staticmethod
    def read(buf):
        return buf.readU64()

    @staticmethod
    def writeUnchecked(value, buf):
        buf.writeU64(value)
