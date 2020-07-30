use crate::{DlModule, Instance, Limits, MmapRegion, Module, Region};
use libc::{c_char, c_int, c_void};
use lucet_module::TrapCode;
use lucet_runtime_internals::c_api::*;
use lucet_runtime_internals::instance::{
    instance_handle_from_raw, instance_handle_to_raw, InstanceInternal,
};
use lucet_runtime_internals::vmctx::VmctxInternal;
use lucet_runtime_internals::WASM_PAGE_SIZE;
use lucet_runtime_internals::{
    assert_nonnull, lucet_hostcall_terminate, lucet_hostcalls, with_ffi_arcs,
};
use num_traits::FromPrimitive;
use std::ffi::CStr;
use std::ptr;
use std::sync::{Arc, Once};

macro_rules! with_instance_ptr {
    ( $name:ident, $body:block ) => {{
        assert_nonnull!($name);
        let $name: &mut Instance = &mut *($name as *mut Instance);
        $body
    }};
}

macro_rules! with_instance_ptr_unchecked {
    ( $name:ident, $body:block ) => {{
        let $name: &mut Instance = &mut *($name as *mut Instance);
        $body
    }};
}

#[no_mangle]
pub extern "C" fn lucet_error_name(e: c_int) -> *const c_char {
    if let Some(e) = lucet_error::from_i32(e) {
        use self::lucet_error::*;
        match e {
            Ok => "lucet_error_ok\0".as_ptr() as _,
            InvalidArgument => "lucet_error_invalid_argument\0".as_ptr() as _,
            RegionFull => "lucet_error_region_full\0".as_ptr() as _,
            Module => "lucet_error_module\0".as_ptr() as _,
            LimitsExceeded => "lucet_error_limits_exceeded\0".as_ptr() as _,
            NoLinearMemory => "lucet_error_no_linear_memory\0".as_ptr() as _,
            SymbolNotFound => "lucet_error_symbol_not_found\0".as_ptr() as _,
            FuncNotFound => "lucet_error_func_not_found\0".as_ptr() as _,
            RuntimeFault => "lucet_error_runtime_fault\0".as_ptr() as _,
            RuntimeTerminated => "lucet_error_runtime_terminated\0".as_ptr() as _,
            Dl => "lucet_error_dl\0".as_ptr() as _,
            InstanceNotReturned => "lucet_error_instance_not_returned\0".as_ptr() as _,
            InstanceNotYielded => "lucet_error_instance_not_yielded\0".as_ptr() as _,
            StartYielded => "lucet_error_start_yielded\0".as_ptr() as _,
            Internal => "lucet_error_internal\0".as_ptr() as _,
            Unsupported => "lucet_error_unsupported\0".as_ptr() as _,
        }
    } else {
        "!!! error: unknown lucet_error variant\0".as_ptr() as _
    }
}

#[no_mangle]
pub extern "C" fn lucet_result_tag_name(tag: libc::c_int) -> *const c_char {
    if let Some(tag) = lucet_result_tag::from_i32(tag) {
        use self::lucet_result_tag::*;
        match tag {
            Returned => "lucet_result_tag_returned\0".as_ptr() as _,
            Yielded => "lucet_result_tag_yielded\0".as_ptr() as _,
            Faulted => "lucet_result_tag_faulted\0".as_ptr() as _,
            Terminated => "lucet_result_tag_terminated\0".as_ptr() as _,
            Errored => "lucet_result_tag_errored\0".as_ptr() as _,
        }
    } else {
        "!!! unknown lucet_result_tag variant!\0".as_ptr() as _
    }
}

#[no_mangle]
pub unsafe extern "C" fn lucet_mmap_region_create(
    instance_capacity: u64,
    limits: *const lucet_alloc_limits,
    region_out: *mut *mut lucet_region,
) -> lucet_error {
    assert_nonnull!(region_out);
    let limits = limits
        .as_ref()
        .map(|l| l.into())
        .unwrap_or(Limits::default());
    match MmapRegion::create(instance_capacity as usize, &limits) {
        Ok(region) => {
            let region_thin = Arc::into_raw(Arc::new(region as Arc<dyn Region>));
            region_out.write(region_thin as _);
            return lucet_error::Ok;
        }
        Err(e) => return e.into(),
    }
}

#[no_mangle]
pub unsafe extern "C" fn lucet_region_release(region: *const lucet_region) {
    Arc::from_raw(region as *const Arc<dyn Region>);
}

