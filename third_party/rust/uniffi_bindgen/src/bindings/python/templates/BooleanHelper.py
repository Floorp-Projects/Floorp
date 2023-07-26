class FfiConverterBool(FfiConverterPrimitive):
    @classmethod
    def check(cls, value):
        return not not value

    @classmethod
    def read(cls, buf):
        return cls.lift(buf.readU8())

    @classmethod
    def writeUnchecked(cls, value, buf):
        buf.writeU8(value)

    @staticmethod
    def lift(value):
        return value != 0
