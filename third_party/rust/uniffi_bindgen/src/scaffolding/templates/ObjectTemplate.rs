{#
// Forward work to `uniffi_macros` This keeps macro-based and UDL-based generated code consistent.
#}

{%- if obj.is_trait_interface() %}
#[::uniffi::export_for_udl{% if obj.has_callback_interface() %}(with_foreign){% endif %}]
pub trait r#{{ obj.name() }} {
    {%- for meth in obj.methods() %}
    {% if meth.is_async() %}async {% endif %}fn r#{{ meth.name() }}(
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
{%- else %}
{%- for tm in obj.uniffi_traits() %}
{%      match tm %}
{%          when UniffiTrait::Debug { fmt }%}
#[uniffi::export(Debug)]
{%          when UniffiTrait::Display { fmt }%}
#[uniffi::export(Display)]
{%          when UniffiTrait::Hash { hash }%}
#[uniffi::export(Hash)]
{%          when UniffiTrait::Eq { eq, ne }%}
#[uniffi::export(Eq)]
{%      endmatch %}
{% endfor %}
#[::uniffi::derive_object_for_udl]
struct {{ obj.rust_name() }} { }

{%- for cons in obj.constructors() %}
#[::uniffi::export_for_udl]
impl {{ obj.rust_name() }} {
    #[uniffi::constructor]
    pub {% if cons.is_async() %}async {% endif %}fn r#{{ cons.name() }}(
        {%- for arg in cons.arguments() %}
        r#{{ arg.name() }}: {% if arg.by_ref() %}&{% endif %}{{ arg.as_type().borrow()|type_rs }},
        {%- endfor %}
    )
    {%- match (cons.return_type(), cons.throws_type()) %}
    {%- when (Some(return_type), None) %} -> {{ return_type|type_rs }}
    {%- when (Some(return_type), Some(error_type)) %} -> ::std::result::Result::<{{ return_type|type_rs }}, {{ error_type|type_rs }}>
    {%- when (None, Some(error_type)) %} -> ::std::result::Result::<(), {{ error_type|type_rs }}>
    {%- when (None, None) %}
    {%- endmatch %}
    {
        unreachable!()
    }
}
{%- endfor %}

{%- for meth in obj.methods() %}
#[::uniffi::export_for_udl]
impl {{ obj.rust_name() }} {
    pub {% if meth.is_async() %}async {% endif %}fn r#{{ meth.name() }}(
        {% if meth.takes_self_by_arc()%}self: Arc<Self>{% else %}&self{% endif %},
        {%- for arg in meth.arguments() %}
        r#{{ arg.name() }}: {% if arg.by_ref() %}&{% endif %}{{ arg.as_type().borrow()|type_rs }},
        {%- endfor %}
    )
    {%- match (meth.return_type(), meth.throws_type()) %}
    {%- when (Some(return_type), None) %} -> {{ return_type|type_rs }}
    {%- when (Some(return_type), Some(error_type)) %} -> ::std::result::Result::<{{ return_type|type_rs }}, {{ error_type|type_rs }}>
    {%- when (None, Some(error_type)) %} -> ::std::result::Result::<(), {{ error_type|type_rs }}>
    {%- when (None, None) %}
    {%- endmatch %}
    {
        unreachable!()
    }
}
{%- endfor %}

{% endif %}
