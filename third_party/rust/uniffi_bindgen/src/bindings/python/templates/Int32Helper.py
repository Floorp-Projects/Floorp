class FfiConverterInt32(FfiConverterPrimitive):
    @staticmethod
    def read(buf):
        return buf.readI32()

    @staticmethod
    def write(value, buf):
        buf.writeI32(value)
