{%- let e = self.inner() %}
class {{ e|type_name }}(Exception):
    {%- if e.is_flat() %}

    # Each variant is a nested class of the error itself.
    # It just carries a string error message, so no special implementation is necessary.
    {%- for variant in e.variants() %}
    class {{ variant.name()|class_name }}(Exception):
        pass
    {%- endfor %}

    {%- else %}

    # Each variant is a nested class of the error itself.
    {%- for variant in e.variants() %}
    class {{ variant.name()|class_name }}(Exception):
        def __init__(self{% for field in variant.fields() %}, {{ field.name()|var_name }}{% endfor %}):
            {%- if variant.has_fields() %}
            {%- for field in variant.fields() %}
            self.{{ field.name()|var_name }} = {{ field.name()|var_name }}
            {%- endfor %}
            {%- else %}
            pass
            {%- endif %}

        def __str__(self):
            {%- if variant.has_fields() %}
            field_parts = [
                {%- for field in variant.fields() %}
                '{{ field.name() }}={!r}'.format(self.{{ field.name() }}),
                {%- endfor %}
            ]
            return "{{ e|type_name }}.{{ variant.name()|class_name }}({})".format(', '.join(field_parts))
            {%- else %}
            return "{{ e|type_name }}.{{ variant.name()|class_name }}"
            {%- endif %}

    {%- endfor %}
    {%- endif %}

class {{ e|ffi_converter_name }}(FfiConverterRustBuffer):
    @staticmethod
    def read(buf):
        variant = buf.readI32()
        {%- for variant in e.variants() %}
        if variant == {{ loop.index }}:
            return {{ e|type_name }}.{{ variant.name()|class_name }}(
                {%- if e.is_flat() %}
                {{ Type::String.borrow()|read_fn }}(buf),
                {%- else %}
                {%- for field in variant.fields() %}
                {{ field.name()|var_name }}={{ field|read_fn }}(buf),
                {%- endfor %}
                {%- endif %}
            )
        {%- endfor %}
        raise InternalError("Raw enum value doesn't match any cases")

    @staticmethod
    def write(value, buf):
        {%- for variant in e.variants() %}
        if isinstance(value, {{ e|type_name }}.{{ variant.name()|class_name }}):
            buf.writeI32({{ loop.index }})
            {%- for field in variant.fields() %}
            {{ field|write_fn }}(value.{{ field.name()|var_name }}, buf)
            {%- endfor %}
        {%- endfor %}
