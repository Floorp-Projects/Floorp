class FfiConverterBool:
    @classmethod
    def read(cls, buf):
        return cls.lift(buf.readU8())

    @classmethod
    def write(cls, value, buf):
        buf.writeU8(cls.lower(value))

    @staticmethod
    def lift(value):
        return int(value) != 0

    @staticmethod
    def lower(value):
        return 1 if value else 0
