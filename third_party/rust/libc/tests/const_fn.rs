#![cfg(libc_const_extern_fn)] // If this does not hold, the file is empty

#[cfg(target_os = "linux")]
const _FOO: libc::c_uint = unsafe { libc::CMSG_SPACE(1) };
//^ if CMSG_SPACE is not const, this will fail to compile
