class FfiConverterFloat(FfiConverterPrimitive):
    @staticmethod
    def read(buf):
        return buf.readFloat()

    @staticmethod
    def write(value, buf):
        buf.writeFloat(value)
