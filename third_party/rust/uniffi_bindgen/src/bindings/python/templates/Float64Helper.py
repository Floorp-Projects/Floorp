class _UniffiConverterDouble(_UniffiConverterPrimitiveFloat):
    @staticmethod
    def read(buf):
        return buf.read_double()

    @staticmethod
    def write_unchecked(value, buf):
        buf.write_double(value)
