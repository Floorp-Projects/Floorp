{%- let e = ci.get_error_definition(name).unwrap() %}

# {{ type_name }}
# We want to define each variant as a nested class that's also a subclass,
# which is tricky in Python.  To accomplish this we're going to create each
# class separated, then manually add the child classes to the base class's
# __dict__.  All of this happens in dummy class to avoid polluting the module
# namespace.
class UniFFIExceptionTmpNamespace:
    class {{ type_name }}(Exception):
        pass
    {% for variant in e.variants() %}
    {%- let variant_type_name = variant.name()|class_name %}

    {%- if e.is_flat() %}
    class {{ variant_type_name }}({{ type_name }}):
        def __str__(self):
            return "{{ type_name }}.{{ variant_type_name }}({})".format(repr(super().__str__()))
    {%- else %}
    class {{ variant_type_name }}({{ type_name }}):
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
                '{{ field.name()|var_name }}={!r}'.format(self.{{ field.name()|var_name }}),
                {%- endfor %}
            ]
            return "{{ type_name }}.{{ variant_type_name }}({})".format(', '.join(field_parts))
            {%- else %}
            return "{{ type_name }}.{{ variant_type_name }}()"
            {%- endif %}
    {%- endif %}

    {{ type_name }}.{{ variant_type_name }} = {{ variant_type_name }}
    {%- endfor %}
{{ type_name }} = UniFFIExceptionTmpNamespace.{{ type_name }}
del UniFFIExceptionTmpNamespace


class {{ ffi_converter_name }}(FfiConverterRustBuffer):
    @staticmethod
    def read(buf):
        variant = buf.readI32()
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
            buf.writeI32({{ loop.index }})
            {%- for field in variant.fields() %}
            {{ field|write_fn }}(value.{{ field.name()|var_name }}, buf)
            {%- endfor %}
        {%- endfor %}
