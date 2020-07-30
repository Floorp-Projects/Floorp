mod c_child;
mod rust_child;
use crate::context::{Context, ContextHandle, Error};
use memoffset::offset_of;
use std::slice;

#[test]
fn context_offsets_correct() {
    assert_eq!(offset_of!(Context, gpr), 0);
    assert_eq!(offset_of!(Context, fpr), 8 * 8);
    assert_eq!(offset_of!(Context, retvals_gp), 8 * 8 + 8 * 16);
    assert_eq!(offset_of!(Context, retval_fp), 8 * 8 + 8 * 16 + 8 * 2);
}

#[test]
fn init_rejects_unaligned() {
    extern "C" fn dummy() {}
    // first we have to specially craft an unaligned slice, since
    // a normal allocation of a [u64] often ends up 16-byte
    // aligned
    let mut len = 1024;
    let mut stack = vec![0u64; len];
    let ptr = stack.as_mut_ptr();
    let skew = ptr as usize % 16;

    // we happened to be aligned already, so let's mess it up
    if skew == 0 {
        len -= 1;
    }

    let mut stack_unaligned = unsafe { slice::from_raw_parts_mut(ptr, len) };

    // now we have the unaligned stack, let's make sure it blows up right
    let mut parent = ContextHandle::new();
    let res =
        ContextHandle::create_and_init(&mut stack_unaligned, &mut parent, dummy as usize, &[]);

    if let Err(Error::UnalignedStack) = res {
        assert!(true);
    } else {
        assert!(false, "init succeeded with unaligned stack");
    }
}
