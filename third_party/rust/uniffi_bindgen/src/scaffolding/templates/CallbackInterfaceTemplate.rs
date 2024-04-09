#[::uniffi::export_for_udl(callback_interface)]
pub trait r#{{ cbi.name() }} {
    {%- for meth in cbi.methods() %}
    fn r#{{ meth.name() }}(
        {% if meth.takes_self_by_arc()%}self: Arc<Self>{% else %}&self{% endif %},
        {%- for arg in meth.arguments() %}
        r#{{ arg.name() }}: {% if arg.by_ref() %}&{% endif %}{{ arg.as_type().borrow()|type_rs }},
        {%- endfor %}
    )
    {%- match (meth.return_type(), meth.throws_type()) %}
    {%- when (Some(return_type), None) %} -> {{ return_type|type_rs }};
    {%- when (Some(return_type), Some(error_type)) %} -> ::std::result::Result::<{{ return_type|type_rs }}, {{ error_type|type_rs }}>;
    {%- when (None, Some(error_type)) %} -> ::std::result::Result::<(), {{ error_type|type_rs }}>;
    {%- when (None, None) %};
    {%- endmatch %}
    {% endfor %}
}
