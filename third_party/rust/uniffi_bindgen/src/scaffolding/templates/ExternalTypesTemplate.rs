// Support for external types.

// Types with an external `FfiConverter`...
{% for (name, crate_name, kind) in ci.iter_external_types() %}
// The FfiConverter for `{{ name }}` is defined in `{{ crate_name }}`
{%- match kind %}
{%- when ExternalKind::DataClass %}
::uniffi::ffi_converter_forward!(r#{{ name }}, ::{{ crate_name|crate_name_rs }}::UniFfiTag, crate::UniFfiTag);
{%- when ExternalKind::Interface %}
::uniffi::ffi_converter_forward!(::std::sync::Arc<r#{{ name }}>, ::{{ crate_name|crate_name_rs }}::UniFfiTag, crate::UniFfiTag);
{%- endmatch %}
{%- endfor %}

// For custom scaffolding types we need to generate an FfiConverter impl based on the
// UniffiCustomTypeConverter implementation that the library supplies
{% for (name, builtin) in ci.iter_custom_types() %}
{% if loop.first %}

// A trait that's in our crate for our external wrapped types to implement.
trait UniffiCustomTypeConverter {
    type Builtin;

    fn into_custom(val: Self::Builtin) -> uniffi::Result<Self> where Self: Sized;
    fn from_custom(obj: Self) -> Self::Builtin;
}

{%- endif -%}

// Type `{{ name }}` wraps a `{{ builtin.canonical_name() }}`

unsafe impl uniffi::FfiConverter<crate::UniFfiTag> for r#{{ name }} {
    type FfiType = {{ FfiType::from(builtin).borrow()|type_ffi }};

    fn lower(obj: {{ name }} ) -> Self::FfiType {
        {{ builtin|ffi_converter }}::lower(<{{ name }} as UniffiCustomTypeConverter>::from_custom(obj))
    }

    fn try_lift(v: Self::FfiType) -> uniffi::Result<{{ name }}> {
        <r#{{ name }} as UniffiCustomTypeConverter>::into_custom({{ builtin|ffi_converter }}::try_lift(v)?)
    }

    fn write(obj: {{ name }}, buf: &mut Vec<u8>) {
        {{ builtin|ffi_converter }}::write(<{{ name }} as UniffiCustomTypeConverter>::from_custom(obj), buf);
    }

    fn try_read(buf: &mut &[u8]) -> uniffi::Result<r#{{ name }}> {
        <{{ name }} as UniffiCustomTypeConverter>::into_custom({{ builtin|ffi_converter }}::try_read(buf)?)
    }

    ::uniffi::ffi_converter_default_return!(crate::UniFfiTag);

    const TYPE_ID_META: ::uniffi::MetadataBuffer = ::uniffi::MetadataBuffer::from_code(::uniffi::metadata::codes::TYPE_CUSTOM)
        .concat_str("{{ name }}")
        .concat({{ builtin|ffi_converter }}::TYPE_ID_META);
}
{%- endfor -%}
