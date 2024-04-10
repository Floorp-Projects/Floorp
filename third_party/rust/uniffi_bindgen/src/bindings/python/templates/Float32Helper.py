class _UniffiConverterFloat(_UniffiConverterPrimitiveFloat):
    @staticmethod
    def read(buf):
        return buf.read_float()

    @staticmethod
    def write(value, buf):
        buf.write_float(value)
