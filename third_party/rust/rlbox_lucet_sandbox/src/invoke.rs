use crate::types::LucetSandboxInstance;

use lucet_runtime_internals::instance::InstanceInternal;

use std::ffi::{c_void, CStr};
use std::os::raw::c_char;

#[no_mangle]
pub extern "C" fn lucet_lookup_function(
    inst_ptr: *mut c_void,
    fn_name: *const c_char,
) -> *mut c_void {
    let inst = unsafe { &mut *(inst_ptr as *mut LucetSandboxInstance) };
    let name = unsafe { CStr::from_ptr(fn_name).to_string_lossy() };
    let func = inst
        .instance_handle
        .module()
        .get_export_func(&name)
        .unwrap();
    let ret = func.ptr.as_usize();
    return ret as *mut c_void;
}

#[no_mangle]
pub extern "C" fn lucet_set_curr_instance(inst_ptr: *mut c_void)
{
    let inst = unsafe { &mut *(inst_ptr as *mut LucetSandboxInstance) };
    inst.instance_handle.set_current_instance();
}
