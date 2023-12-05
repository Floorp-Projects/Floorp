class FfiConverterFloat(FfiConverterPrimitiveFloat):
    @staticmethod
    def read(buf):
        return buf.readFloat()

    @staticmethod
    def writeUnchecked(value, buf):
        buf.writeFloat(value)