// omg this naming convention might not scale
#[no_mangle]
pub unsafe extern "C" fn lucet_region_new_instance_with_ctx(
    region: *const lucet_region,
    module: *const lucet_dl_module,
    embed_ctx: *mut c_void,
    inst_out: *mut *mut lucet_instance,
) -> lucet_error {
    assert_nonnull!(inst_out);
    with_ffi_arcs!([region: dyn Region, module: DlModule], {
        region
            .new_instance_builder(module.clone() as Arc<dyn Module>)
            .with_embed_ctx(embed_ctx)
            .build()
            .map(|i| {
                inst_out.write(instance_handle_to_raw(i) as _);
                lucet_error::Ok
            })
            .unwrap_or_else(|e| e.into())
    })
}

#[no_mangle]
pub unsafe extern "C" fn lucet_region_new_instance(
    region: *const lucet_region,
    module: *const lucet_dl_module,
    inst_out: *mut *mut lucet_instance,
) -> lucet_error {
    lucet_region_new_instance_with_ctx(region, module, ptr::null_mut(), inst_out)
}

#[no_mangle]
pub unsafe extern "C" fn lucet_dl_module_load(
    path: *const c_char,
    mod_out: *mut *mut lucet_dl_module,
) -> lucet_error {
    assert_nonnull!(mod_out);
    let path = CStr::from_ptr(path);
    DlModule::load(path.to_string_lossy().into_owned())
        .map(|m| {
            mod_out.write(Arc::into_raw(m) as _);
            lucet_error::Ok
        })
        .unwrap_or_else(|e| e.into())
}

#[no_mangle]
pub unsafe extern "C" fn lucet_dl_module_release(module: *const lucet_dl_module) {
    Arc::from_raw(module as *const DlModule);
}

#[no_mangle]
pub unsafe extern "C" fn lucet_instance_run(
    inst: *mut lucet_instance,
    entrypoint: *const c_char,
    argc: usize,
    argv: *const lucet_val::lucet_val,
    result_out: *mut lucet_result::lucet_result,
) -> lucet_error {
    assert_nonnull!(entrypoint);
    if argc != 0 && argv.is_null() {
        return lucet_error::InvalidArgument;
    }
    let args = if argc == 0 {
        vec![]
    } else {
        std::slice::from_raw_parts(argv, argc)
            .into_iter()
            .map(|v| v.into())
            .collect()
    };
    let entrypoint = match CStr::from_ptr(entrypoint).to_str() {
        Ok(entrypoint_str) => entrypoint_str,
        Err(_) => {
            return lucet_error::SymbolNotFound;
        }
    };

    with_instance_ptr!(inst, {
        let res = inst.run(entrypoint, args.as_slice());
        let ret = res
            .as_ref()
            .map(|_| lucet_error::Ok)
            .unwrap_or_else(|e| e.into());
        if !result_out.is_null() {
            std::ptr::write(result_out, res.into());
        }
        ret
    })
}

#[no_mangle]
pub unsafe extern "C" fn lucet_instance_run_func_idx(
    inst: *mut lucet_instance,
    table_idx: u32,
    func_idx: u32,
    argc: usize,
    argv: *const lucet_val::lucet_val,
    result_out: *mut lucet_result::lucet_result,
) -> lucet_error {
    if argc != 0 && argv.is_null() {
        return lucet_error::InvalidArgument;
    }
    let args = if argc == 0 {
        vec![]
    } else {
        std::slice::from_raw_parts(argv, argc)
            .into_iter()
            .map(|v| v.into())
            .collect()
    };
    with_instance_ptr!(inst, {
        let res = inst.run_func_idx(table_idx, func_idx, args.as_slice());
        let ret = res
            .as_ref()
            .map(|_| lucet_error::Ok)
            .unwrap_or_else(|e| e.into());
        if !result_out.is_null() {
            std::ptr::write(result_out, res.into());
        }
        ret
    })
}

#[no_mangle]
pub unsafe extern "C" fn lucet_instance_resume(
    inst: *const lucet_instance,
    val: *mut c_void,
    result_out: *mut lucet_result::lucet_result,
) -> lucet_error {
    with_instance_ptr!(inst, {
        let res = inst.resume_with_val(CYieldedVal { val });
        let ret = res
            .as_ref()
            .map(|_| lucet_error::Ok)
            .unwrap_or_else(|e| e.into());
        if !result_out.is_null() {
            std::ptr::write(result_out, res.into());
        }
        ret
    })
}

#[no_mangle]
pub unsafe extern "C" fn lucet_instance_reset(inst: *mut lucet_instance) -> lucet_error {
    with_instance_ptr!(inst, {
        inst.reset()
            .map(|_| lucet_error::Ok)
            .unwrap_or_else(|e| e.into())
    })
}

#[no_mangle]
pub unsafe extern "C" fn lucet_instance_release(inst: *mut lucet_instance) {
    instance_handle_from_raw(inst as *mut Instance, true);
}

#[no_mangle]
pub unsafe extern "C" fn lucet_instance_heap(inst: *mut lucet_instance) -> *mut u8 {
    with_instance_ptr_unchecked!(inst, { inst.heap_mut().as_mut_ptr() })
}

