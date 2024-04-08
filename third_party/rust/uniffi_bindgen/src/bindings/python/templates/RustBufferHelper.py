# Types conforming to `_UniffiConverterPrimitive` pass themselves directly over the FFI.
class _UniffiConverterPrimitive:
    @classmethod
    def lift(cls, value):
        return value

    @classmethod
    def lower(cls, value):
        return value

class _UniffiConverterPrimitiveInt(_UniffiConverterPrimitive):
    @classmethod
    def check_lower(cls, value):
        try:
            value = value.__index__()
        except Exception:
            raise TypeError("'{}' object cannot be interpreted as an integer".format(type(value).__name__))
        if not isinstance(value, int):
            raise TypeError("__index__ returned non-int (type {})".format(type(value).__name__))
        if not cls.VALUE_MIN <= value < cls.VALUE_MAX:
            raise ValueError("{} requires {} <= value < {}".format(cls.CLASS_NAME, cls.VALUE_MIN, cls.VALUE_MAX))

class _UniffiConverterPrimitiveFloat(_UniffiConverterPrimitive):
    @classmethod
    def check_lower(cls, value):
        try:
            value = value.__float__()
        except Exception:
            raise TypeError("must be real number, not {}".format(type(value).__name__))
        if not isinstance(value, float):
            raise TypeError("__float__ returned non-float (type {})".format(type(value).__name__))

# Helper class for wrapper types that will always go through a _UniffiRustBuffer.
# Classes should inherit from this and implement the `read` and `write` static methods.
class _UniffiConverterRustBuffer:
    @classmethod
    def lift(cls, rbuf):
        with rbuf.consume_with_stream() as stream:
            return cls.read(stream)

    @classmethod
    def lower(cls, value):
        with _UniffiRustBuffer.alloc_with_builder() as builder:
            cls.write(value, builder)
            return builder.finalize()
