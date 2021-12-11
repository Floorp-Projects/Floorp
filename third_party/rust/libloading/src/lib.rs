//! A memory-safer wrapper around system dynamic library loading primitives.
//!
//! Using this library allows loading [dynamic libraries](struct.Library.html) (also known as
//! shared libraries) and use functions & global variables contained within the libraries.
//!
//! `libloading` crate exposes a cross-platform interface to load a library and utilize its
//! contents, but little is done to paper over the differences in behaviour between different
//! platforms. The API documentation strives to document such differences on the best-effort basis.
//!
//! Platform specific APIs are also available in the [`os`](crate::os) module. These APIs are more
//! flexible but less safe.
//!
//! # Usage
//!
//! Add a dependency on this library to your `Cargo.toml`:
//!
//! ```toml
//! [dependencies]
//! libloading = "0.7"
//! ```
//!
//! Then inside your code:
//!
//! ```no_run
//! fn call_dynamic() -> Result<u32, Box<dyn std::error::Error>> {
//!     unsafe {
//!         let lib = libloading::Library::new("/path/to/liblibrary.so")?;
//!         let func: libloading::Symbol<unsafe extern fn() -> u32> = lib.get(b"my_func")?;
//!         Ok(func())
//!     }
//! }
//! ```
//!
//! The compiler will ensure that the loaded `function` will not outlive the `Library` it comes
//! from, preventing a common class of issues.
#![deny(
    missing_docs,
    clippy::all,
    unreachable_pub,
    unused,
)]
#![cfg_attr(docsrs, deny(broken_intra_doc_links))]
#![cfg_attr(docsrs, feature(doc_cfg))]

use std::env::consts::{DLL_PREFIX, DLL_SUFFIX};
use std::ffi::{OsStr, OsString};
use std::fmt;
use std::ops;
use std::marker;

#[cfg(unix)]
use self::os::unix as imp;
#[cfg(windows)]
use self::os::windows as imp;
pub use self::error::Error;

pub mod os;
pub mod changelog;
mod util;
mod error;

/// A loaded dynamic library.
pub struct Library(imp::Library);

impl Library {
    /// Find and load a dynamic library.
    ///
    /// The `filename` argument may be any of:
    ///
    /// * A library filename;
    /// * Absolute path to the library;
    /// * Relative (to the current working directory) path to the library.
    ///
    /// # Safety
    ///
    /// When a library is loaded initialization routines contained within the library are executed.
    /// For the purposes of safety, execution of these routines is conceptually the same calling an
    /// unknown foreign function and may impose arbitrary requirements on the caller for the call
    /// to be sound.
    ///
    /// Additionally, the callers of this function must also ensure that execution of the
    /// termination routines contained within the library is safe as well. These routines may be
    /// executed when the library is unloaded.
    ///
    /// # Thread-safety
    ///
    /// The implementation strives to be as MT-safe as sanely possible, however on certain
    /// platforms the underlying error-handling related APIs not always MT-safe. This library
    /// shares these limitations on those platforms. In particular, on certain UNIX targets
    /// `dlerror` is not MT-safe, resulting in garbage error messages in certain MT-scenarios.
    ///
    /// Calling this function from multiple threads is not MT-safe if used in conjunction with
    /// library filenames and the library search path is modified (`SetDllDirectory` function on
    /// Windows, `{DY,}LD_LIBRARY_PATH` environment variable on UNIX).
    ///
    /// # Platform-specific behaviour
    ///
    /// When a plain library filename is supplied, locations where library is searched for is
    /// platform specific and cannot be adjusted in a portable manner. See documentation for
    /// the platform specific [`os::unix::Library::new`] and [`os::windows::Library::new`] methods
    /// for further information on library lookup behaviour.
    ///
    /// If the `filename` specifies a library filename without path and with extension omitted,
    /// `.dll` extension is implicitly added on Windows.
    ///
    /// # Tips
    ///
    /// Distributing your dynamic libraries under a filename common to all platforms (e.g.
    /// `awesome.module`) allows to avoid code which has to account for platform’s conventional
    /// library filenames.
    ///
    /// Strive to specify an absolute or at least a relative path to your library, unless
    /// system-wide libraries are being loaded. Platform-dependent library search locations
    /// combined with various quirks related to path-less filenames may cause flakiness in
    /// programs.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use ::libloading::Library;
    /// // Any of the following are valid.
    /// unsafe {
    ///     let _ = Library::new("/path/to/awesome.module").unwrap();
    ///     let _ = Library::new("../awesome.module").unwrap();
    ///     let _ = Library::new("libsomelib.so.1").unwrap();
    /// }
    /// ```
    pub unsafe fn new<P: AsRef<OsStr>>(filename: P) -> Result<Library, Error> {
        imp::Library::new(filename).map(From::from)
    }