#[no_mangle]
pub unsafe extern "C" fn lucet_instance_heap_len(inst: *const lucet_instance) -> u32 {
    with_instance_ptr_unchecked!(inst, { inst.heap().len() as u32 })
}

#[no_mangle]
pub unsafe extern "C" fn lucet_instance_check_heap(
    inst: *const lucet_instance,
    ptr: *const c_void,
    len: usize,
) -> bool {
    with_instance_ptr_unchecked!(inst, { inst.check_heap(ptr, len) })
}

#[no_mangle]
pub unsafe extern "C" fn lucet_instance_grow_heap(
    inst: *mut lucet_instance,
    additional_pages: u32,
    previous_pages_out: *mut u32,
) -> lucet_error {
    with_instance_ptr!(inst, {
        match inst.grow_memory(additional_pages) {
            Ok(previous_pages) => {
                if !previous_pages_out.is_null() {
                    previous_pages_out.write(previous_pages);
                }
                lucet_error::Ok
            }
            Err(e) => e.into(),
        }
    })
}

#[no_mangle]
pub unsafe extern "C" fn lucet_instance_embed_ctx(inst: *mut lucet_instance) -> *mut c_void {
    with_instance_ptr_unchecked!(inst, {
        inst.get_embed_ctx::<*mut c_void>()
            .map(|r| r.map(|p| *p).unwrap_or(ptr::null_mut()))
            .unwrap_or(ptr::null_mut())
    })
}

/// Release or run* must not be called in the body of this function!
#[no_mangle]
pub unsafe extern "C" fn lucet_instance_set_signal_handler(
    inst: *mut lucet_instance,
    signal_handler: lucet_signal_handler,
) -> lucet_error {
    let handler = move |inst: &Instance, trap: &Option<TrapCode>, signum, siginfo, context| {
        let inst = inst as *const Instance as *mut lucet_instance;
        signal_handler(inst, trap.into(), signum, siginfo, context).into()
    };
    with_instance_ptr!(inst, {
        inst.set_signal_handler(handler);
    });
    lucet_error::Ok
}

#[no_mangle]
pub unsafe extern "C" fn lucet_instance_set_fatal_handler(
    inst: *mut lucet_instance,
    fatal_handler: lucet_fatal_handler,
) -> lucet_error {
    // transmuting is fine here because *mut lucet_instance = *mut Instance
    let fatal_handler: unsafe extern "C" fn(inst: *mut Instance) =
        std::mem::transmute(fatal_handler);
    with_instance_ptr!(inst, {
        inst.set_c_fatal_handler(fatal_handler);
    });
    lucet_error::Ok
}

#[no_mangle]
pub unsafe extern "C" fn lucet_retval_gp(retval: *const lucet_untyped_retval) -> lucet_retval_gp {
    lucet_retval_gp {
        as_untyped: (*retval).gp,
    }
}

#[no_mangle]
pub unsafe extern "C" fn lucet_retval_f32(retval: *const lucet_untyped_retval) -> f32 {
    let mut v = 0.0f32;
    core::arch::x86_64::_mm_storeu_ps(
        &mut v as *mut f32,
        core::arch::x86_64::_mm_loadu_ps((*retval).fp.as_ptr() as *const f32),
    );
    v
}

#[no_mangle]
pub unsafe extern "C" fn lucet_retval_f64(retval: *const lucet_untyped_retval) -> f64 {
    let mut v = 0.0f64;
    core::arch::x86_64::_mm_storeu_pd(
        &mut v as *mut f64,
        core::arch::x86_64::_mm_loadu_pd((*retval).fp.as_ptr() as *const f64),
    );
    v
}

static C_API_INIT: Once = Once::new();

/// Should never actually be called, but should be reachable via a trait method to prevent DCE.
pub fn ensure_linked() {
    use std::ptr::read_volatile;
    C_API_INIT.call_once(|| unsafe {
        read_volatile(lucet_vmctx_get_heap as *const extern "C" fn());
        read_volatile(lucet_vmctx_current_memory as *const extern "C" fn());
        read_volatile(lucet_vmctx_grow_memory as *const extern "C" fn());
    });
}

