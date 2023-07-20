{#
// For each record declared in the UDL, we assume the caller has provided a corresponding
// rust `struct` with the declared fields. We provide the traits for sending it across the FFI.
// If the caller's struct does not match the shape and types declared in the UDL then the rust
// compiler will complain with a type error.
//
// We define a unit-struct to implement the trait to sidestep Rust's orphan rule (ADR-0006). It's
// public so other crates can refer to it via an `[External='crate'] typedef`
#}

#[doc(hidden)]
pub struct {{ rec.type_().borrow()|ffi_converter_name }};

#[doc(hidden)]
impl uniffi::RustBufferFfiConverter for {{ rec.type_().borrow()|ffi_converter_name }} {
    type RustType = r#{{ rec.name() }};

    fn write(obj: r#{{ rec.name() }}, buf: &mut std::vec::Vec<u8>) {
        // If the provided struct doesn't match the fields declared in the UDL, then
        // the generated code here will fail to compile with somewhat helpful error.
        {%- for field in rec.fields() %}
        {{ field.type_().borrow()|ffi_converter }}::write(obj.r#{{ field.name() }}, buf);
        {%- endfor %}
    }

    fn try_read(buf: &mut &[u8]) -> uniffi::deps::anyhow::Result<r#{{ rec.name() }}> {
        Ok(r#{{ rec.name() }} {
            {%- for field in rec.fields() %}
                r#{{ field.name() }}: {{ field.type_().borrow()|ffi_converter }}::try_read(buf)?,
            {%- endfor %}
        })
    }
}
