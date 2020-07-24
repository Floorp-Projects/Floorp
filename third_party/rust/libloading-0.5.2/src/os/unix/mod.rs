use util::{ensure_compatible_types, cstr_cow_from_bytes};

use std::ffi::{CStr, OsStr};
use std::{fmt, io, marker, mem, ptr};
use std::os::raw;
use std::os::unix::ffi::OsStrExt;

extern "C" {
    fn rust_libloading_dlerror_mutex_lock();
    fn rust_libloading_dlerror_mutex_unlock();
}

struct DlerrorMutexGuard(());

impl DlerrorMutexGuard {
    fn new() -> DlerrorMutexGuard {
        unsafe {
            rust_libloading_dlerror_mutex_lock();
        }
        DlerrorMutexGuard(())
    }
}

impl Drop for DlerrorMutexGuard {
    fn drop(&mut self) {
        unsafe {
            rust_libloading_dlerror_mutex_unlock();
        }
    }
}

// libdl is crazy.
//
// First of all, whole error handling scheme in libdl is done via setting and querying some global
// state, therefore it is not safe to use libdl in MT-capable environment at all. Only in POSIX
// 2008+TC1 a thread-local state was allowed, which for our purposes is way too late.
fn with_dlerror<T, F>(closure: F) -> Result<T, Option<io::Error>>
where F: FnOnce() -> Option<T> {
    // We will guard all uses of libdl library with our own mutex. This makes libdl
    // safe to use in MT programs provided the only way a program uses libdl is via this library.
    let _lock = DlerrorMutexGuard::new();
    // While we could could call libdl here to clear the previous error value, only the dlsym
    // depends on it being cleared beforehand and only in some cases too. We will instead clear the
    // error inside the dlsym binding instead.
    //
    // In all the other cases, clearing the error here will only be hiding misuse of these bindings
    // or the libdl.
    closure().ok_or_else(|| unsafe {
        // This code will only get executed if the `closure` returns `None`.
        let error = dlerror();
        if error.is_null() {
            // In non-dlsym case this may happen when there’re bugs in our bindings or there’s
            // non-libloading user of libdl; possibly in another thread.
            None
        } else {
            // You can’t even rely on error string being static here; call to subsequent dlerror
            // may invalidate or overwrite the error message. Why couldn’t they simply give up the
            // ownership over the message?
            // TODO: should do locale-aware conversion here. OTOH Rust doesn’t seem to work well in
            // any system that uses non-utf8 locale, so I doubt there’s a problem here.
            let message = CStr::from_ptr(error).to_string_lossy().into_owned();
            Some(io::Error::new(io::ErrorKind::Other, message))
            // Since we do a copy of the error string above, maybe we should call dlerror again to
            // let libdl know it may free its copy of the string now?
        }
    })
}

/// A platform-specific equivalent of the cross-platform `Library`.
pub struct Library {
    handle: *mut raw::c_void
}

unsafe impl Send for Library {}

// That being said... this section in the volume 2 of POSIX.1-2008 states:
//
// > All functions defined by this volume of POSIX.1-2008 shall be thread-safe, except that the
// > following functions need not be thread-safe.
//
// With notable absence of any dl* function other than dlerror in the list. By “this volume”
// I suppose they refer precisely to the “volume 2”. dl* family of functions are specified
// by this same volume, so the conclusion is indeed that dl* functions are required by POSIX
// to be thread-safe. Great!
//
// See for more details:
//
//  * https://github.com/nagisa/rust_libloading/pull/17
//  * http://pubs.opengroup.org/onlinepubs/9699919799/functions/V2_chap02.html#tag_15_09_01
unsafe impl Sync for Library {}

impl Library {
    /// Find and load a shared library (module).
    ///
    /// Locations where library is searched for is platform specific and can’t be adjusted
    /// portably.
    ///
    /// Corresponds to `dlopen(filename, RTLD_NOW)`.
    #[inline]
    pub fn new<P: AsRef<OsStr>>(filename: P) -> ::Result<Library> {
        Library::open(Some(filename), RTLD_NOW)
    }

    /// Load the dynamic libraries linked into main program.
    ///
    /// This allows retrieving symbols from any **dynamic** library linked into the program,
    /// without specifying the exact library.
    ///
    /// Corresponds to `dlopen(NULL, RTLD_NOW)`.
    #[inline]
    pub fn this() -> Library {
        Library::open(None::<&OsStr>, RTLD_NOW).unwrap()
    }

    /// Find and load a shared library (module).
    ///
    /// Locations where library is searched for is platform specific and can’t be adjusted
    /// portably.
    ///
    /// If the `filename` is None, null pointer is passed to `dlopen`.
    ///
    /// Corresponds to `dlopen(filename, flags)`.
    pub fn open<P>(filename: Option<P>, flags: raw::c_int) -> ::Result<Library>
    where P: AsRef<OsStr> {
        let filename = match filename {
            None => None,
            Some(ref f) => Some(try!(cstr_cow_from_bytes(f.as_ref().as_bytes()))),
        };
        with_dlerror(move || {
            let result = unsafe {
                let r = dlopen(match filename {
                    None => ptr::null(),
                    Some(ref f) => f.as_ptr()
                }, flags);
                // ensure filename lives until dlopen completes
                drop(filename);
                r
            };
            if result.is_null() {
                None
            } else {
                Some(Library {
                    handle: result
                })
            }
        }).map_err(|e| e.unwrap_or_else(||
            panic!("dlopen failed but dlerror did not report anything")
        ))
    }

