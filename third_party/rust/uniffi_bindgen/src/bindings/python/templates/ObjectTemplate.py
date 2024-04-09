{%- let obj = ci|get_object_definition(name) %}
{%- let (protocol_name, impl_name) = obj|object_names %}
{%- let methods = obj.methods() %}
{%- let protocol_docstring = obj.docstring() %}

{% include "Protocol.py" %}

{% if ci.is_name_used_as_error(name) %}
class {{ impl_name }}(Exception):
{%- else %}
class {{ impl_name }}:
{%- endif %}
    {%- call py::docstring(obj, 4) %}
    _pointer: ctypes.c_void_p

{%- match obj.primary_constructor() %}
{%-     when Some with (cons) %}
{%-         if cons.is_async() %}
    def __init__(self, *args, **kw):
        raise ValueError("async constructors not supported.")
{%-         else %}
    def __init__(self, {% call py::arg_list_decl(cons) -%}):
        {%- call py::docstring(cons, 8) %}
        {%- call py::setup_args_extra_indent(cons) %}
        self._pointer = {% call py::to_ffi_call(cons) %}
{%-         endif %}
{%-     when None %}
    {# no __init__ means simple construction without a pointer works, which can confuse #}
    def __init__(self, *args, **kwargs):
        raise ValueError("This class has no default constructor")
{%- endmatch %}

    def __del__(self):
        # In case of partial initialization of instances.
        pointer = getattr(self, "_pointer", None)
        if pointer is not None:
            _rust_call(_UniffiLib.{{ obj.ffi_object_free().name() }}, pointer)

    def _uniffi_clone_pointer(self):
        return _rust_call(_UniffiLib.{{ obj.ffi_object_clone().name() }}, self._pointer)

    # Used by alternative constructors or any methods which return this type.
    @classmethod
    def _make_instance_(cls, pointer):
        # Lightly yucky way to bypass the usual __init__ logic
        # and just create a new instance with the required pointer.
        inst = cls.__new__(cls)
        inst._pointer = pointer
        return inst

{%- for cons in obj.alternate_constructors() %}

    @classmethod
{%-  if cons.is_async() %}
    async def {{ cons.name()|fn_name }}(cls, {% call py::arg_list_decl(cons) %}):
        {%- call py::docstring(cons, 8) %}
        {%- call py::setup_args_extra_indent(cons) %}

        return await _uniffi_rust_call_async(
            _UniffiLib.{{ cons.ffi_func().name() }}({% call py::arg_list_lowered(cons) %}),
            _UniffiLib.{{ cons.ffi_rust_future_poll(ci) }},
            _UniffiLib.{{ cons.ffi_rust_future_complete(ci) }},
            _UniffiLib.{{ cons.ffi_rust_future_free(ci) }},
            {{ ffi_converter_name }}.lift,
            {% call py::error_ffi_converter(cons) %}
        )
{%-  else %}
    def {{ cons.name()|fn_name }}(cls, {% call py::arg_list_decl(cons) %}):
        {%- call py::docstring(cons, 8) %}
        {%- call py::setup_args_extra_indent(cons) %}
        # Call the (fallible) function before creating any half-baked object instances.
        pointer = {% call py::to_ffi_call(cons) %}
        return cls._make_instance_(pointer)
{%-  endif %}
{% endfor %}

{%- for meth in obj.methods() -%}
    {%- call py::method_decl(meth.name()|fn_name, meth) %}
{%- endfor %}
{%- for tm in obj.uniffi_traits() -%}
{%-     match tm %}
{%-         when UniffiTrait::Debug { fmt } %}
            {%- call py::method_decl("__repr__", fmt) %}
{%-         when UniffiTrait::Display { fmt } %}
            {%- call py::method_decl("__str__", fmt) %}
{%-         when UniffiTrait::Eq { eq, ne } %}
    def __eq__(self, other: object) -> {{ eq.return_type().unwrap()|type_name }}:
        if not isinstance(other, {{ type_name }}):
            return NotImplemented

        return {{ eq.return_type().unwrap()|lift_fn }}({% call py::to_ffi_call_with_prefix("self._uniffi_clone_pointer()", eq) %})

    def __ne__(self, other: object) -> {{ ne.return_type().unwrap()|type_name }}:
        if not isinstance(other, {{ type_name }}):
            return NotImplemented

        return {{ ne.return_type().unwrap()|lift_fn }}({% call py::to_ffi_call_with_prefix("self._uniffi_clone_pointer()", ne) %})
{%-         when UniffiTrait::Hash { hash } %}
            {%- call py::method_decl("__hash__", hash) %}
{%-      endmatch %}
{%- endfor %}

{%- if obj.has_callback_interface() %}
{%- let ffi_init_callback = obj.ffi_init_callback() %}
{%- let vtable = obj.vtable().expect("trait interface should have a vtable") %}
{%- let vtable_methods = obj.vtable_methods() %}
{% include "CallbackInterfaceImpl.py" %}
{%- endif %}

{# Objects as error #}
{%- if ci.is_name_used_as_error(name) %}
{# Due to some mismatches in the ffi converter mechanisms, errors are forced to be a RustBuffer #}
class {{ ffi_converter_name }}__as_error(_UniffiConverterRustBuffer):
    @classmethod
    def read(cls, buf):
        raise NotImplementedError()

    @classmethod
    def write(cls, value, buf):
        raise NotImplementedError()

    @staticmethod
    def lift(value):
        # Errors are always a rust buffer holding a pointer - which is a "read"
        with value.consume_with_stream() as stream:
            return {{ ffi_converter_name }}.read(stream)

    @staticmethod
    def lower(value):
        raise NotImplementedError()

{%- endif %}

class {{ ffi_converter_name }}:
    {%- if obj.has_callback_interface() %}
    _handle_map = _UniffiHandleMap()
    {%- endif %}

    @staticmethod
    def lift(value: int):
        return {{ impl_name }}._make_instance_(value)

    @staticmethod
    def check_lower(value: {{ type_name }}):
        {%- if obj.has_callback_interface() %}
        pass
        {%- else %}
        if not isinstance(value, {{ impl_name }}):
            raise TypeError("Expected {{ impl_name }} instance, {} found".format(type(value).__name__))
        {%- endif %}

    @staticmethod
    def lower(value: {{ protocol_name }}):
        {%- if obj.has_callback_interface() %}
        return {{ ffi_converter_name }}._handle_map.insert(value)
        {%- else %}
        if not isinstance(value, {{ impl_name }}):
            raise TypeError("Expected {{ impl_name }} instance, {} found".format(type(value).__name__))
        return value._uniffi_clone_pointer()
        {%- endif %}

    @classmethod
    def read(cls, buf: _UniffiRustBuffer):
        ptr = buf.read_u64()
        if ptr == 0:
            raise InternalError("Raw pointer value was null")
        return cls.lift(ptr)

    @classmethod
    def write(cls, value: {{ protocol_name }}, buf: _UniffiRustBuffer):
        buf.write_u64(cls.lower(value))
