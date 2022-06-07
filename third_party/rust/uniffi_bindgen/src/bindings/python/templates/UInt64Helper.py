class FfiConverterUInt64(FfiConverterPrimitive):
    @staticmethod
    def read(buf):
        return buf.readU64()

    @staticmethod
    def write(value, buf):
        buf.writeU64(value)
