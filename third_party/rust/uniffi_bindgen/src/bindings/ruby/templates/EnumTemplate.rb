{% if e.is_flat() %}

class {{ e.name()|class_name_rb }}
  {% for variant in e.variants() -%}
  {{ variant.name()|enum_name_rb }} = {{ loop.index }}
  {% endfor %}
end

{% else %}

class {{ e.name()|class_name_rb }}
  def initialize
    raise RuntimeError, '{{ e.name()|class_name_rb }} cannot be instantiated directly'
  end

  # Each enum variant is a nested class of the enum itself.
  {% for variant in e.variants() -%}
  class {{ variant.name()|enum_name_rb }}
    {% if variant.has_fields() %}
    attr_reader {% for field in variant.fields() %}:{{ field.name()|var_name_rb }}{% if loop.last %}{% else %}, {% endif %}{%- endfor %}
    {% endif %}
    def initialize({% for field in variant.fields() %}{{ field.name()|var_name_rb }}{% if loop.last %}{% else %}, {% endif %}{% endfor %})
      {% if variant.has_fields() %}
      {%- for field in variant.fields() %}
      @{{ field.name()|var_name_rb }} = {{ field.name()|var_name_rb }}
      {%- endfor %}
      {% else %}
      {% endif %}
    end

    def to_s
      "{{ e.name()|class_name_rb }}::{{ variant.name()|enum_name_rb }}({% for field in variant.fields() %}{{ field.name() }}=#{@{{ field.name() }}}{% if loop.last %}{% else %}, {% endif %}{% endfor %})"
    end

    def ==(other)
      if !other.{{ variant.name()|var_name_rb }}?
        return false
      end
      {%- for field in variant.fields() %}
      if @{{ field.name()|var_name_rb }} != other.{{ field.name()|var_name_rb }}
        return false
      end
      {%- endfor %}

      true
    end

    # For each variant, we have an `NAME?` method for easily checking
    # whether an instance is that variant.
    {% for variant in e.variants() %}
    def {{ variant.name()|var_name_rb }}?
      instance_of? {{ e.name()|class_name_rb }}::{{ variant.name()|enum_name_rb }}
    end
    {% endfor %}
  end
  {% endfor %}
end

{% endif %}
