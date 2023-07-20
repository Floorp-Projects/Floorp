{%- import "macros.kt" as kt %}

{%- for type_ in ci.iter_types() %}
{%- let type_name = type_|type_name %}
{%- let ffi_converter_name = type_|ffi_converter_name %}
{%- let canonical_type_name = type_|canonical_name %}
{%- let contains_object_references = ci.item_contains_object_references(type_) %}

{#
 # Map `Type` instances to an include statement for that type.
 #
 # There is a companion match in `KotlinCodeOracle::create_code_type()` which performs a similar function for the
 # Rust code.
 #
 #   - When adding additional types here, make sure to also add a match arm to that function.
 #   - To keep things manageable, let's try to limit ourselves to these 2 mega-matches
 #}
{%- match type_ %}

{%- when Type::Boolean %}
{%- include "BooleanHelper.kt" %}

{%- when Type::Int8 %}
{%- include "Int8Helper.kt" %}

{%- when Type::Int16 %}
{%- include "Int16Helper.kt" %}

{%- when Type::Int32 %}
{%- include "Int32Helper.kt" %}

{%- when Type::Int64 %}
{%- include "Int64Helper.kt" %}

{%- when Type::UInt8 %}
{%- include "UInt8Helper.kt" %}

{%- when Type::UInt16 %}
{%- include "UInt16Helper.kt" %}

{%- when Type::UInt32 %}
{%- include "UInt32Helper.kt" %}

{%- when Type::UInt64 %}
{%- include "UInt64Helper.kt" %}

{%- when Type::Float32 %}
{%- include "Float32Helper.kt" %}

{%- when Type::Float64 %}
{%- include "Float64Helper.kt" %}

{%- when Type::String %}
{%- include "StringHelper.kt" %}

{%- when Type::Enum(name) %}
{% include "EnumTemplate.kt" %}

{%- when Type::Error(name) %}
{% include "ErrorTemplate.kt" %}

{%- when Type::Object(name) %}
{% include "ObjectTemplate.kt" %}

{%- when Type::Record(name) %}
{% include "RecordTemplate.kt" %}

{%- when Type::Optional(inner_type) %}
{% include "OptionalTemplate.kt" %}

{%- when Type::Sequence(inner_type) %}
{% include "SequenceTemplate.kt" %}

{%- when Type::Map(key_type, value_type) %}
{% include "MapTemplate.kt" %}

{%- when Type::CallbackInterface(name) %}
{% include "CallbackInterfaceTemplate.kt" %}

{%- when Type::Timestamp %}
{% include "TimestampHelper.kt" %}

{%- when Type::Duration %}
{% include "DurationHelper.kt" %}

{%- when Type::Custom { name, builtin } %}
{% include "CustomTypeTemplate.kt" %}

{%- when Type::External { crate_name, name } %}
{% include "ExternalTypeTemplate.kt" %}

{%- else %}
{%- endmatch %}
{%- endfor %}
