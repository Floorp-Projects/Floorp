
/// Export info about the UDL while used to create us
/// See `uniffi_bindgen::macro_metadata` for how this is used.

// ditto for info about the UDL which spawned us.
{%- let const_udl_var = "UNIFFI_META_CONST_UDL_{}"|format(ci.namespace().to_shouty_snake_case()) %}
{%- let static_udl_var = "UNIFFI_META_UDL_{}"|format(ci.namespace().to_shouty_snake_case()) %}

const {{ const_udl_var }}: ::uniffi::MetadataBuffer = ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::UDL_FILE)
    .concat_str("{{ ci.types.namespace.crate_name }}")
    .concat_str("{{ ci.namespace() }}")
    .concat_str("{{ udl_base_name }}");

#[doc(hidden)]
#[no_mangle]
pub static {{ static_udl_var }}: [u8; {{ const_udl_var }}.size] = {{ const_udl_var }}.into_array();
