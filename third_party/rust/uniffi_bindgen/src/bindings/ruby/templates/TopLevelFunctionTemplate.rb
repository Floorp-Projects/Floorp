{%- match func.return_type() -%}
{%- when Some with (return_type) %}

def self.{{ func.name()|fn_name_rb }}({%- call rb::arg_list_decl(func) -%})
  {%- call rb::coerce_args(func) %}
  result = {% call rb::to_ffi_call(func) %}
  return {{ "result"|lift_rb(return_type) }}
end

{% when None %}

def self.{{ func.name()|fn_name_rb }}({%- call rb::arg_list_decl(func) -%})
  {%- call rb::coerce_args(func) %}
  {% call rb::to_ffi_call(func) %}
end
{% endmatch %}
