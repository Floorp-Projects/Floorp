class _UniffiConverterBool(_UniffiConverterPrimitive):
    @classmethod
    def check(cls, value):
        return not not value

    @classmethod
    def read(cls, buf):
        return cls.lift(buf.read_u8())

    @classmethod
    def write_unchecked(cls, value, buf):
        buf.write_u8(value)

    @staticmethod
    def lift(value):
        return value != 0
