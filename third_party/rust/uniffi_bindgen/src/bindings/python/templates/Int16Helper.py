class FfiConverterInt16(FfiConverterPrimitive):
    @staticmethod
    def read(buf):
        return buf.readI16()

    @staticmethod
    def write(value, buf):
        buf.writeI16(value)
