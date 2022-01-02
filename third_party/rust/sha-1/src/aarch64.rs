// TODO: Import those from libc, see https://github.com/rust-lang/libc/pull/1638
const AT_HWCAP: u64 = 16;
const HWCAP_SHA1: u64 = 32;

#[inline(always)]
pub fn sha1_supported() -> bool {
    let hwcaps: u64 = unsafe { ::libc::getauxval(AT_HWCAP) };
    (hwcaps & HWCAP_SHA1) != 0
}
