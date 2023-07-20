{%- if !ci.callback_interface_definitions().is_empty() %}
{%- include "CallbackInterfaceRuntime.sys.mjs" %}

{% endif %}

{%- for type_ in ci.iter_types() %}
{%- let ffi_converter = type_.ffi_converter() %}
{%- match type_ %}

{%- when Type::Boolean %}
{%- include "Boolean.sys.mjs" %}

{%- when Type::UInt8 %}
{%- include "UInt8.sys.mjs" %}

{%- when Type::UInt16 %}
{%- include "UInt16.sys.mjs" %}

{%- when Type::UInt32 %}
{%- include "UInt32.sys.mjs" %}

{%- when Type::UInt64 %}
{%- include "UInt64.sys.mjs" %}

{%- when Type::Int8 %}
{%- include "Int8.sys.mjs" %}

{%- when Type::Int16 %}
{%- include "Int16.sys.mjs" %}

{%- when Type::Int32 %}
{%- include "Int32.sys.mjs" %}

{%- when Type::Int64 %}
{%- include "Int64.sys.mjs" %}

{%- when Type::Float32 %}
{%- include "Float32.sys.mjs" %}

{%- when Type::Float64 %}
{%- include "Float64.sys.mjs" %}

{%- when Type::Record with (name) %}
{%- include "Record.sys.mjs" %}

{%- when Type::Optional with (inner) %}
{%- include "Optional.sys.mjs" %}

{%- when Type::String %}
{%- include "String.sys.mjs" %}

{%- when Type::Sequence with (inner) %}
{%- include "Sequence.sys.mjs" %}

{%- when Type::Map with (key_type, value_type) %}
{%- include "Map.sys.mjs" %}

{%- when Type::Enum with (name) %}
{%- let e = ci.get_enum_definition(name).unwrap() %}
{# For enums, there are either an error *or* an enum, they can't be both. #}
{%- if ci.is_name_used_as_error(name) %}
{%- let error = e %}
{%- include "Error.sys.mjs" %}
{%- else %}
{%- let enum_ = e %}
{%- include "Enum.sys.mjs" %}
{% endif %}

{%- when Type::Object with { name, imp } %}
{%- include "Object.sys.mjs" %}

{%- when Type::Custom with { name, builtin } %}
{%- include "CustomType.sys.mjs" %}

{%- when Type::External with { name, crate_name, kind } %}
{%- include "ExternalType.sys.mjs" %}

{%- when Type::CallbackInterface with (name) %}
{%- include "CallbackInterface.sys.mjs" %}

{%- else %}
{#- TODO implement the other types #}

{%- endmatch %}

{% endfor %}

{%- if !ci.callback_interface_definitions().is_empty() %}
// Define callback interface handlers, this must come after the type loop since they reference the FfiConverters defined above.

{% for cbi in ci.callback_interface_definitions() %}
{%- include "CallbackInterfaceHandler.sys.mjs" %}
{% endfor %}
{% endif %}
