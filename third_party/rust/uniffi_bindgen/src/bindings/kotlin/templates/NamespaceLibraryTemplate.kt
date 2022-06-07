@Synchronized
private fun findLibraryName(componentName: String): String {
    val libOverride = System.getProperty("uniffi.component.$componentName.libraryOverride")
    if (libOverride != null) {
        return libOverride
    }
    return "{{ config.cdylib_name() }}"
}

private inline fun <reified Lib : Library> loadIndirect(
    componentName: String
): Lib {
    return Native.load<Lib>(findLibraryName(componentName), Lib::class.java)
}

// A JNA Library to expose the extern-C FFI definitions.
// This is an implementation detail which will be called internally by the public API.

internal interface _UniFFILib : Library {
    companion object {
        internal val INSTANCE: _UniFFILib by lazy {
            loadIndirect<_UniFFILib>(componentName = "{{ ci.namespace() }}")
            {% let initialization_fns = self.initialization_fns() %}
            {%- if !initialization_fns.is_empty() -%}
            .also { lib: _UniFFILib ->
                {% for fn in initialization_fns -%}
                {{ fn }}(lib)
                {% endfor -%}
            }
            {% endif %}
        }
    }

    {% for func in ci.iter_ffi_function_definitions() -%}
    fun {{ func.name() }}(
        {%- call kt::arg_list_ffi_decl(func) %}
    ){%- match func.return_type() -%}{%- when Some with (type_) %}: {{ type_.borrow()|ffi_type_name }}{% when None %}: Unit{% endmatch %}

    {% endfor %}
}
