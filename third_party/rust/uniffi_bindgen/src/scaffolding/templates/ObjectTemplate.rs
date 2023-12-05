// For each Object definition, we assume the caller has provided an appropriately-shaped `struct T`
// with an `impl` for each method on the object. We create an `Arc<T>` for "safely" handing out
// references to these structs to foreign language code, and we provide a `pub extern "C"` function
// corresponding to each method.
//
// (Note that "safely" is in "scare quotes" - that's because we use functions on an `Arc` that
// that are inherently unsafe, but the code we generate is safe in practice.)
//
// If the caller's implementation of the struct does not match with the methods or types specified
// in the UDL, then the rust compiler will complain with a (hopefully at least somewhat helpful!)
// error message when processing this generated code.

{%- match obj.imp() -%}
{%- when ObjectImpl::Trait %}
::uniffi::ffi_converter_trait_decl!({{ obj.rust_name() }}, "{{ obj.name() }}", crate::UniFfiTag);
{% else %}
#[::uniffi::ffi_converter_interface(tag = crate::UniFfiTag)]
struct {{ obj.rust_name() }} { }
{% endmatch %}

// All Object structs must be `Sync + Send`. The generated scaffolding will fail to compile
// if they are not, but unfortunately it fails with an unactionably obscure error message.
// By asserting the requirement explicitly, we help Rust produce a more scrutable error message
// and thus help the user debug why the requirement isn't being met.
uniffi::deps::static_assertions::assert_impl_all!({{ obj.rust_name() }}: Sync, Send);

{% let ffi_free = obj.ffi_object_free() -%}
#[doc(hidden)]
#[no_mangle]
pub extern "C" fn {{ ffi_free.name() }}(ptr: *const std::os::raw::c_void, call_status: &mut uniffi::RustCallStatus) {
    uniffi::rust_call(call_status, || {
        assert!(!ptr.is_null());
        {%- match obj.imp() -%}
        {%- when ObjectImpl::Trait %}
        {#- turn it into a Box<Arc<T> and explicitly drop it. #}
        drop(unsafe { Box::from_raw(ptr as *mut std::sync::Arc<{{ obj.rust_name() }}>) });
        {%- when ObjectImpl::Struct %}
        {#- turn it into an Arc and explicitly drop it. #}
        drop(unsafe { ::std::sync::Arc::from_raw(ptr as *const {{ obj.rust_name() }}) });
        {% endmatch %}
        Ok(())
    })
}

{%- for cons in obj.constructors() %}
    #[doc(hidden)]
    #[no_mangle]
    pub extern "C" fn r#{{ cons.ffi_func().name() }}(
        {%- call rs::arg_list_ffi_decl(cons.ffi_func()) %}
    ) -> *const std::os::raw::c_void /* *const {{ obj.name() }} */ {
        uniffi::deps::log::debug!("{{ cons.ffi_func().name() }}");

        // If the constructor does not have the same signature as declared in the UDL, then
        // this attempt to call it will fail with a (somewhat) helpful compiler error.
        uniffi::rust_call(call_status, || {
            {{ cons|return_ffi_converter }}::lower_return(
                {%- if cons.throws() %}
                {{ obj.rust_name() }}::{% call rs::to_rs_call(cons) %}.map(::std::sync::Arc::new).map_err(Into::into)
                {%- else %}
                ::std::sync::Arc::new({{ obj.rust_name() }}::{% call rs::to_rs_call(cons) %})
                {%- endif %}
            )
        })
    }
{%- endfor %}

{%- for meth in obj.methods() %}
    {% call rs::method_decl_prelude(meth) %}
            <{{ obj.rust_name() }}>::{% call rs::to_rs_call(meth) %}
    {% call rs::method_decl_postscript(meth) %}
{% endfor %}

{%- for tm in obj.uniffi_traits() %}
{#      All magic methods get an explicit shim #}
{%      match tm %}
{%          when UniffiTrait::Debug { fmt }%}
    {% call rs::method_decl_prelude(fmt) %}
        {
            uniffi::deps::static_assertions::assert_impl_all!({{ obj.rust_name() }}: std::fmt::Debug); // This object has a trait method which requires `Debug` be implemented.
            format!(
                "{:?}",
                match<std::sync::Arc<{{ obj.rust_name() }}> as ::uniffi::FfiConverter<crate::UniFfiTag>>::try_lift(r#ptr) {
                    Ok(ref val) => val,
                    Err(err) => panic!("Failed to convert arg '{}': {}", "ptr", err),
                }
            )
        }
    {% call rs::method_decl_postscript(fmt) %}
{%          when UniffiTrait::Display { fmt }%}
    {% call rs::method_decl_prelude(fmt) %}
        {
            uniffi::deps::static_assertions::assert_impl_all!({{ obj.rust_name() }}: std::fmt::Display); // This object has a trait method which requires `Display` be implemented.
            format!(
                "{}",
                match<std::sync::Arc<{{ obj.rust_name() }}> as ::uniffi::FfiConverter<crate::UniFfiTag>>::try_lift(r#ptr) {
                    Ok(ref val) => val,
                    Err(err) => panic!("Failed to convert arg '{}': {}", "ptr", err),
                }
            )
        }
    {% call rs::method_decl_postscript(fmt) %}
{%          when UniffiTrait::Hash { hash }%}
    {% call rs::method_decl_prelude(hash) %}
            {
                use ::std::hash::{Hash, Hasher};
                uniffi::deps::static_assertions::assert_impl_all!({{ obj.rust_name() }}: Hash); // This object has a trait method which requires `Hash` be implemented.
                let mut s = ::std::collections::hash_map::DefaultHasher::new();
                Hash::hash(match<std::sync::Arc<{{ obj.rust_name() }}> as ::uniffi::FfiConverter<crate::UniFfiTag>>::try_lift(r#ptr) {
                    Ok(ref val) => val,
                    Err(err) => panic!("Failed to convert arg '{}': {}", "ptr", err),
                }, &mut s);
                s.finish()
            }
    {% call rs::method_decl_postscript(hash) %}
{%          when UniffiTrait::Eq { eq, ne }%}
        {# PartialEq::Eq #}
        {% call rs::method_decl_prelude(eq) %}
            {
                use ::std::cmp::PartialEq;
                uniffi::deps::static_assertions::assert_impl_all!({{ obj.rust_name() }}: PartialEq); // This object has a trait method which requires `PartialEq` be implemented.
                PartialEq::eq({% call rs::_arg_list_rs_call(eq) -%})
            }
        {% call rs::method_decl_postscript(eq) %}
        {# PartialEq::Ne #}
        {% call rs::method_decl_prelude(ne) %}
            {
                use ::std::cmp::PartialEq;
                uniffi::deps::static_assertions::assert_impl_all!({{ obj.rust_name() }}: PartialEq); // This object has a trait method which requires `PartialEq` be implemented.
                PartialEq::ne({% call rs::_arg_list_rs_call(ne) -%})
            }
        {% call rs::method_decl_postscript(ne) %}
{%      endmatch %}
{% endfor %}
