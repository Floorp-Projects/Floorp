# Record type {{ rec.name() }}
class {{ rec.name()|class_name_rb }}
  attr_reader {% for field in rec.fields() %}:{{ field.name()|var_name_rb }}{% if loop.last %}{% else %}, {% endif %}{%- endfor %}

  def initialize({% for field in rec.fields() %}{{ field.name()|var_name_rb -}}:
        {%- match field.default_value() %}
        {%- when Some with(literal) %} {{ literal|literal_rb }}
        {%- else %}
        {%- endmatch %}
  {%- if loop.last %}{% else %}, {% endif -%}{% endfor %})
    {%- for field in rec.fields() %}
    @{{ field.name()|var_name_rb }} = {{ field.name()|var_name_rb }}
    {%- endfor %}
  end

  def ==(other)
    {%- for field in rec.fields() %}
    if @{{ field.name()|var_name_rb }} != other.{{ field.name()|var_name_rb }}
      return false
    end
    {%- endfor %}

    true
  end
end
