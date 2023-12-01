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
#[::uniffi::export_for_udl]
pub trait r#{{ obj.name() }} {
    {%- for meth in obj.methods() %}
    fn {{ meth.name() }}(
        {% if meth.takes_self_by_arc()%}self: Arc<Self>{% else %}&self{% endif %},
        {%- for arg in meth.arguments() %}
        {{ arg.name() }}: {% if arg.by_ref() %}&{% endif %}{{ arg.as_type().borrow()|type_rs }},
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
{% when ObjectImpl::Struct %}
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
#[::uniffi::export_for_udl(constructor)]
impl {{ obj.rust_name() }} {
    pub fn r#{{ cons.name() }}(
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
    pub fn r#{{ meth.name() }}(
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

{% endmatch %}
