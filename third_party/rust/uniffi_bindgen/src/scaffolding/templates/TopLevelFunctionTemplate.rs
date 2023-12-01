{#
// For each top-level function declared in the UDL, we assume the caller has provided a corresponding
// rust function of the same name. We provide a `pub extern "C"` wrapper that does type conversions to
// send data across the FFI, which will fail to compile if the provided function does not match what's
// specified in the UDL.    
#}
#[doc(hidden)]
#[no_mangle]
#[allow(clippy::let_unit_value,clippy::unit_arg)] // The generated code uses the unit type like other types to keep things uniform
pub extern "C" fn r#{{ func.ffi_func().name() }}(
    {% call rs::arg_list_ffi_decl(func.ffi_func()) %}
) {% call rs::return_signature(func) %} {
    // If the provided function does not match the signature specified in the UDL
    // then this attempt to call it will not compile, and will give guidance as to why.
    uniffi::deps::log::debug!("{{ func.ffi_func().name() }}");
    uniffi::rust_call(call_status, || {{ func|return_ffi_converter }}::lower_return(
            {% call rs::to_rs_call(func) %}){% if func.throws() %}.map_err(Into::into){% endif %}
    )
}
