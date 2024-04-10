# Define some ctypes FFI types that we use in the library

"""
Function pointer for a Rust task, which a callback function that takes a opaque pointer
"""
_UNIFFI_RUST_TASK = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_int8)

def _uniffi_future_callback_t(return_type):
    """
    Factory function to create callback function types for async functions
    """
    return ctypes.CFUNCTYPE(None, ctypes.c_uint64, return_type, _UniffiRustCallStatus)

def _uniffi_load_indirect():
    """
    This is how we find and load the dynamic library provided by the component.
    For now we just look it up by name.
    """
    if sys.platform == "darwin":
        libname = "lib{}.dylib"
    elif sys.platform.startswith("win"):
        # As of python3.8, ctypes does not seem to search $PATH when loading DLLs.
        # We could use `os.add_dll_directory` to configure the search path, but
        # it doesn't feel right to mess with application-wide settings. Let's
        # assume that the `.dll` is next to the `.py` file and load by full path.
        libname = os.path.join(
            os.path.dirname(__file__),
            "{}.dll",
        )
    else:
        # Anything else must be an ELF platform - Linux, *BSD, Solaris/illumos
        libname = "lib{}.so"

    libname = libname.format("{{ config.cdylib_name() }}")
    path = os.path.join(os.path.dirname(__file__), libname)
    lib = ctypes.cdll.LoadLibrary(path)
    return lib

def _uniffi_check_contract_api_version(lib):
    # Get the bindings contract version from our ComponentInterface
    bindings_contract_version = {{ ci.uniffi_contract_version() }}
    # Get the scaffolding contract version by calling the into the dylib
    scaffolding_contract_version = lib.{{ ci.ffi_uniffi_contract_version().name() }}()
    if bindings_contract_version != scaffolding_contract_version:
        raise InternalError("UniFFI contract version mismatch: try cleaning and rebuilding your project")

def _uniffi_check_api_checksums(lib):
    {%- for (name, expected_checksum) in ci.iter_checksums() %}
    if lib.{{ name }}() != {{ expected_checksum }}:
        raise InternalError("UniFFI API checksum mismatch: try cleaning and rebuilding your project")
    {%- else %}
    pass
    {%- endfor %}

# A ctypes library to expose the extern-C FFI definitions.
# This is an implementation detail which will be called internally by the public API.

_UniffiLib = _uniffi_load_indirect()

{%- for def in ci.ffi_definitions() %}
{%- match def %}
{%- when FfiDefinition::CallbackFunction(callback) %}
{{ callback.name()|ffi_callback_name }} = ctypes.CFUNCTYPE(
    {%- match callback.return_type() %}
    {%- when Some(return_type) %}{{ return_type|ffi_type_name }},
    {%- when None %}None,
    {%- endmatch %}
    {%- for arg in callback.arguments() -%}
    {{ arg.type_().borrow()|ffi_type_name }},
    {%- endfor -%}
    {%- if callback.has_rust_call_status_arg() %}
    ctypes.POINTER(_UniffiRustCallStatus),
    {%- endif %}
)
{%- when FfiDefinition::Struct(ffi_struct) %}
class {{ ffi_struct.name()|ffi_struct_name }}(ctypes.Structure):
    _fields_ = [
        {%- for field in ffi_struct.fields() %}
        ("{{ field.name()|var_name }}", {{ field.type_().borrow()|ffi_type_name }}),
        {%- endfor %}
    ]
{%- when FfiDefinition::Function(func) %}
_UniffiLib.{{ func.name() }}.argtypes = (
    {%- call py::arg_list_ffi_decl(func) -%}
)
_UniffiLib.{{ func.name() }}.restype = {% match func.return_type() %}{% when Some with (type_) %}{{ type_|ffi_type_name }}{% when None %}None{% endmatch %}
{%- endmatch %}
{%- endfor %}

{# Ensure to call the contract verification only after we defined all functions. -#}
_uniffi_check_contract_api_version(_UniffiLib)
_uniffi_check_api_checksums(_UniffiLib)
