{%- let cbi = ci|get_callback_interface_definition(name) %}
{%- let foreign_callback = format!("foreignCallback{}", canonical_type_name) %}
{%- if self.include_once_check("CallbackInterfaceRuntime.swift") %}{%- include "CallbackInterfaceRuntime.swift" %}{%- endif %}

// Declaration and FfiConverters for {{ type_name }} Callback Interface

public protocol {{ type_name }} : AnyObject {
    {% for meth in cbi.methods() -%}
    func {{ meth.name()|fn_name }}({% call swift::arg_list_protocol(meth) %}) {% call swift::throws(meth) -%}
    {%- match meth.return_type() -%}
    {%- when Some with (return_type) %} -> {{ return_type|type_name -}}
    {%- else -%}
    {%- endmatch %}
    {% endfor %}
}

// The ForeignCallback that is passed to Rust.
fileprivate let {{ foreign_callback }} : ForeignCallback =
    { (handle: UniFFICallbackHandle, method: Int32, argsData: UnsafePointer<UInt8>, argsLen: Int32, out_buf: UnsafeMutablePointer<RustBuffer>) -> Int32 in
    {% for meth in cbi.methods() -%}
    {%- let method_name = format!("invoke_{}", meth.name())|fn_name %}

    func {{ method_name }}(_ swiftCallbackInterface: {{ type_name }}, _ argsData: UnsafePointer<UInt8>, _ argsLen: Int32, _ out_buf: UnsafeMutablePointer<RustBuffer>) throws -> Int32 {
        {%- if meth.arguments().len() > 0 %}
        var reader = createReader(data: Data(bytes: argsData, count: Int(argsLen)))
        {%- endif %}

        {%- match meth.return_type() %}
        {%- when Some(return_type) %}
        func makeCall() throws -> Int32 {
            let result = {% if meth.throws() %} try{% endif %} swiftCallbackInterface.{{ meth.name()|fn_name }}(
                    {% for arg in meth.arguments() -%}
                    {% if !config.omit_argument_labels() %}{{ arg.name()|var_name }}: {% endif %} try {{ arg|read_fn }}(from: &reader)
                    {%- if !loop.last %}, {% endif %}
                    {% endfor -%}
                )
            var writer = [UInt8]()
            {{ return_type|write_fn }}(result, into: &writer)
            out_buf.pointee = RustBuffer(bytes: writer)
            return UNIFFI_CALLBACK_SUCCESS
        }
        {%- when None %}
        func makeCall() throws -> Int32 {
            try swiftCallbackInterface.{{ meth.name()|fn_name }}(
                    {% for arg in meth.arguments() -%}
                    {% if !config.omit_argument_labels() %}{{ arg.name()|var_name }}: {% endif %} try {{ arg|read_fn }}(from: &reader)
                    {%- if !loop.last %}, {% endif %}
                    {% endfor -%}
                )
            return UNIFFI_CALLBACK_SUCCESS
        }
        {%- endmatch %}

        {%- match meth.throws_type() %}
        {%- when None %}
        return try makeCall()
        {%- when Some(error_type) %}
        do {
            return try makeCall()
        } catch let error as {{ error_type|type_name }} {
            out_buf.pointee = {{ error_type|lower_fn }}(error)
            return UNIFFI_CALLBACK_ERROR
        }
        {%- endmatch %}
    }
    {%- endfor %}


    switch method {
        case IDX_CALLBACK_FREE:
            {{ ffi_converter_name }}.drop(handle: handle)
            // Sucessful return
            // See docs of ForeignCallback in `uniffi_core/src/ffi/foreigncallbacks.rs`
            return UNIFFI_CALLBACK_SUCCESS
        {% for meth in cbi.methods() -%}
        {% let method_name = format!("invoke_{}", meth.name())|fn_name -%}
        case {{ loop.index }}:
            let cb: {{ cbi|type_name }}
            do {
                cb = try {{ ffi_converter_name }}.lift(handle)
            } catch {
                out_buf.pointee = {{ Type::String.borrow()|lower_fn }}("{{ cbi.name() }}: Invalid handle")
                return UNIFFI_CALLBACK_UNEXPECTED_ERROR
            }
            do {
                return try {{ method_name }}(cb, argsData, argsLen, out_buf)
            } catch let error {
                out_buf.pointee = {{ Type::String.borrow()|lower_fn }}(String(describing: error))
                return UNIFFI_CALLBACK_UNEXPECTED_ERROR
            }
        {% endfor %}
        // This should never happen, because an out of bounds method index won't
        // ever be used. Once we can catch errors, we should return an InternalError.
        // https://github.com/mozilla/uniffi-rs/issues/351
        default:
            // An unexpected error happened.
            // See docs of ForeignCallback in `uniffi_core/src/ffi/foreigncallbacks.rs`
            return UNIFFI_CALLBACK_UNEXPECTED_ERROR
    }
}

// FfiConverter protocol for callback interfaces
fileprivate struct {{ ffi_converter_name }} {
    private static let initCallbackOnce: () = {
        // Swift ensures this initializer code will once run once, even when accessed by multiple threads.
        try! rustCall { (err: UnsafeMutablePointer<RustCallStatus>) in
            {{ cbi.ffi_init_callback().name() }}({{ foreign_callback }}, err)
        }
    }()

    private static func ensureCallbackinitialized() {
        _ = initCallbackOnce
    }

    static func drop(handle: UniFFICallbackHandle) {
        handleMap.remove(handle: handle)
    }

    private static var handleMap = UniFFICallbackHandleMap<{{ type_name }}>()
}

extension {{ ffi_converter_name }} : FfiConverter {
    typealias SwiftType = {{ type_name }}
    // We can use Handle as the FfiType because it's a typealias to UInt64
    typealias FfiType = UniFFICallbackHandle

    public static func lift(_ handle: UniFFICallbackHandle) throws -> SwiftType {
        ensureCallbackinitialized();
        guard let callback = handleMap.get(handle: handle) else {
            throw UniffiInternalError.unexpectedStaleHandle
        }
        return callback
    }

    public static func read(from buf: inout (data: Data, offset: Data.Index)) throws -> SwiftType {
        ensureCallbackinitialized();
        let handle: UniFFICallbackHandle = try readInt(&buf)
        return try lift(handle)
    }

    public static func lower(_ v: SwiftType) -> UniFFICallbackHandle {
        ensureCallbackinitialized();
        return handleMap.insert(obj: v)
    }

    public static func write(_ v: SwiftType, into buf: inout [UInt8]) {
        ensureCallbackinitialized();
        writeInt(&buf, lower(v))
    }
}
