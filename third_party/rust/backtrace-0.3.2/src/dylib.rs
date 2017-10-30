use std::ffi::CString;
use std::marker;
use std::mem;
use std::sync::atomic::{AtomicUsize, Ordering};

use libc::{self, c_char, c_void};

pub struct Dylib {
    pub init: AtomicUsize,
}

pub struct Symbol<T> {
    pub name: &'static str,
    pub addr: AtomicUsize,
    pub _marker: marker::PhantomData<T>,
}

impl Dylib {
    pub unsafe fn get<'a, T>(&self, sym: &'a Symbol<T>) -> Option<&'a T> {
        self.load().and_then(|handle| {
            sym.get(handle)
        })
    }

    pub unsafe fn init(&self, path: &str) -> bool {
        if self.init.load(Ordering::SeqCst) != 0 {
            return true
        }
        let name = CString::new(path).unwrap();
        let ptr = libc::dlopen(name.as_ptr() as *const c_char, libc::RTLD_LAZY);
        if ptr.is_null() {
            return false
        }
        match self.init.compare_and_swap(0, ptr as usize, Ordering::SeqCst) {
            0 => {}
            _ => { libc::dlclose(ptr); }
        }
        return true
    }

    unsafe fn load(&self) -> Option<*mut c_void> {
        match self.init.load(Ordering::SeqCst) {
            0 => None,
            n => Some(n as *mut c_void),
        }
    }
}

impl<T> Symbol<T> {
    unsafe fn get(&self, handle: *mut c_void) -> Option<&T> {
        assert_eq!(mem::size_of::<T>(), mem::size_of_val(&self.addr));
        if self.addr.load(Ordering::SeqCst) == 0 {
            self.addr.store(fetch(handle, self.name.as_ptr()), Ordering::SeqCst)
        }
        if self.addr.load(Ordering::SeqCst) == 1 {
            None
        } else {
            mem::transmute::<&AtomicUsize, Option<&T>>(&self.addr)
        }
    }
}

unsafe fn fetch(handle: *mut c_void, name: *const u8) -> usize {
    let ptr = libc::dlsym(handle, name as *const _);
    if ptr.is_null() {
        1
    } else {
        ptr as usize
    }
}