lucet_hostcalls! {
    #[no_mangle]
    pub unsafe extern "C" fn lucet_vmctx_get_heap(
        &mut vmctx,
    ) -> *mut u8 {
        vmctx.instance().alloc().slot().heap as *mut u8
    }

    #[no_mangle]
    pub unsafe extern "C" fn lucet_vmctx_get_globals(
        &mut vmctx,
    ) -> *mut i64 {
        vmctx.instance().alloc().slot().globals as *mut i64
    }

    #[no_mangle]
    /// Get the number of WebAssembly pages currently in the heap.
    pub unsafe extern "C" fn lucet_vmctx_current_memory(
        &mut vmctx,
    ) -> u32 {
        vmctx.instance().alloc().heap_len() as u32 / WASM_PAGE_SIZE
    }

    #[no_mangle]
    /// Grows the guest heap by the given number of WebAssembly pages.
    ///
    /// On success, returns the number of pages that existed before the call. On failure, returns `-1`.
    pub unsafe extern "C" fn lucet_vmctx_grow_memory(
        &mut vmctx,
        additional_pages: u32,
    ) -> i32 {
        if let Ok(old_pages) = vmctx.instance_mut().grow_memory(additional_pages) {
            old_pages as i32
        } else {
            -1
        }
    }

    #[no_mangle]
    /// Check if a memory region is inside the instance heap.
    pub unsafe extern "C" fn lucet_vmctx_check_heap(
        &mut vmctx,
        ptr: *mut c_void,
        len: libc::size_t,
    ) -> bool {
        vmctx.instance().check_heap(ptr, len)
    }

    #[no_mangle]
    pub unsafe extern "C" fn lucet_vmctx_get_func_from_idx(
        &mut vmctx,
        table_idx: u32,
        func_idx: u32,
    ) -> *const c_void {
        vmctx.instance()
            .module()
            .get_func_from_idx(table_idx, func_idx)
            .map(|fptr| fptr.ptr.as_usize() as *const c_void)
            .unwrap_or(std::ptr::null())
    }

    #[no_mangle]
    pub unsafe extern "C" fn lucet_vmctx_terminate(
        &mut _vmctx,
        details: *mut c_void,
    ) -> () {
        lucet_hostcall_terminate!(CTerminationDetails { details });
    }

    #[no_mangle]
    /// Get the delegate object for the current instance.
    ///
    /// TODO: rename
    pub unsafe extern "C" fn lucet_vmctx_get_delegate(
        &mut vmctx,
    ) -> *mut c_void {
        vmctx.instance()
            .get_embed_ctx::<*mut c_void>()
            .map(|r| r.map(|p| *p).unwrap_or(ptr::null_mut()))
            .unwrap_or(std::ptr::null_mut())
    }

    #[no_mangle]
    pub unsafe extern "C" fn lucet_vmctx_yield(
        &mut vmctx,
        val: *mut c_void,
    ) -> *mut c_void {
        vmctx
            .yield_val_try_val(CYieldedVal { val })
            .map(|CYieldedVal { val }| val)
            .unwrap_or(std::ptr::null_mut())
    }
}

#[cfg(test)]
mod tests {
    use super::lucet_dl_module;
    use crate::DlModule;
    use lucet_module::bindings::Bindings;
    use lucet_wasi_sdk::{CompileOpts, LinkOpt, LinkOpts, Lucetc};
    use lucetc::LucetcOpts;
    use std::sync::Arc;
    use tempfile::TempDir;

    extern "C" {
        fn lucet_runtime_test_expand_heap(module: *mut lucet_dl_module) -> bool;
        fn lucet_runtime_test_yield_resume(module: *mut lucet_dl_module) -> bool;
    }

    #[test]
    fn expand_heap() {
        let workdir = TempDir::new().expect("create working directory");

        let native_build = Lucetc::new(&["tests/guests/null.c"])
            .with_cflag("-nostartfiles")
            .with_link_opt(LinkOpt::NoDefaultEntryPoint)
            .with_link_opt(LinkOpt::AllowUndefinedAll)
            .with_link_opt(LinkOpt::ExportAll);

        let so_file = workdir.path().join("null.so");

        native_build.build(so_file.clone()).unwrap();

        let dlmodule = DlModule::load(so_file).unwrap();

        unsafe {
            assert!(lucet_runtime_test_expand_heap(
                Arc::into_raw(dlmodule) as *mut lucet_dl_module
            ));
        }
    }

    #[test]
    fn yield_resume() {
        let workdir = TempDir::new().expect("create working directory");

        let native_build = Lucetc::new(&["tests/guests/yield_resume.c"])
            .with_cflag("-nostartfiles")
            .with_link_opt(LinkOpt::NoDefaultEntryPoint)
            .with_link_opt(LinkOpt::AllowUndefinedAll)
            .with_link_opt(LinkOpt::ExportAll)
            .with_bindings(Bindings::from_file("tests/guests/yield_resume_bindings.json").unwrap());

        let so_file = workdir.path().join("yield_resume.so");

        native_build.build(so_file.clone()).unwrap();

        let dlmodule = DlModule::load(so_file).unwrap();

        unsafe {
            assert!(lucet_runtime_test_yield_resume(
                Arc::into_raw(dlmodule) as *mut lucet_dl_module
            ));
        }
    }
}
