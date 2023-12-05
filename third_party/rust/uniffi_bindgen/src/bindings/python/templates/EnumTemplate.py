{#
# Python has a built-in `enum` module which is nice to use, but doesn't support
# variants with associated data. So, we switch here, and generate a stdlib `enum`
# when none of the variants have associated data, or a generic nested-class
# construct when they do.
#}
{% if e.is_flat() %}

class {{ type_name }}(enum.Enum):
    {% for variant in e.variants() -%}
    {{ variant.name()|enum_variant_py }} = {{ loop.index }}
    {% endfor %}
{% else %}

class {{ type_name }}:
    def __init__(self):
        raise RuntimeError("{{ type_name }} cannot be instantiated directly")

    # Each enum variant is a nested class of the enum itself.
    {% for variant in e.variants() -%}
    class {{ variant.name()|enum_variant_py }}:
        {% for field in variant.fields() %}
            {{- field.name()|var_name }}: "{{- field|type_name }}";
        {%- endfor %}

        @typing.no_type_check
        def __init__(self,{% for field in variant.fields() %}{{ field.name()|var_name }}: "{{- field|type_name }}"{% if loop.last %}{% else %}, {% endif %}{% endfor %}):
            {% if variant.has_fields() %}
            {%- for field in variant.fields() %}
            self.{{ field.name()|var_name }} = {{ field.name()|var_name }}
            {%- endfor %}
            {% else %}
            pass
            {% endif %}

        def __str__(self):
            return "{{ type_name }}.{{ variant.name()|enum_variant_py }}({% for field in variant.fields() %}{{ field.name()|var_name }}={}{% if loop.last %}{% else %}, {% endif %}{% endfor %})".format({% for field in variant.fields() %}self.{{ field.name()|var_name }}{% if loop.last %}{% else %}, {% endif %}{% endfor %})

        def __eq__(self, other):
            if not other.is_{{ variant.name()|var_name }}():
                return False
            {%- for field in variant.fields() %}
            if self.{{ field.name()|var_name }} != other.{{ field.name()|var_name }}:
                return False
            {%- endfor %}
            return True
    {% endfor %}

    # For each variant, we have an `is_NAME` method for easily checking
    # whether an instance is that variant.
    {% for variant in e.variants() -%}
    def is_{{ variant.name()|var_name }}(self) -> bool:
        return isinstance(self, {{ type_name }}.{{ variant.name()|enum_variant_py }})
    {% endfor %}

# Now, a little trick - we make each nested variant class be a subclass of the main
# enum class, so that method calls and instance checks etc will work intuitively.
# We might be able to do this a little more neatly with a metaclass, but this'll do.
{% for variant in e.variants() -%}
{{ type_name }}.{{ variant.name()|enum_variant_py }} = type("{{ type_name }}.{{ variant.name()|enum_variant_py }}", ({{ type_name }}.{{variant.name()|enum_variant_py}}, {{ type_name }},), {})  # type: ignore
{% endfor %}

{% endif %}

class {{ ffi_converter_name }}(_UniffiConverterRustBuffer):
    @staticmethod
    def read(buf):
        variant = buf.read_i32()

        {%- for variant in e.variants() %}
        if variant == {{ loop.index }}:
            {%- if e.is_flat() %}
            return {{ type_name }}.{{variant.name()|enum_variant_py}}
            {%- else %}
            return {{ type_name }}.{{variant.name()|enum_variant_py}}(
                {%- for field in variant.fields() %}
                {{ field|read_fn }}(buf),
                {%- endfor %}
            )
            {%- endif %}
        {%- endfor %}
        raise InternalError("Raw enum value doesn't match any cases")

    def write(value, buf):
        {%- for variant in e.variants() %}
        {%- if e.is_flat() %}
        if value == {{ type_name }}.{{ variant.name()|enum_variant_py }}:
            buf.write_i32({{ loop.index }})
        {%- else %}
        if value.is_{{ variant.name()|var_name }}():
            buf.write_i32({{ loop.index }})
            {%- for field in variant.fields() %}
            {{ field|write_fn }}(value.{{ field.name()|var_name }}, buf)
            {%- endfor %}
        {%- endif %}
        {%- endfor %}

