{%- for type_ in ci.iter_types() %}
{%- let ffi_converter = type_.ffi_converter() %}
{%- match type_ %}

{%- when Type::Boolean %}
{%- include "Boolean.jsm" %}

{%- when Type::UInt8 %}
{%- include "UInt8.jsm" %}

{%- when Type::UInt16 %}
{%- include "UInt16.jsm" %}

{%- when Type::UInt32 %}
{%- include "UInt32.jsm" %}

{%- when Type::UInt64 %}
{%- include "UInt64.jsm" %}

{%- when Type::Int8 %}
{%- include "Int8.jsm" %}

{%- when Type::Int16 %}
{%- include "Int16.jsm" %}

{%- when Type::Int32 %}
{%- include "Int32.jsm" %}

{%- when Type::Int64 %}
{%- include "Int64.jsm" %}

{%- when Type::Float32 %}
{%- include "Float32.jsm" %}

{%- when Type::Float64 %}
{%- include "Float64.jsm" %}

{%- when Type::Record with (name) %}
{%- include "Record.jsm" %}

{%- when Type::Optional with (inner) %}
{%- include "Optional.jsm" %}

{%- when Type::String %}
{%- include "String.jsm" %}

{%- when Type::Sequence with (inner) %}
{%- include "Sequence.jsm" %}

{%- when Type::Map with (key_type, value_type) %}
{%- include "Map.jsm" %}

{%- when Type::Error with (name) %}
{% include "Error.jsm" %}

{% when Type::Enum with (name) %}
{% include "Enum.jsm" %}

{% when Type::Object with (name) %}
{% include "Object.jsm" %}

{%- when Type::Custom with { name, builtin } %}
{% include "CustomType.jsm" %}

{%- else %}
{# TODO implement the other types #}

{%- endmatch %}
{%- endfor %}
