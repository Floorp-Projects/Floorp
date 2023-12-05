class FfiConverterInt64(FfiConverterPrimitiveInt):
    CLASS_NAME = "i64"
    VALUE_MIN = -2**63
    VALUE_MAX = 2**63

    @staticmethod
    def read(buf):
        return buf.readI64()

    @staticmethod
    def writeUnchecked(value, buf):
        buf.writeI64(value)
