# This is how we find and load the dynamic library provided by the component.
# For now we just look it up by name.
module UniFFILib
  extend FFI::Library

  {% if config.custom_cdylib_path() %}
  ffi_lib {{ config.cdylib_path() }}
  {% else %}
  ffi_lib '{{ config.cdylib_name() }}'
  {% endif %}

  {% for func in ci.iter_ffi_function_definitions() -%}
  attach_function :{{ func.name() }},
    {%- call rb::arg_list_ffi_decl(func) %},
    {% match func.return_type() %}{% when Some with (type_) %}{{ type_|type_ffi }}{% when None %}:void{% endmatch %}
  {% endfor %}
end
