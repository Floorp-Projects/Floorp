class FfiConverterInt8(FfiConverterPrimitive):
    @staticmethod
    def read(buf):
        return buf.readI8()

    @staticmethod
    def write(value, buf):
        buf.writeI8(value)
