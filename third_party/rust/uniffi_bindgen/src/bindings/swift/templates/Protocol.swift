{%- call swift::docstring_value(protocol_docstring, 0) %}
public protocol {{ protocol_name }} : AnyObject {
    {% for meth in methods.iter() -%}
    {%- call swift::docstring(meth, 4) %}
    func {{ meth.name()|fn_name }}({% call swift::arg_list_protocol(meth) %}) {% call swift::async(meth) -%}{% call swift::throws(meth) -%}
    {%- match meth.return_type() -%}
    {%- when Some with (return_type) %} -> {{ return_type|type_name -}}
    {%- else -%}
    {%- endmatch %}
    {% endfor %}
}

