{%- let inner_type = self.inner() %}
{%- let outer_type = self.outer() %}
{%- let inner_ffi_converter = inner_type|ffi_converter_name %}

class {{ outer_type|ffi_converter_name }}(FfiConverterRustBuffer):
    @classmethod
    def write(cls, value, buf):
        if value is None:
            buf.writeU8(0)
            return

        buf.writeU8(1)
        {{ inner_ffi_converter }}.write(value, buf)

    @classmethod
    def read(cls, buf):
        flag = buf.readU8()
        if flag == 0:
            return None
        elif flag == 1:
            return {{ inner_ffi_converter }}.read(buf)
        else:
            raise InternalError("Unexpected flag byte for optional type")
