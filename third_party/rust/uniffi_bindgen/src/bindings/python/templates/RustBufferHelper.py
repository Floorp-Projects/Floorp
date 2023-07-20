# Types conforming to `FfiConverterPrimitive` pass themselves directly over the FFI.
class FfiConverterPrimitive:
    @classmethod
    def lift(cls, value):
        return value

    @classmethod
    def lower(cls, value):
        return value

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
