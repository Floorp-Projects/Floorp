use libc::{getauxval, AT_HWCAP, HWCAP_SHA2};

#[inline(always)]
pub fn sha2_supported() -> bool {
    let hwcaps: u64 = unsafe { getauxval(AT_HWCAP) };
    (hwcaps & HWCAP_SHA2) != 0
}
