use crate::types::{Error, LucetSandboxInstance};

use lucet_runtime::{DlModule, Limits, MmapRegion, Module, Region};
use lucet_runtime_internals::{module::ModuleInternal};

use lucet_wasi::WasiCtxBuilder;

use std::ffi::{c_void, CStr};
use std::os::raw::c_char;
use std::ptr;
use std::sync::Arc;

#[no_mangle]
pub extern "C" fn lucet_ensure_linked() {
    lucet_runtime::lucet_internal_ensure_linked();
    lucet_wasi::hostcalls::ensure_linked();
}

#[no_mangle]
pub extern "C" fn lucet_load_module(lucet_module_path: *const c_char) -> *mut c_void {
    let module_path;
    unsafe {
        module_path = CStr::from_ptr(lucet_module_path)
            .to_string_lossy()
            .into_owned();
    }

    let result = lucet_load_module_helper(&module_path);

    let r = match result {
        Ok(inst) => Box::into_raw(Box::new(inst)) as *mut c_void,
        Err(e) => {
            println!("Error {:?}!", e);
            ptr::null_mut()
        }
    };

    return r;
}

#[no_mangle]
pub extern "C" fn lucet_drop_module(inst_ptr: *mut c_void) {
    // Need to invoke the destructor
    let _inst = unsafe {
        Box::from_raw(inst_ptr as *mut LucetSandboxInstance)
    };

}

fn lucet_load_module_helper(module_path: &String) -> Result<LucetSandboxInstance, Error> {
    let module = DlModule::load(module_path)?;

    //Replicating calculations used in lucet examples
    let min_globals_size = module.globals().len() * std::mem::size_of::<u64>();
    // Nearest 4k
    let globals_size = ((min_globals_size + 4096 - 1) / 4096) * 4096;

    let region = MmapRegion::create_aligned(
        1,
        &Limits {
            heap_memory_size: 4 * 1024 * 1024 * 1024,        // 4GB
            heap_address_space_size: 8 * 1024 * 1024 * 1024, // 8GB
            stack_size: 8 * 1024 * 1024,                     // 8MB - pthread default
            globals_size: globals_size,
        },
        4 * 1024 * 1024 * 1024,                              // 4GB heap alignment
    )?;

    let sig = module.get_signatures().to_vec();

    // put the path to the module on the front for argv[0]
    let ctx = WasiCtxBuilder::new()
        .args(&[&module_path])
        .inherit_stdio()
        .build()?;

    let instance_handle = region
        .new_instance_builder(module as Arc<dyn Module>)
        .with_embed_ctx(ctx)
        .build()?;

    let opaque_instance = LucetSandboxInstance {
        region: region,
        instance_handle: instance_handle,
        signatures: sig,
    };

    return Ok(opaque_instance);
}
