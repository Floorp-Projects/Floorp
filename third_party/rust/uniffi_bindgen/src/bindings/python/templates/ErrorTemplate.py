# {{ type_name }}
# We want to define each variant as a nested class that's also a subclass,
# which is tricky in Python.  To accomplish this we're going to create each
# class separately, then manually add the child classes to the base class's
# __dict__.  All of this happens in dummy class to avoid polluting the module
# namespace.
class {{ type_name }}(Exception):
    pass

_UniffiTemp{{ type_name }} = {{ type_name }}

class {{ type_name }}:  # type: ignore
    {%- for variant in e.variants() -%}
    {%- let variant_type_name = variant.name()|class_name -%}
    {%- if e.is_flat() %}
    class {{ variant_type_name }}(_UniffiTemp{{ type_name }}):
        def __repr__(self):
            return "{{ type_name }}.{{ variant_type_name }}({})".format(repr(str(self)))
    {%- else %}
    class {{ variant_type_name }}(_UniffiTemp{{ type_name }}):
        def __init__(self{% for field in variant.fields() %}, {{ field.name()|var_name }}{% endfor %}):
            {%- if variant.has_fields() %}
            super().__init__(", ".join([
                {%- for field in variant.fields() %}
                "{{ field.name()|var_name }}={!r}".format({{ field.name()|var_name }}),
                {%- endfor %}
            ]))
            {%- for field in variant.fields() %}
            self.{{ field.name()|var_name }} = {{ field.name()|var_name }}
            {%- endfor %}
            {%- else %}
            pass
            {%- endif %}
        def __repr__(self):
            return "{{ type_name }}.{{ variant_type_name }}({})".format(str(self))
    {%- endif %}
    _UniffiTemp{{ type_name }}.{{ variant_type_name }} = {{ variant_type_name }} # type: ignore
    {%- endfor %}

{{ type_name }} = _UniffiTemp{{ type_name }} # type: ignore
del _UniffiTemp{{ type_name }}


class {{ ffi_converter_name }}(_UniffiConverterRustBuffer):
    @staticmethod
    def read(buf):
        variant = buf.read_i32()
        {%- for variant in e.variants() %}
        if variant == {{ loop.index }}:
            return {{ type_name }}.{{ variant.name()|class_name }}(
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
        if isinstance(value, {{ type_name }}.{{ variant.name()|class_name }}):
            buf.write_i32({{ loop.index }})
            {%- for field in variant.fields() %}
            {{ field|write_fn }}(value.{{ field.name()|var_name }}, buf)
            {%- endfor %}
        {%- endfor %}
