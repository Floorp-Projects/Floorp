{%- import "macros.swift" as swift %}
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
{%- include "BooleanHelper.swift" %}

{%- when Type::String %}
{%- include "StringHelper.swift" %}

{%- when Type::Bytes %}
{%- include "DataHelper.swift" %}

{%- when Type::Int8 %}
{%- include "Int8Helper.swift" %}

{%- when Type::Int16 %}
{%- include "Int16Helper.swift" %}

{%- when Type::Int32 %}
{%- include "Int32Helper.swift" %}

{%- when Type::Int64 %}
{%- include "Int64Helper.swift" %}

{%- when Type::UInt8 %}
{%- include "UInt8Helper.swift" %}

{%- when Type::UInt16 %}
{%- include "UInt16Helper.swift" %}

{%- when Type::UInt32 %}
{%- include "UInt32Helper.swift" %}

{%- when Type::UInt64 %}
{%- include "UInt64Helper.swift" %}

{%- when Type::Float32 %}
{%- include "Float32Helper.swift" %}

{%- when Type::Float64 %}
{%- include "Float64Helper.swift" %}

{%- when Type::Timestamp %}
{%- include "TimestampHelper.swift" %}

{%- when Type::Duration %}
{%- include "DurationHelper.swift" %}

{%- when Type::CallbackInterface(name) %}
{%- include "CallbackInterfaceTemplate.swift" %}

{%- when Type::ForeignExecutor %}
{%- include "ForeignExecutorTemplate.swift" %}

{%- when Type::Custom { name, builtin } %}
{%- include "CustomType.swift" %}

{%- when Type::Enum(name) %}
{%- let e = ci.get_enum_definition(name).unwrap() %}
{%- if ci.is_name_used_as_error(name) %}
{%- include "ErrorTemplate.swift" %}
{%- else %}
{%- include "EnumTemplate.swift" %}
{% endif %}

{%- when Type::Object{ name, imp } %}
{%- include "ObjectTemplate.swift" %}

{%- when Type::Record(name) %}
{%- include "RecordTemplate.swift" %}

{%- when Type::Optional(inner_type) %}
{%- include "OptionalTemplate.swift" %}

{%- when Type::Sequence(inner_type) %}
{%- include "SequenceTemplate.swift" %}

{%- when Type::Map(key_type, value_type) %}
{%- include "MapTemplate.swift" %}

{%- else %}
{%- endmatch %}
{%- endfor %}

{%- if ci.has_async_fns() %}
{%- include "AsyncTypes.swift" %}
{%- endif %}
