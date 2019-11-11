use crate::types::{LucetFunctionSignature, LucetSandboxInstance, SizedBuffer};

use lucet_module::Signature;
use lucet_runtime_internals::instance::InstanceInternal;

use std::convert::TryFrom;
use std::ffi::c_void;

#[no_mangle]
pub extern "C" fn lucet_get_reserved_callback_slot_val(
    inst_ptr: *mut c_void,
    slot_number: u32,
) -> usize {
    let inst = unsafe { &mut *(inst_ptr as *mut LucetSandboxInstance) };

    let name = format!("sandboxReservedCallbackSlot{}", slot_number);
    let func = inst
        .instance_handle
        .module()
        .get_export_func(&name)
        .unwrap();
    return func.ptr.as_usize();
}

#[no_mangle]
pub extern "C" fn lucet_get_function_pointer_table(inst_ptr: *mut c_void) -> SizedBuffer {
    let inst = unsafe { &mut *(inst_ptr as *mut LucetSandboxInstance) };

    let elems = inst.instance_handle.module().table_elements().unwrap();

    SizedBuffer {
        data: elems.as_ptr() as *mut c_void,
        length: elems.len(),
    }
}

#[no_mangle]
pub extern "C" fn lucet_get_function_type_index(
    inst_ptr: *mut c_void,
    csig: LucetFunctionSignature,
) -> i32 {
    let inst = unsafe { &mut *(inst_ptr as *mut LucetSandboxInstance) };

    let conv_sig: Signature = csig.into();
    let index = inst.signatures.iter().position(|r| *r == conv_sig);

    match index {
        Some(x) => i32::try_from(x).unwrap_or(-1),
        _ => -1,
    }
}
