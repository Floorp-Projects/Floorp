{#
// For each enum declared in the UDL, we assume the caller has provided a corresponding
// rust `enum`. We provide the traits for sending it across the FFI, which will fail to
// compile if the provided struct has a different shape to the one declared in the UDL.
//
// We define a unit-struct to implement the trait to sidestep Rust's orphan rule (ADR-0006). It's
// public so other crates can refer to it via an `[External='crate'] typedef`
#}

#[doc(hidden)]
pub struct {{ e.type_().borrow()|ffi_converter_name }};

#[doc(hidden)]
impl uniffi::RustBufferFfiConverter for {{ e.type_().borrow()|ffi_converter_name }} {
    type RustType = {{ e.name() }};

    fn write(obj: Self::RustType, buf: &mut std::vec::Vec<u8>) {
        use uniffi::deps::bytes::BufMut;
        match obj {
            {%- for variant in e.variants() %}
            {{ e.name() }}::{{ variant.name() }} { {% for field in variant.fields() %}{{ field.name() }}, {%- endfor %} } => {
                buf.put_i32({{ loop.index }});
                {% for field in variant.fields() -%}
                {{ field.type_().borrow()|ffi_converter }}::write({{ field.name() }}, buf);
                {%- endfor %}
            },
            {%- endfor %}
        };
    }

    fn try_read(buf: &mut &[u8]) -> uniffi::deps::anyhow::Result<{{ e.name() }}> {
        use uniffi::deps::bytes::Buf;
        uniffi::check_remaining(buf, 4)?;
        Ok(match buf.get_i32() {
            {%- for variant in e.variants() %}
            {{ loop.index }} => {{ e.name() }}::{{ variant.name() }}{% if variant.has_fields() %} {
                {% for field in variant.fields() %}
                {{ field.name() }}: {{ field.type_().borrow()|ffi_converter }}::try_read(buf)?,
                {%- endfor %}
            }{% endif %},
            {%- endfor %}
            v => uniffi::deps::anyhow::bail!("Invalid {{ e.name() }} enum value: {}", v),
        })
    }
}
