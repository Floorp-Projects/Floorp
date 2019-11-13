#![allow(dead_code)]
use vk;
use std::os::raw::c_void;
use std::ptr;
pub trait VkAllocation {
    unsafe extern "system" fn allocation(
        *mut (),
        usize,
        usize,
        vk::SystemAllocationScope,
    ) -> *mut ();
    unsafe extern "system" fn reallocation(
        *mut c_void,
        *mut c_void,
        usize,
        usize,
        vk::SystemAllocationScope,
    ) -> *mut c_void;
    unsafe extern "system" fn free(*mut c_void, *mut c_void);
    unsafe extern "system" fn internal_allocation(
        *mut c_void,
        usize,
        vk::InternalAllocationType,
        vk::SystemAllocationScope,
    );
    unsafe extern "system" fn internal_free(
        *mut c_void,
        usize,
        vk::InternalAllocationType,
        vk::SystemAllocationScope,
    );
    fn create_allocation_callback() -> Option<vk::AllocationCallbacks> {
        let alloc = vk::AllocationCallbacks {
            p_user_data: ptr::null_mut(),
            pfn_allocation: Self::allocation,
            pfn_reallocation: Self::reallocation,
            pfn_free: Self::free,
            pfn_internal_allocation: Self::internal_allocation,
            pfn_internal_free: Self::internal_free,
        };
        Some(alloc)
    }
}

pub struct DefaultAllocatorCallback;
pub struct TestAlloc;

impl VkAllocation for TestAlloc {
    unsafe extern "system" fn allocation(
        _: *mut (),
        _: usize,
        _: usize,
        _: vk::SystemAllocationScope,
    ) -> *mut () {
        ptr::null_mut()
    }

    unsafe extern "system" fn reallocation(
        _: *mut c_void,
        _: *mut c_void,
        _: usize,
        _: usize,
        _: vk::SystemAllocationScope,
    ) -> *mut c_void {
        ptr::null_mut()
    }
    unsafe extern "system" fn free(_: *mut c_void, _: *mut c_void) {}
    unsafe extern "system" fn internal_allocation(
        _: *mut c_void,
        _: usize,
        _: vk::InternalAllocationType,
        _: vk::SystemAllocationScope,
    ) {
    }
    unsafe extern "system" fn internal_free(
        _: *mut c_void,
        _: usize,
        _: vk::InternalAllocationType,
        _: vk::SystemAllocationScope,
    ) {
    }
}
impl VkAllocation for DefaultAllocatorCallback {
    unsafe extern "system" fn allocation(
        _: *mut (),
        _: usize,
        _: usize,
        _: vk::SystemAllocationScope,
    ) -> *mut () {
        ptr::null_mut()
    }

    unsafe extern "system" fn reallocation(
        _: *mut c_void,
        _: *mut c_void,
        _: usize,
        _: usize,
        _: vk::SystemAllocationScope,
    ) -> *mut c_void {
        ptr::null_mut()
    }
    unsafe extern "system" fn free(_: *mut c_void, _: *mut c_void) {}
    unsafe extern "system" fn internal_allocation(
        _: *mut c_void,
        _: usize,
        _: vk::InternalAllocationType,
        _: vk::SystemAllocationScope,
    ) {
    }
    unsafe extern "system" fn internal_free(
        _: *mut c_void,
        _: usize,
        _: vk::InternalAllocationType,
        _: vk::SystemAllocationScope,
    ) {
    }
    fn create_allocation_callback() -> Option<vk::AllocationCallbacks> {
        None
    }
}