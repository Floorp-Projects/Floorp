{%- let inner_type = self.inner() %}
{%- let outer_type = self.outer() %}
{%- let inner_ffi_converter = inner_type|ffi_converter_name %}

class {{ outer_type|ffi_converter_name}}(FfiConverterRustBuffer):
    @classmethod
    def write(cls, value, buf):
        items = len(value)
        buf.writeI32(items)
        for item in value:
            {{ inner_ffi_converter }}.write(item, buf)

    @classmethod
    def read(cls, buf):
        count = buf.readI32()
        if count < 0:
            raise InternalError("Unexpected negative sequence length")

        return [
            {{ inner_ffi_converter }}.read(buf) for i in range(count)
        ]
