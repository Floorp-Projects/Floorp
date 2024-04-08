{#
// For each record declared in the UDL, we assume the caller has provided a corresponding
// rust `struct` with the declared fields. We provide the traits for sending it across the FFI.
// If the caller's struct does not match the shape and types declared in the UDL then the rust
// compiler will complain with a type error.
//
// We define a unit-struct to implement the trait to sidestep Rust's orphan rule (ADR-0006). It's
// public so other crates can refer to it via an `[External='crate'] typedef`
#}

#[::uniffi::derive_record_for_udl]
struct r#{{ rec.name() }} {
    {%- for field in rec.fields() %}
    r#{{ field.name() }}: {{ field.as_type().borrow()|type_rs }},
    {%- endfor %}
}
