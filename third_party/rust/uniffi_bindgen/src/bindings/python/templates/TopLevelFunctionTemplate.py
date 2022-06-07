{% import "macros.py" as py %}
{%- let func = self.inner() %}
{%- match func.return_type() -%}
{%- when Some with (return_type) %}

def {{ func.name()|fn_name }}({%- call py::arg_list_decl(func) -%}):
    {%- call py::coerce_args(func) %}
    return {{ return_type|lift_fn }}({% call py::to_ffi_call(func) %})

{% when None -%}

def {{ func.name()|fn_name }}({%- call py::arg_list_decl(func) -%}):
    {%- call py::coerce_args(func) %}
    {% call py::to_ffi_call(func) %}
{% endmatch %}
