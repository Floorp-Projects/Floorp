{%- let inner_ffi_converter = inner_type|ffi_converter_name %}

class {{ ffi_converter_name}}(_UniffiConverterRustBuffer):
    @classmethod
    def check_lower(cls, value):
        for item in value:
            {{ inner_ffi_converter }}.check_lower(item)

    @classmethod
    def write(cls, value, buf):
        items = len(value)
        buf.write_i32(items)
        for item in value:
            {{ inner_ffi_converter }}.write(item, buf)

    @classmethod
    def read(cls, buf):
        count = buf.read_i32()
        if count < 0:
            raise InternalError("Unexpected negative sequence length")

        return [
            {{ inner_ffi_converter }}.read(buf) for i in range(count)
        ]
