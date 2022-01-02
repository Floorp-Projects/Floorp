#![cfg(target_os = "android")]
//! Provides a wrapper around Android's ASharedMemory API.
//!
//! ashmem has existed in Android as a non-public API for some time.
//! Originally accessed via ioctl, it was later available via libcutils as ashmem_create_region etc.
//! ASharedMemory is the new public API, but it isn't available until API 26 (Android 8).
//! Builds targeting Android 10 (API 29) are no longer permitted to access ashmem via the ioctl interface.
//! This makes life for a portable program difficult - you can't reliably use the old or new interface during this transition period.
//! We try to dynamically load the new API first, then fall back to the ioctl interface.
//!
//! References:
//!   - [ASharedMemory documentation](https://developer.android.com/ndk/reference/group/memory)
//!   - [Linux ashmem.h definitions](https://elixir.bootlin.com/linux/v5.11.8/source/drivers/staging/android/uapi/ashmem.h)

#[macro_use]
extern crate ioctl_sys;

const __ASHMEMIOC: u32 = 0x77;

static mut LIBANDROID_ASHAREDMEMORY_CREATE: Option<
    extern "C" fn(*const libc::c_char, libc::size_t) -> libc::c_int,
> = None;
static mut LIBANDROID_ASHAREDMEMORY_GETSIZE: Option<extern "C" fn(libc::c_int) -> libc::size_t> =
    None;
static mut LIBANDROID_ASHAREDMEMORY_SETPROT: Option<
    extern "C" fn(libc::c_int, libc::c_int) -> libc::c_int,
> = None;

unsafe fn maybe_init() {
    const LIBANDROID_NAME: *const libc::c_char = "libandroid\0".as_ptr() as *const libc::c_char;
    const LIBANDROID_ASHAREDMEMORY_CREATE_NAME: *const libc::c_char =
        "ASharedMemory_create\0".as_ptr() as _;
    const LIBANDROID_ASHAREDMEMORY_GETSIZE_NAME: *const libc::c_char =
        "ASharedMemory_getSize\0".as_ptr() as _;
    const LIBANDROID_ASHAREDMEMORY_SETPROT_NAME: *const libc::c_char =
        "ASharedMemory_setProt\0".as_ptr() as _;
    static ONCE: std::sync::Once = std::sync::Once::new();
    ONCE.call_once(|| {
        // Leak the handle, there's no safe time to close it.
        let handle = libc::dlopen(LIBANDROID_NAME, libc::RTLD_LAZY | libc::RTLD_LOCAL);
        if handle.is_null() {
            return;
        }
        // Transmute guarantee for `fn -> Option<fn>`: https://doc.rust-lang.org/std/option/#representation
        LIBANDROID_ASHAREDMEMORY_CREATE =
            std::mem::transmute(libc::dlsym(handle, LIBANDROID_ASHAREDMEMORY_CREATE_NAME));
        LIBANDROID_ASHAREDMEMORY_GETSIZE =
            std::mem::transmute(libc::dlsym(handle, LIBANDROID_ASHAREDMEMORY_GETSIZE_NAME));
        LIBANDROID_ASHAREDMEMORY_SETPROT =
            std::mem::transmute(libc::dlsym(handle, LIBANDROID_ASHAREDMEMORY_SETPROT_NAME));
    });
}

/// See [ASharedMemory_create NDK documentation](https://developer.android.com/ndk/reference/group/memory#asharedmemory_create)
///
/// # Safety
///
/// Directly calls C or kernel APIs.
#[allow(non_snake_case)]
pub unsafe fn ASharedMemory_create(name: *const libc::c_char, size: libc::size_t) -> libc::c_int {
    const ASHMEM_NAME_DEF: *const libc::c_char = "/dev/ashmem".as_ptr() as _;
    const ASHMEM_NAME_LEN: usize = 256;
    const ASHMEM_SET_NAME: libc::c_int = iow!(
        __ASHMEMIOC,
        1,
        std::mem::size_of::<[libc::c_char; ASHMEM_NAME_LEN]>()
    ) as _;
    const ASHMEM_SET_SIZE: libc::c_int =
        iow!(__ASHMEMIOC, 3, std::mem::size_of::<libc::size_t>()) as _;

    maybe_init();
    if let Some(fun) = LIBANDROID_ASHAREDMEMORY_CREATE {
        return fun(name, size);
    }

    let fd = libc::open(ASHMEM_NAME_DEF, libc::O_RDWR, 0o600);
    if fd < 0 {
        return fd;
    }

    if !name.is_null() {
        // NOTE: libcutils uses a local stack copy of `name`.
        let r = libc::ioctl(fd, ASHMEM_SET_NAME, name);
        if r != 0 {
            libc::close(fd);
            return -1;
        }
    }

    let r = libc::ioctl(fd, ASHMEM_SET_SIZE, size);
    if r != 0 {
        libc::close(fd);
        return -1;
    }

    fd
}

/// See [ASharedMemory_getSize NDK documentation](https://developer.android.com/ndk/reference/group/memory#asharedmemory_getsize)
///
/// # Safety
///
/// Directly calls C or kernel APIs.
#[allow(non_snake_case)]
pub unsafe fn ASharedMemory_getSize(fd: libc::c_int) -> libc::size_t {
    const ASHMEM_GET_SIZE: libc::c_int = io!(__ASHMEMIOC, 4) as _;

    maybe_init();
    if let Some(fun) = LIBANDROID_ASHAREDMEMORY_GETSIZE {
        return fun(fd);
    }

    libc::ioctl(fd, ASHMEM_GET_SIZE) as libc::size_t
}

/// See [ASharedMemory_setProt NDK documentation](https://developer.android.com/ndk/reference/group/memory#asharedmemory_setprot)
///
/// # Safety
///
/// Directly calls C or kernel APIs.
#[allow(non_snake_case)]
pub unsafe fn ASharedMemory_setProt(fd: libc::c_int, prot: libc::c_int) -> libc::c_int {
    const ASHMEM_SET_PROT_MASK: libc::c_int =
        iow!(__ASHMEMIOC, 5, std::mem::size_of::<libc::c_ulong>()) as _;

    maybe_init();
    if let Some(fun) = LIBANDROID_ASHAREDMEMORY_SETPROT {
        return fun(fd, prot);
    }

    let r = libc::ioctl(fd, ASHMEM_SET_PROT_MASK, prot);
    if r != 0 {
        return -1;
    }
    r
}

#[cfg(test)]
mod tests {
    #[test]
    fn basic() {
        unsafe {
            let name = std::ffi::CString::new("/test-ashmem").unwrap();
            let fd = super::ASharedMemory_create(name.as_ptr(), 128);
            assert!(fd >= 0);
            assert_eq!(super::ASharedMemory_getSize(fd), 128);
            assert_eq!(super::ASharedMemory_setProt(fd, 0), 0);
            libc::close(fd);
        }
    }

    #[test]
    fn anonymous() {
        unsafe {
            let fd = super::ASharedMemory_create(std::ptr::null(), 128);
            assert!(fd >= 0);
            libc::close(fd);
        }
    }
}
