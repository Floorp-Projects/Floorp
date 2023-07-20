class FfiConverterUInt32(FfiConverterPrimitive):
    @staticmethod
    def read(buf):
        return buf.readU32()

    @staticmethod
    def write(value, buf):
        buf.writeU32(value)
