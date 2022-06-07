class FfiConverterDouble(FfiConverterPrimitive):
    @staticmethod
    def read(buf):
        return buf.readDouble()

    @staticmethod
    def write(value, buf):
        buf.writeDouble(value)
