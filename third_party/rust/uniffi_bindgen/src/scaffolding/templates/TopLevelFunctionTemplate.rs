{#
// Forward work to `uniffi_macros` This keeps macro-based and UDL-based generated code consistent.
#}
#[::uniffi::export_for_udl]
pub {% if func.is_async() %}async {% endif %}fn r#{{ func.name() }}(
    {%- for arg in func.arguments() %}
    r#{{ arg.name() }}: {% if arg.by_ref() %}&{% endif %}{{ arg.as_type().borrow()|type_rs }},
    {%- endfor %}
)
{%- match (func.return_type(), func.throws_type()) %}
{%- when (Some(return_type), None) %} -> {{ return_type|type_rs }}
{%- when (Some(return_type), Some(error_type)) %} -> ::std::result::Result::<{{ return_type|type_rs }}, {{ error_type|type_rs }}>
{%- when (None, Some(error_type)) %} -> ::std::result::Result::<(), {{ error_type|type_rs }}>
{%- when (None, None) %}
{%- endmatch %}
{
    unreachable!()
}
