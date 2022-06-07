class FfiConverterUInt8(FfiConverterPrimitive):
    @staticmethod
    def read(buf):
        return buf.readU8()

    @staticmethod
    def write(value, buf):
        buf.writeU8(value)
