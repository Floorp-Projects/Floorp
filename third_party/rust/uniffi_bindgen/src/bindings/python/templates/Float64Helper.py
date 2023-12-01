class FfiConverterDouble(FfiConverterPrimitiveFloat):
    @staticmethod
    def read(buf):
        return buf.readDouble()

    @staticmethod
    def writeUnchecked(value, buf):
        buf.writeDouble(value)
