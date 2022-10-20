{#
// For each Callback Interface definition, we assume that there is a corresponding trait defined in Rust client code.
// If the UDL callback interface and Rust trait's methods don't match, the Rust compiler will complain.
// We generate:
//  * an init function to accept that `ForeignCallback` from the foreign language, and stores it.
//  * a holder for a `ForeignCallback`, of type `uniffi::ForeignCallbackInternals`.
//  * a proxy `struct` which implements the `trait` that the Callback Interface corresponds to. This
//    is the object that client code interacts with.
//    - for each method, arguments will be packed into a `RustBuffer` and sent over the `ForeignCallback` to be
//      unpacked and called. The return value is packed into another `RustBuffer` and sent back to Rust.
//    - a `Drop` `impl`, which tells the foreign language to forget about the real callback object.
#}
{% let trait_name = cbi.name() -%}
{% let trait_impl = cbi.type_().borrow()|ffi_converter_name -%}
{% let foreign_callback_internals = format!("foreign_callback_{}_internals", trait_name)|upper -%}

// Register a foreign callback for getting across the FFI.
#[doc(hidden)]
static {{ foreign_callback_internals }}: uniffi::ForeignCallbackInternals = uniffi::ForeignCallbackInternals::new();

#[doc(hidden)]
#[no_mangle]
pub extern "C" fn {{ cbi.ffi_init_callback().name() }}(callback: uniffi::ForeignCallback, _: &mut uniffi::RustCallStatus) {
    {{ foreign_callback_internals }}.set_callback(callback);
    // The call status should be initialized to CALL_SUCCESS, so no need to modify it.
}

// Make an implementation which will shell out to the foreign language.
#[doc(hidden)]
#[derive(Debug)]
struct {{ trait_impl }} {
  handle: u64
}

impl Drop for {{ trait_impl }} {
    fn drop(&mut self) {
        let callback = {{ foreign_callback_internals }}.get_callback().unwrap();
        let mut rbuf = uniffi::RustBuffer::new();
        unsafe { callback(self.handle, uniffi::IDX_CALLBACK_FREE, Default::default(), &mut rbuf) };
    }
}

uniffi::deps::static_assertions::assert_impl_all!({{ trait_impl }}: Send);

impl r#{{ trait_name }} for {{ trait_impl }} {
    {%- for meth in cbi.methods() %}

    {#- Method declaration #}
    fn r#{{ meth.name() -}}
    ({% call rs::arg_list_decl_with_prefix("&self", meth) %})
    {%- match (meth.return_type(), meth.throws_type()) %}
    {%- when (Some(return_type), None) %} -> {{ return_type.borrow()|type_rs }}
    {%- when (Some(return_type), Some(err)) %} -> ::std::result::Result<{{ return_type.borrow()|type_rs }}, {{ err|type_rs }}>
    {%- when (None, Some(err)) %} -> ::std::result::Result<(), {{ err|type_rs }}>
    {% else -%}
    {%- endmatch -%} {
    {#- Method body #}
        uniffi::deps::log::debug!("{{ cbi.name() }}.{{ meth.name() }}");

    {#- Packing args into a RustBuffer #}
        {% if meth.arguments().len() == 0 -%}
        let args_buf = Vec::new();
        {% else -%}
        let mut args_buf = Vec::new();
        {% endif -%}
        {%- for arg in meth.arguments() %}
        {{ arg.type_().borrow()|ffi_converter }}::write(r#{{ arg.name() }}, &mut args_buf);
        {%- endfor -%}
        let args_rbuf = uniffi::RustBuffer::from_vec(args_buf);

    {#- Calling into foreign code. #}
        let callback = {{ foreign_callback_internals }}.get_callback().unwrap();

        unsafe {
            // SAFETY:
            // * We're passing in a pointer to an empty buffer.
            //   * Nothing allocated, so nothing to drop.
            // * We expect the callback to write into that a valid allocated instance of a
            //   RustBuffer.
            let mut ret_rbuf = uniffi::RustBuffer::new();
            let ret = callback(self.handle, {{ loop.index }}, args_rbuf, &mut ret_rbuf);
            #[allow(clippy::let_and_return, clippy::let_unit_value)]
            match ret {
                1 => {
                    // 1 indicates success with the return value written to the RustBuffer for
                    //   non-void calls.
                    let result = {
                        {% match meth.return_type() -%}
                        {%- when Some(return_type) -%}
                        let vec = ret_rbuf.destroy_into_vec();
                        let mut ret_buf = vec.as_slice();
                        {{ return_type|ffi_converter }}::try_read(&mut ret_buf).unwrap()
                        {%- else %}
                        uniffi::RustBuffer::destroy(ret_rbuf);
                        {%- endmatch %}
                    };
                    {%- if meth.throws() %}
                    Ok(result)
                    {%- else %}
                    result
                    {%- endif %}
                }
                -2 => {
                    // -2 indicates an error written to the RustBuffer
                    {% match meth.throws_type() -%}
                    {% when Some(error_type) -%}
                    let vec = ret_rbuf.destroy_into_vec();
                    let mut ret_buf = vec.as_slice();
                    Err({{ error_type|ffi_converter }}::try_read(&mut ret_buf).unwrap())
                    {%- else -%}
                    panic!("Callback return -2, but throws_type() is None");
                    {%- endmatch %}
                }
                // 0 is a deprecated method to indicates success for void returns
                0 => {
                    eprintln!("UniFFI: Callback interface returned 0.  Please update the bindings code to return 1 for all successfull calls");
                    {% match (meth.return_type(), meth.throws()) %}
                    {% when (Some(_), _) %}
                    panic!("Callback returned 0 when we were expecting a return value");
                    {% when (None, false) %}
                    {% when (None, true) %}
                    Ok(())
                    {%- endmatch %}
                }
                // -1 indicates an unexpected error
                {% match meth.throws_type() %}
                {%- when Some(error_type) -%}
                -1 => {
                    let reason = if !ret_rbuf.is_empty() {
                        match {{ Type::String.borrow()|ffi_converter }}::try_lift(ret_rbuf) {
                            Ok(s) => s,
                            Err(e) => {
                                println!("{{ trait_name }} Error reading ret_buf: {e}");
                                String::from("[Error reading reason]")
                            }
                        }
                    } else {
                        uniffi::RustBuffer::destroy(ret_rbuf);
                        String::from("[Unknown Reason]")
                    };
                    let e: {{ error_type|type_rs }} = uniffi::UnexpectedUniFFICallbackError::from_reason(reason).into();
                    Err(e)
                }
                {%- else %}
                -1 => panic!("Callback failed"),
                {%- endmatch %}
                // Other values should never be returned
                _ => panic!("Callback failed with unexpected return code"),
            }
        }
    }
    {%- endfor %}
}

unsafe impl uniffi::FfiConverter for {{ trait_impl }} {
    // This RustType allows for rust code that inputs this type as a Box<dyn CallbackInterfaceTrait> param
    type RustType = Box<dyn r#{{ trait_name }}>;
    type FfiType = u64;

    // Lower and write are tricky to implement because we have a dyn trait as our type.  There's
    // probably a way to, but this carries lots of thread safety risks, down to impedence
    // mismatches between Rust and foreign languages, and our uncertainty around implementations of
    // concurrent handlemaps.
    //
    // The use case for them is also quite exotic: it's passing a foreign callback back to the foreign
    // language.
    //
    // Until we have some certainty, and use cases, we shouldn't use them.
    fn lower(_obj: Self::RustType) -> Self::FfiType {
        panic!("Lowering CallbackInterface not supported")
    }

    fn write(_obj: Self::RustType, _buf: &mut std::vec::Vec<u8>) {
        panic!("Writing CallbackInterface not supported")
    }

    fn try_lift(v: Self::FfiType) -> uniffi::deps::anyhow::Result<Self::RustType> {
        Ok(Box::new(Self { handle: v }))
    }

    fn try_read(buf: &mut &[u8]) -> uniffi::deps::anyhow::Result<Self::RustType> {
        use uniffi::deps::bytes::Buf;
        uniffi::check_remaining(buf, 8)?;
        <Self as uniffi::FfiConverter>::try_lift(buf.get_u64())
    }
}
