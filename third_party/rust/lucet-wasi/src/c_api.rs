use crate::ctx::WasiCtxBuilder;
use lucet_runtime::{DlModule, Module, Region};
use lucet_runtime_internals::c_api::{lucet_dl_module, lucet_error, lucet_instance, lucet_region};
use lucet_runtime_internals::instance::instance_handle_to_raw;
use lucet_runtime_internals::{assert_nonnull, with_ffi_arcs};
use std::ffi::CStr;
use std::sync::Arc;

#[repr(C)]
pub struct lucet_wasi_ctx {
    _unused: [u8; 0],
}

#[no_mangle]
pub unsafe extern "C" fn lucet_wasi_ctx_create() -> *mut lucet_wasi_ctx {
    let b = WasiCtxBuilder::new();
    Box::into_raw(Box::new(b)) as _
}

#[no_mangle]
pub unsafe extern "C" fn lucet_wasi_ctx_args(
    wasi_ctx: *mut lucet_wasi_ctx,
    argc: usize,
    argv: *const *const libc::c_char,
) -> lucet_error {
    assert_nonnull!(wasi_ctx);
    let mut b = Box::from_raw(wasi_ctx as *mut WasiCtxBuilder);
    let args_raw = std::slice::from_raw_parts(argv, argc);
    // TODO: error handling
    let args = args_raw
        .into_iter()
        .map(|arg| CStr::from_ptr(*arg))
        .collect::<Vec<&CStr>>();
    *b = b.c_args(&args);
    Box::into_raw(b);
    lucet_error::Ok
}

#[no_mangle]
pub unsafe extern "C" fn lucet_wasi_ctx_inherit_env(wasi_ctx: *mut lucet_wasi_ctx) -> lucet_error {
    assert_nonnull!(wasi_ctx);
    let mut b = Box::from_raw(wasi_ctx as *mut WasiCtxBuilder);
    *b = b.inherit_env();
    Box::into_raw(b);
    lucet_error::Ok
}

#[no_mangle]
pub unsafe extern "C" fn lucet_wasi_ctx_inherit_stdio(
    wasi_ctx: *mut lucet_wasi_ctx,
) -> lucet_error {
    assert_nonnull!(wasi_ctx);
    let mut b = Box::from_raw(wasi_ctx as *mut WasiCtxBuilder);
    *b = b.inherit_stdio();
    Box::into_raw(b);
    lucet_error::Ok
}

#[no_mangle]
pub unsafe extern "C" fn lucet_wasi_ctx_destroy(wasi_ctx: *mut lucet_wasi_ctx) {
    Box::from_raw(wasi_ctx as *mut WasiCtxBuilder);
}

/// Create a Lucet instance with the given WASI context.
///
/// After this call, the `wasi_ctx` pointer is no longer valid.
#[no_mangle]
pub unsafe extern "C" fn lucet_region_new_instance_with_wasi_ctx(
    region: *const lucet_region,
    module: *const lucet_dl_module,
    wasi_ctx: *mut lucet_wasi_ctx,
    inst_out: *mut *mut lucet_instance,
) -> lucet_error {
    assert_nonnull!(wasi_ctx);
    assert_nonnull!(inst_out);
    with_ffi_arcs!([region: dyn Region, module: DlModule], {
        let wasi_ctx = *Box::from_raw(wasi_ctx as *mut WasiCtxBuilder);
        region
            .new_instance_builder(module.clone() as Arc<dyn Module>)
            .with_embed_ctx(wasi_ctx.build())
            .build()
            .map(|i| {
                inst_out.write(instance_handle_to_raw(i) as _);
                lucet_error::Ok
            })
            .unwrap_or_else(|e| e.into())
    })
}

/// Call this if you're having trouble with `__wasi_*` symbols not being exported.
///
/// This is pretty hackish; we will hopefully be able to avoid this altogether once [this
/// issue](https://github.com/rust-lang/rust/issues/58037) is addressed.
#[no_mangle]
#[doc(hidden)]
pub extern "C" fn lucet_wasi_internal_ensure_linked() {
    crate::hostcalls::ensure_linked();
}
