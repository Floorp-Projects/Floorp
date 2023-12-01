
/// Export namespace metadata.
///
/// See `uniffi_bindgen::macro_metadata` for how this is used.
{%- let const_var = "UNIFFI_META_CONST_NAMESPACE_{}"|format(ci.namespace().to_shouty_snake_case()) %}
{%- let static_var = "UNIFFI_META_NAMESPACE_{}"|format(ci.namespace().to_shouty_snake_case()) %}

const {{ const_var }}: ::uniffi::MetadataBuffer = ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::NAMESPACE)
    .concat_str(env!("CARGO_CRATE_NAME"))
    .concat_str("{{ ci.namespace() }}");

#[doc(hidden)]
#[no_mangle]
pub static {{ static_var }}: [u8; {{ const_var }}.size] = {{ const_var }}.into_array();
