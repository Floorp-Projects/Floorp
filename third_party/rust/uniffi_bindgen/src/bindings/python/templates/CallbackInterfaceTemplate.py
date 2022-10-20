{%- let cbi = ci.get_callback_interface_definition(id).unwrap() %}
{%- let foreign_callback = format!("foreignCallback{}", canonical_type_name) %}

{% if self.include_once_check("CallbackInterfaceRuntime.py") %}{% include "CallbackInterfaceRuntime.py" %}{% endif %}

# Declaration and FfiConverters for {{ type_name }} Callback Interface

class {{ type_name }}:
    {% for meth in cbi.methods() -%}
    def {{ meth.name()|fn_name }}({% call py::arg_list_decl(meth) %}):
        raise NotImplementedError

    {% endfor %}

def py_{{ foreign_callback }}(handle, method, args, buf_ptr):
    {% for meth in cbi.methods() -%}
    {% let method_name = format!("invoke_{}", meth.name())|fn_name %}
    def {{ method_name }}(python_callback, args):
        {#- Unpacking args from the RustBuffer #}
        {%- if meth.arguments().len() != 0 -%}
        {#- Calling the concrete callback object #}
        with args.consumeWithStream() as buf:
            rval = python_callback.{{ meth.name()|fn_name }}(
                {% for arg in meth.arguments() -%}
                {{ arg|read_fn }}(buf)
                {%- if !loop.last %}, {% endif %}
                {% endfor -%}
            )
        {% else %}
        rval = python_callback.{{ meth.name()|fn_name }}()
        {% endif -%}

        {#- Packing up the return value into a RustBuffer #}
        {%- match meth.return_type() -%}
        {%- when Some with (return_type) -%}
        with RustBuffer.allocWithBuilder() as builder:
            {{ return_type|write_fn }}(rval, builder)
            return builder.finalize()
        {%- else -%}
        return RustBuffer.alloc(0)
        {% endmatch -%}
    {% endfor %}

    cb = {{ ffi_converter_name }}.lift(handle)
    if not cb:
        raise InternalError("No callback in handlemap; this is a Uniffi bug")

    if method == IDX_CALLBACK_FREE:
        {{ ffi_converter_name }}.drop(handle)
        # No return value.
        # See docs of ForeignCallback in `uniffi/src/ffi/foreigncallbacks.rs`
        return 0

    {% for meth in cbi.methods() -%}
    {% let method_name = format!("invoke_{}", meth.name())|fn_name -%}
    if method == {{ loop.index }}:
        # Call the method and handle any errors
        # See docs of ForeignCallback in `uniffi/src/ffi/foreigncallbacks.rs` for details
        try:
            {%- match meth.throws_type() %}
            {%- when Some(err) %}
            try:
                # Sucessful return
                buf_ptr[0] = {{ method_name }}(cb, args)
                return 1
            except {{ err|type_name }} as e:
                # Catch errors declared in the UDL file
                with RustBuffer.allocWithBuilder() as builder:
                    {{ err|write_fn }}(e, builder)
                    buf_ptr[0] = builder.finalize()
                return -2
            {%- else %}
            # Sucessful return
            buf_ptr[0] = {{ method_name }}(cb, args)
            return 1
            {%- endmatch %}
        except BaseException as e:
            # Catch unexpected errors
            try:
                # Try to serialize the exception into a String
                buf_ptr[0] = {{ Type::String.borrow()|lower_fn }}(repr(e))
            except:
                # If that fails, just give up
                pass
            return -1
    {% endfor %}

    # This should never happen, because an out of bounds method index won't
    # ever be used. Once we can catch errors, we should return an InternalException.
    # https://github.com/mozilla/uniffi-rs/issues/351

    # An unexpected error happened.
    # See docs of ForeignCallback in `uniffi/src/ffi/foreigncallbacks.rs`
    return -1

# We need to keep this function reference alive:
# if they get GC'd while in use then UniFFI internals could attempt to call a function
# that is in freed memory.
# That would be...uh...bad. Yeah, that's the word. Bad.
{{ foreign_callback }} = FOREIGN_CALLBACK_T(py_{{ foreign_callback }})

# The FfiConverter which transforms the Callbacks in to Handles to pass to Rust.
rust_call(lambda err: _UniFFILib.{{ cbi.ffi_init_callback().name() }}({{ foreign_callback }}, err))
{{ ffi_converter_name }} = FfiConverterCallbackInterface({{ foreign_callback }})
