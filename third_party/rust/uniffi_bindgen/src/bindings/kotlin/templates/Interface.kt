{%- call kt::docstring_value(interface_docstring, 0) %}
public interface {{ interface_name }} {
    {% for meth in methods.iter() -%}
    {%- call kt::docstring(meth, 4) %}
    {% if meth.is_async() -%}suspend {% endif -%}
    fun {{ meth.name()|fn_name }}({% call kt::arg_list(meth, true) %})
    {%- match meth.return_type() -%}
    {%- when Some with (return_type) %}: {{ return_type|type_name(ci) -}}
    {%- else -%}
    {%- endmatch %}
    {% endfor %}
    companion object
}