    /// Get a pointer to function or static variable by symbol name.
    ///
    /// The `symbol` may not contain any null bytes, with an exception of last byte. Providing a
    /// null terminated `symbol` may help to avoid an allocation.
    ///
    /// Symbol is interpreted as-is; no mangling is done. This means that symbols like `x::y` are
    /// most likely invalid.
    ///
    /// # Safety
    ///
    /// Users of this API must specify the correct type of the function or variable loaded. Using a
    /// `Symbol` with a wrong type is undefined.
    ///
    /// # Platform-specific behaviour
    ///
    /// Implementation of thread local variables is extremely platform specific and uses of such
    /// variables that work on e.g. Linux may have unintended behaviour on other targets.
    ///
    /// On POSIX implementations where the `dlerror` function is not confirmed to be MT-safe (such
    /// as FreeBSD), this function will unconditionally return an error when the underlying `dlsym`
    /// call returns a null pointer. There are rare situations where `dlsym` returns a genuine null
    /// pointer without it being an error. If loading a null pointer is something you care about,
    /// consider using the [`os::unix::Library::get_singlethreaded`] call.
    ///
    /// # Examples
    ///
    /// Given a loaded library:
    ///
    /// ```no_run
    /// # use ::libloading::Library;
    /// let lib = unsafe {
    ///     Library::new("/path/to/awesome.module").unwrap()
    /// };
    /// ```
    ///
    /// Loading and using a function looks like this:
    ///
    /// ```no_run
    /// # use ::libloading::{Library, Symbol};
    /// # let lib = unsafe {
    /// #     Library::new("/path/to/awesome.module").unwrap()
    /// # };
    /// unsafe {
    ///     let awesome_function: Symbol<unsafe extern fn(f64) -> f64> =
    ///         lib.get(b"awesome_function\0").unwrap();
    ///     awesome_function(0.42);
    /// }
    /// ```
    ///
    /// A static variable may also be loaded and inspected:
    ///
    /// ```no_run
    /// # use ::libloading::{Library, Symbol};
    /// # let lib = unsafe { Library::new("/path/to/awesome.module").unwrap() };
    /// unsafe {
    ///     let awesome_variable: Symbol<*mut f64> = lib.get(b"awesome_variable\0").unwrap();
    ///     **awesome_variable = 42.0;
    /// };
    /// ```
    pub unsafe fn get<'lib, T>(&'lib self, symbol: &[u8]) -> Result<Symbol<'lib, T>, Error> {
        self.0.get(symbol).map(|from| Symbol::from_raw(from, self))
    }

    /// Unload the library.
    ///
    /// This method might be a no-op, depending on the flags with which the `Library` was opened,
    /// what library was opened or other platform specifics.
    ///
    /// You only need to call this if you are interested in handling any errors that may arise when
    /// library is unloaded. Otherwise the implementation of `Drop` for `Library` will close the
    /// library and ignore the errors were they arise.
    ///
    /// The underlying data structures may still get leaked if an error does occur.
    pub fn close(self) -> Result<(), Error> {
        self.0.close()
    }
}

impl fmt::Debug for Library {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.0.fmt(f)
    }
}

impl From<imp::Library> for Library {
    fn from(lib: imp::Library) -> Library {
        Library(lib)
    }
}

impl From<Library> for imp::Library {
    fn from(lib: Library) -> imp::Library {
        lib.0
    }
}

unsafe impl Send for Library {}
unsafe impl Sync for Library {}

/// Symbol from a library.
///
/// This type is a safeguard against using dynamically loaded symbols after a `Library` is
/// unloaded. Primary method to create an instance of a `Symbol` is via [`Library::get`].
///
/// The `Deref` trait implementation allows use of `Symbol` as if it was a function or variable
/// itself, without taking care to “extract” function or variable manually most of the time.
///
/// [`Library::get`]: Library::get
pub struct Symbol<'lib, T: 'lib> {
    inner: imp::Symbol<T>,
    pd: marker::PhantomData<&'lib T>
}

