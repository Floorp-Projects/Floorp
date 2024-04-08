{%- let inner_ffi_converter = inner_type|ffi_converter_name %}

class {{ ffi_converter_name }}(_UniffiConverterRustBuffer):
    @classmethod
    def check_lower(cls, value):
        if value is not None:
            {{ inner_ffi_converter }}.check_lower(value)

    @classmethod
    def write(cls, value, buf):
        if value is None:
            buf.write_u8(0)
            return

        buf.write_u8(1)
        {{ inner_ffi_converter }}.write(value, buf)

    @classmethod
    def read(cls, buf):
        flag = buf.read_u8()
        if flag == 0:
            return None
        elif flag == 1:
            return {{ inner_ffi_converter }}.read(buf)
        else:
            raise InternalError("Unexpected flag byte for optional type")
