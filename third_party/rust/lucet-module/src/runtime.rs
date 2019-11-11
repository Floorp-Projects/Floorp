/// This struct describes the handful of fields that Lucet-compiled programs may directly interact with, but
/// are provided through VMContext.
#[repr(C)]
#[repr(align(8))]
pub struct InstanceRuntimeData {
    pub globals_ptr: *mut i64,
    pub instruction_count: u64,
}
