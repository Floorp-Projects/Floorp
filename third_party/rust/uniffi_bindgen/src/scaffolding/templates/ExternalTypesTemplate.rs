// Support for external types.

// Types with an external `FfiConverter`...
{% for (name, crate_name) in ci.iter_external_types() %}
// `{{ name }}` is defined in `{{ crate_name }}`
use {{ crate_name|crate_name_rs }}::FfiConverterType{{ name }};
{% endfor %}

// For custom scaffolding types we need to generate an FfiConverterType based on the
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
#[doc(hidden)]
pub struct FfiConverterType{{ name }};

unsafe impl uniffi::FfiConverter for FfiConverterType{{ name }} {
    type RustType = {{ name }};
    type FfiType = {{ FFIType::from(builtin).borrow()|type_ffi }};

    fn lower(obj: {{ name }} ) -> Self::FfiType {
        <{{ builtin|type_rs }} as uniffi::FfiConverter>::lower(<{{ name }} as UniffiCustomTypeConverter>::from_custom(obj))
    }

    fn try_lift(v: Self::FfiType) -> uniffi::Result<{{ name }}> {
        <{{ name }} as UniffiCustomTypeConverter>::into_custom(<{{ builtin|type_rs }} as uniffi::FfiConverter>::try_lift(v)?)
    }

    fn write(obj: {{ name }}, buf: &mut Vec<u8>) {
        <{{ builtin|type_rs }} as uniffi::FfiConverter>::write(<{{ name }} as UniffiCustomTypeConverter>::from_custom(obj), buf);
    }

    fn try_read(buf: &mut &[u8]) -> uniffi::Result<{{ name }}> {
        <{{ name }} as UniffiCustomTypeConverter>::into_custom(<{{ builtin|type_rs }} as uniffi::FfiConverter>::try_read(buf)?)
    }
}
{%- endfor -%}