    /// Get a pointer to function or static variable by symbol name.
    ///
    /// The `symbol` may not contain any null bytes, with an exception of last byte. A null
    /// terminated `symbol` may avoid a string allocation in some cases.
    ///
    /// Symbol is interpreted as-is; no mangling is done. This means that symbols like `x::y` are
    /// most likely invalid.
    ///
    /// ## Unsafety
    ///
    /// Pointer to a value of arbitrary type is returned. Using a value with wrong type is
    /// undefined.
    ///
    /// ## Platform-specific behaviour
    ///
    /// OS X uses some sort of lazy initialization scheme, which makes loading TLS variables
    /// impossible. Using a TLS variable loaded this way on OS X is undefined behaviour.
    pub unsafe fn get<T>(&self, symbol: &[u8]) -> ::Result<Symbol<T>> {
        ensure_compatible_types::<T, *mut raw::c_void>();
        let symbol = try!(cstr_cow_from_bytes(symbol));
        // `dlsym` may return nullptr in two cases: when a symbol genuinely points to a null
        // pointer or the symbol cannot be found. In order to detect this case a double dlerror
        // pattern must be used, which is, sadly, a little bit racy.
        //
        // We try to leave as little space as possible for this to occur, but we can’t exactly
        // fully prevent it.
        match with_dlerror(|| {
            dlerror();
            let symbol = dlsym(self.handle, symbol.as_ptr());
            if symbol.is_null() {
                None
            } else {
                Some(Symbol {
                    pointer: symbol,
                    pd: marker::PhantomData
                })
            }
        }) {
            Err(None) => Ok(Symbol {
                pointer: ptr::null_mut(),
                pd: marker::PhantomData
            }),
            Err(Some(e)) => Err(e),
            Ok(x) => Ok(x)
        }
    }

    /// Convert the `Library` to a raw handle.
    ///
    /// The handle returned by this function shall be usable with APIs which accept handles
    /// as returned by `dlopen`.
    pub fn into_raw(self) -> *mut raw::c_void {
        let handle = self.handle;
        mem::forget(self);
        handle
    }

    /// Convert a raw handle returned by `dlopen`-family of calls to a `Library`.
    ///
    /// ## Unsafety
    ///
    /// The pointer shall be a result of a successful call of the `dlopen`-family of functions or a
    /// pointer previously returned by `Library::into_raw` call. It must be valid to call `dlclose`
    /// with this pointer as an argument.
    pub unsafe fn from_raw(handle: *mut raw::c_void) -> Library {
        Library {
            handle: handle
        }
    }
}

impl Drop for Library {
    fn drop(&mut self) {
        with_dlerror(|| if unsafe { dlclose(self.handle) } == 0 {
            Some(())
        } else {
            None
        }).unwrap();
    }
}

impl fmt::Debug for Library {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.write_str(&format!("Library@{:p}", self.handle))
    }
}

/// Symbol from a library.
///
/// A major difference compared to the cross-platform `Symbol` is that this does not ensure the
/// `Symbol` does not outlive `Library` it comes from.
pub struct Symbol<T> {
    pointer: *mut raw::c_void,
    pd: marker::PhantomData<T>
}

impl<T> Symbol<T> {
    /// Convert the loaded Symbol into a raw pointer.
    pub fn into_raw(self) -> *mut raw::c_void {
        let pointer = self.pointer;
        mem::forget(self);
        pointer
    }
}

impl<T> Symbol<Option<T>> {
    /// Lift Option out of the symbol.
    pub fn lift_option(self) -> Option<Symbol<T>> {
        if self.pointer.is_null() {
            None
        } else {
            Some(Symbol {
                pointer: self.pointer,
                pd: marker::PhantomData,
            })
        }
    }
}

unsafe impl<T: Send> Send for Symbol<T> {}
unsafe impl<T: Sync> Sync for Symbol<T> {}

impl<T> Clone for Symbol<T> {
    fn clone(&self) -> Symbol<T> {
        Symbol { ..*self }
    }
}

impl<T> ::std::ops::Deref for Symbol<T> {
    type Target = T;
    fn deref(&self) -> &T {
        unsafe {
            // Additional reference level for a dereference on `deref` return value.
            mem::transmute(&self.pointer)
        }
    }
}

impl<T> fmt::Debug for Symbol<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        unsafe {
            let mut info: DlInfo = mem::uninitialized();
            if dladdr(self.pointer, &mut info) != 0 {
                if info.dli_sname.is_null() {
                    f.write_str(&format!("Symbol@{:p} from {:?}",
                                         self.pointer,
                                         CStr::from_ptr(info.dli_fname)))
                } else {
                    f.write_str(&format!("Symbol {:?}@{:p} from {:?}",
                                         CStr::from_ptr(info.dli_sname), self.pointer,
                                         CStr::from_ptr(info.dli_fname)))
                }
            } else {
                f.write_str(&format!("Symbol@{:p}", self.pointer))
            }
        }
    }
}

// Platform specific things

extern {
    fn dlopen(filename: *const raw::c_char, flags: raw::c_int) -> *mut raw::c_void;
    fn dlclose(handle: *mut raw::c_void) -> raw::c_int;
    fn dlsym(handle: *mut raw::c_void, symbol: *const raw::c_char) -> *mut raw::c_void;
    fn dlerror() -> *mut raw::c_char;
    fn dladdr(addr: *mut raw::c_void, info: *mut DlInfo) -> raw::c_int;
}

#[cfg(not(target_os="android"))]
const RTLD_NOW: raw::c_int = 2;
#[cfg(target_os="android")]
const RTLD_NOW: raw::c_int = 0;

#[repr(C)]
struct DlInfo {
  dli_fname: *const raw::c_char,
  dli_fbase: *mut raw::c_void,
  dli_sname: *const raw::c_char,
  dli_saddr: *mut raw::c_void
}

#[test]
fn this() {
    Library::this();
}
