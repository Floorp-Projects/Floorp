class FfiConverterUInt16(FfiConverterPrimitive):
    @staticmethod
    def read(buf):
        return buf.readU16()

    @staticmethod
    def write(value, buf):
        buf.writeU16(value)
