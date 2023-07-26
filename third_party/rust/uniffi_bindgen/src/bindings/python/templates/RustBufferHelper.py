# Types conforming to `FfiConverterPrimitive` pass themselves directly over the FFI.
class FfiConverterPrimitive:
    @classmethod
    def check(cls, value):
        return value

    @classmethod
    def lift(cls, value):
        return value

    @classmethod
    def lower(cls, value):
        return cls.lowerUnchecked(cls.check(value))

    @classmethod
    def lowerUnchecked(cls, value):
        return value

    @classmethod
    def write(cls, value, buf):
        cls.writeUnchecked(cls.check(value), buf)

class FfiConverterPrimitiveInt(FfiConverterPrimitive):
    @classmethod
    def check(cls, value):
        try:
            value = value.__index__()
        except Exception:
            raise TypeError("'{}' object cannot be interpreted as an integer".format(type(value).__name__))
        if not isinstance(value, int):
            raise TypeError("__index__ returned non-int (type {})".format(type(value).__name__))
        if not cls.VALUE_MIN <= value < cls.VALUE_MAX:
            raise ValueError("{} requires {} <= value < {}".format(cls.CLASS_NAME, cls.VALUE_MIN, cls.VALUE_MAX))
        return super().check(value)

class FfiConverterPrimitiveFloat(FfiConverterPrimitive):
    @classmethod
    def check(cls, value):
        try:
            value = value.__float__()
        except Exception:
            raise TypeError("must be real number, not {}".format(type(value).__name__))
        if not isinstance(value, float):
            raise TypeError("__float__ returned non-float (type {})".format(type(value).__name__))
        return super().check(value)

# Helper class for wrapper types that will always go through a RustBuffer.
# Classes should inherit from this and implement the `read` and `write` static methods.
class FfiConverterRustBuffer:
    @classmethod
    def lift(cls, rbuf):
        with rbuf.consumeWithStream() as stream:
            return cls.read(stream)

    @classmethod
    def lower(cls, value):
        with RustBuffer.allocWithBuilder() as builder:
            cls.write(value, builder)
            return builder.finalize()
