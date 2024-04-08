{#
// Forward work to `uniffi_macros` This keeps macro-based and UDL-based generated code consistent.
#}

#[::uniffi::derive_enum_for_udl(
    {%- if e.is_non_exhaustive() -%}
    non_exhaustive,
    {%- endif %}
)]
enum r#{{ e.name() }} {
    {%- for variant in e.variants() %}
    r#{{ variant.name() }} {
        {%- for field in variant.fields() %}
        r#{{ field.name() }}: {{ field.as_type().borrow()|type_rs }},
        {%- endfor %}
    },
    {%- endfor %}
}
