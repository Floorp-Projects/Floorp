# This is how we find and load the dynamic library provided by the component.
# For now we just look it up by name.
#
# XXX TODO: This will probably grow some magic for resolving megazording in future.
# E.g. we might start by looking for the named component in `libuniffi.so` and if
# that fails, fall back to loading it separately from `lib${componentName}.so`.

from pathlib import Path

def loadIndirect():
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

    lib = libname.format("{{ config.cdylib_name() }}")
    path = str(Path(__file__).parent / lib)
    return ctypes.cdll.LoadLibrary(path)

# A ctypes library to expose the extern-C FFI definitions.
# This is an implementation detail which will be called internally by the public API.

_UniFFILib = loadIndirect()
{%- for func in ci.iter_ffi_function_definitions() %}
_UniFFILib.{{ func.name() }}.argtypes = (
    {%- call py::arg_list_ffi_decl(func) -%}
)
_UniFFILib.{{ func.name() }}.restype = {% match func.return_type() %}{% when Some with (type_) %}{{ type_|ffi_type_name }}{% when None %}None{% endmatch %}
{%- endfor %}