impl<'lib, T> Symbol<'lib, T> {
    /// Extract the wrapped `os::platform::Symbol`.
    ///
    /// # Safety
    ///
    /// Using this function relinquishes all the lifetime guarantees. It is up to programmer to
    /// ensure the resulting `Symbol` is not used past the lifetime of the `Library` this symbol
    /// was loaded from.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use ::libloading::{Library, Symbol};
    /// unsafe {
    ///     let lib = Library::new("/path/to/awesome.module").unwrap();
    ///     let symbol: Symbol<*mut u32> = lib.get(b"symbol\0").unwrap();
    ///     let symbol = symbol.into_raw();
    /// }
    /// ```
    pub unsafe fn into_raw(self) -> imp::Symbol<T> {
        self.inner
    }

    /// Wrap the `os::platform::Symbol` into this safe wrapper.
    ///
    /// Note that, in order to create association between the symbol and the library this symbol
    /// came from, this function requires a reference to the library.
    ///
    /// # Safety
    ///
    /// The `library` reference must be exactly the library `sym` was loaded from.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use ::libloading::{Library, Symbol};
    /// unsafe {
    ///     let lib = Library::new("/path/to/awesome.module").unwrap();
    ///     let symbol: Symbol<*mut u32> = lib.get(b"symbol\0").unwrap();
    ///     let symbol = symbol.into_raw();
    ///     let symbol = Symbol::from_raw(symbol, &lib);
    /// }
    /// ```
    pub unsafe fn from_raw<L>(sym: imp::Symbol<T>, library: &'lib L) -> Symbol<'lib, T> {
        let _ = library; // ignore here for documentation purposes.
        Symbol {
            inner: sym,
            pd: marker::PhantomData
        }
    }
}

impl<'lib, T> Symbol<'lib, Option<T>> {
    /// Lift Option out of the symbol.
    ///
    /// # Examples
    ///
    /// ```no_run
    /// # use ::libloading::{Library, Symbol};
    /// unsafe {
    ///     let lib = Library::new("/path/to/awesome.module").unwrap();
    ///     let symbol: Symbol<Option<*mut u32>> = lib.get(b"symbol\0").unwrap();
    ///     let symbol: Symbol<*mut u32> = symbol.lift_option().expect("static is not null");
    /// }
    /// ```
    pub fn lift_option(self) -> Option<Symbol<'lib, T>> {
        self.inner.lift_option().map(|is| Symbol {
            inner: is,
            pd: marker::PhantomData,
        })
    }
}

impl<'lib, T> Clone for Symbol<'lib, T> {
    fn clone(&self) -> Symbol<'lib, T> {
        Symbol {
            inner: self.inner.clone(),
            pd: marker::PhantomData
        }
    }
}

// FIXME: implement FnOnce for callable stuff instead.
impl<'lib, T> ops::Deref for Symbol<'lib, T> {
    type Target = T;
    fn deref(&self) -> &T {
        ops::Deref::deref(&self.inner)
    }
}

impl<'lib, T> fmt::Debug for Symbol<'lib, T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.inner.fmt(f)
    }
}

unsafe impl<'lib, T: Send> Send for Symbol<'lib, T> {}
unsafe impl<'lib, T: Sync> Sync for Symbol<'lib, T> {}

/// Converts a library name to a filename generally appropriate for use on the system.
///
/// The function will prepend prefixes (such as `lib`) and suffixes (such as `.so`) to the library
/// `name` to construct the filename.
///
/// # Examples
///
/// It can be used to load global libraries in a platform independent manner:
///
/// ```
/// use libloading::{Library, library_filename};
/// // Will attempt to load `libLLVM.so` on Linux, `libLLVM.dylib` on macOS and `LLVM.dll` on
/// // Windows.
/// let library = unsafe {
///     Library::new(library_filename("LLVM"))
/// };
/// ```
pub fn library_filename<S: AsRef<OsStr>>(name: S) -> OsString {
    let name = name.as_ref();
    let mut string = OsString::with_capacity(name.len() + DLL_PREFIX.len() + DLL_SUFFIX.len());
    string.push(DLL_PREFIX);
    string.push(name);
    string.push(DLL_SUFFIX);
    string
}
