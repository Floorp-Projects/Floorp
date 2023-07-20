class FfiConverterInt64(FfiConverterPrimitive):
    @staticmethod
    def read(buf):
        return buf.readI64()

    @staticmethod
    def write(value, buf):
        buf.writeI64(value)
