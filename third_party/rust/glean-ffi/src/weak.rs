//! Based on `library/std/src/sys/unix/weak.rs` from the Rust libstd code base (Rust v1.49.0).
//!
//! Original License: MIT/Apache.
//! <https://github.com/rust-lang/rust/blob/master/LICENSE-MIT>
//! <https://github.com/rust-lang/rust/blob/master/LICENSE-APACHE>
//!
//! Support for "weak linkage" to symbols on Unix
//!
//! We use `dlsym` to get a symbol value at runtime.
//! It assumes that the symbol we're looking for is linked in to our application somehow.
//! In case of glean-ffi and the RLB this is the case if everything is built as one dynamic
//! library.

use std::ffi::CStr;
use std::marker;
use std::mem;
use std::sync::atomic::{AtomicUsize, Ordering};

macro_rules! weak {
    (fn $name:ident($($t:ty),*) -> $ret:ty) => (
        static $name: crate::weak::Weak<unsafe extern fn($($t),*) -> $ret> =
            crate::weak::Weak::new(concat!(stringify!($name), '\0'));
    )
}

pub struct Weak<F> {
    name: &'static str,
    addr: AtomicUsize,
    _marker: marker::PhantomData<F>,
}

impl<F> Weak<F> {
    pub const fn new(name: &'static str) -> Weak<F> {
        Weak {
            name,
            addr: AtomicUsize::new(1),
            _marker: marker::PhantomData,
        }
    }

    pub fn get(&self) -> Option<F> {
        assert_eq!(mem::size_of::<F>(), mem::size_of::<usize>());
        unsafe {
            if self.addr.load(Ordering::SeqCst) == 1 {
                self.addr.store(fetch(self.name), Ordering::SeqCst);
            }
            match self.addr.load(Ordering::SeqCst) {
                0 => None,
                addr => Some(mem::transmute_copy::<usize, F>(&addr)),
            }
        }
    }
}

unsafe fn fetch(name: &str) -> usize {
    let name = match CStr::from_bytes_with_nul(name.as_bytes()) {
        Ok(cstr) => cstr,
        Err(..) => return 0,
    };
    ::libc::dlsym(::libc::RTLD_DEFAULT, name.as_ptr()) as usize
}
