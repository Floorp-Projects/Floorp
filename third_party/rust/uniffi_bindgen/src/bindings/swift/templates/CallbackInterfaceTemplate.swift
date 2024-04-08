{%- let cbi = ci|get_callback_interface_definition(name) %}
{%- let callback_handler = format!("uniffiCallbackHandler{}", name) %}
{%- let callback_init = format!("uniffiCallbackInit{}", name) %}
{%- let methods = cbi.methods() %}
{%- let protocol_name = type_name.clone() %}
{%- let protocol_docstring = cbi.docstring() %}
{%- let vtable = cbi.vtable() %}
{%- let vtable_methods = cbi.vtable_methods() %}
{%- let ffi_init_callback = cbi.ffi_init_callback() %}

{% include "Protocol.swift" %}
{% include "CallbackInterfaceImpl.swift" %}

// FfiConverter protocol for callback interfaces
fileprivate struct {{ ffi_converter_name }} {
    fileprivate static var handleMap = UniffiHandleMap<{{ type_name }}>()
}

extension {{ ffi_converter_name }} : FfiConverter {
    typealias SwiftType = {{ type_name }}
    typealias FfiType = UInt64

    public static func lift(_ handle: UInt64) throws -> SwiftType {
        try handleMap.get(handle: handle)
    }

    public static func read(from buf: inout (data: Data, offset: Data.Index)) throws -> SwiftType {
        let handle: UInt64 = try readInt(&buf)
        return try lift(handle)
    }

    public static func lower(_ v: SwiftType) -> UInt64 {
        return handleMap.insert(obj: v)
    }

    public static func write(_ v: SwiftType, into buf: inout [UInt8]) {
        writeInt(&buf, lower(v))
    }
}
