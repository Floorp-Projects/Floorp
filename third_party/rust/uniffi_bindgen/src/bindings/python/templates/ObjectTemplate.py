{%- let obj = ci.get_object_definition(name).unwrap() %}

class {{ type_name }}(object):
    {%- match obj.primary_constructor() %}
    {%- when Some with (cons) %}
    def __init__(self, {% call py::arg_list_decl(cons) -%}):
        {%- call py::setup_args_extra_indent(cons) %}
        self._pointer = {% call py::to_ffi_call(cons) %}
    {%- when None %}
    {%- endmatch %}

    def __del__(self):
        # In case of partial initialization of instances.
        pointer = getattr(self, "_pointer", None)
        if pointer is not None:
            rust_call(_UniFFILib.{{ obj.ffi_object_free().name() }}, pointer)

    # Used by alternative constructors or any methods which return this type.
    @classmethod
    def _make_instance_(cls, pointer):
        # Lightly yucky way to bypass the usual __init__ logic
        # and just create a new instance with the required pointer.
        inst = cls.__new__(cls)
        inst._pointer = pointer
        return inst

    {% for cons in obj.alternate_constructors() -%}
    @classmethod
    def {{ cons.name()|fn_name }}(cls, {% call py::arg_list_decl(cons) %}):
        {%- call py::setup_args_extra_indent(cons) %}
        # Call the (fallible) function before creating any half-baked object instances.
        pointer = {% call py::to_ffi_call(cons) %}
        return cls._make_instance_(pointer)
    {% endfor %}

    {% for meth in obj.methods() -%}
    {%- match meth.return_type() -%}

    {%- when Some with (return_type) -%}
    def {{ meth.name()|fn_name }}(self, {% call py::arg_list_decl(meth) %}):
        {%- call py::setup_args_extra_indent(meth) %}
        return {{ return_type|lift_fn }}(
            {% call py::to_ffi_call_with_prefix("self._pointer", meth) %}
        )

    {%- when None -%}
    def {{ meth.name()|fn_name }}(self, {% call py::arg_list_decl(meth) %}):
        {%- call py::setup_args_extra_indent(meth) %}
        {% call py::to_ffi_call_with_prefix("self._pointer", meth) %}
    {% endmatch %}
    {% endfor %}


class {{ ffi_converter_name }}:
    @classmethod
    def read(cls, buf):
        ptr = buf.readU64()
        if ptr == 0:
            raise InternalError("Raw pointer value was null")
        return cls.lift(ptr)

    @classmethod
    def write(cls, value, buf):
        if not isinstance(value, {{ type_name }}):
            raise TypeError("Expected {{ type_name }} instance, {} found".format(value.__class__.__name__))
        buf.writeU64(cls.lower(value))

    @staticmethod
    def lift(value):
        return {{ type_name }}._make_instance_(value)

    @staticmethod
    def lower(value):
        return value._pointer
