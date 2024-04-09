{#
// Forward work to `uniffi_macros` This keeps macro-based and UDL-based generated code consistent.
#}

#[::uniffi::derive_error_for_udl(
    {% if e.is_flat() -%}
    flat_error,
    {% if ci.should_generate_error_read(e) -%}
    with_try_read,
    {%- endif %}
    {%- endif %}
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
