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
pub extern "C" fn {{ cbi.ffi_init_callback().name() }}(callback: uniffi::ForeignCallback) {
    {{ foreign_callback_internals }}.set_callback(callback);
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
    {%- match meth.return_type() %}
    {%- when Some with (return_type) %} -> {{ return_type.borrow()|type_rs }}
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

        let ret_rbuf = unsafe {
            // SAFETY:
            // * We're passing in a pointer to an empty buffer.
            //   * Nothing allocated, so nothing to drop.
            // * We expect the callback to write into that a valid allocated instance of a
            //   RustBuffer.
            // * A positive return value signals success.
            let mut ret_rbuf = uniffi::RustBuffer::new();
            let ret = callback(self.handle, {{ loop.index }}, args_rbuf, &mut ret_rbuf);
            match ret {
                0 => uniffi::RustBuffer::new(),
                _ if ret < 0 => panic!("Callback failed"),
                _ => ret_rbuf
            }
        };

    {#- Unpacking the RustBuffer to return to Rust #}
        {% match meth.return_type() -%}
        {% when Some with (return_type) -%}
        let vec = ret_rbuf.destroy_into_vec();
        let mut ret_buf = vec.as_slice();
        {{ return_type|ffi_converter }}::try_read(&mut ret_buf).unwrap()
        {%- else -%}
        uniffi::RustBuffer::destroy(ret_rbuf);
        {%- endmatch %}
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
