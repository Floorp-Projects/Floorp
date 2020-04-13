//! A memory-safer wrapper around system dynamic library loading primitives.
//!
//! Using this library allows loading [dynamic libraries](struct.Library.html) (also known as
//! shared libraries) as well as use functions and static variables these libraries contain.
//!
//! While the library does expose a cross-platform interface to load a library and find stuff
//! inside it, little is done to paper over the platform differences, especially where library
//! loading is involved. The documentation for each function will attempt to document such
//! differences on the best-effort basis.
//!
//! Less safe, platform specific bindings are also available. See the
//! [`os::platform`](os/index.html) module for details.
//!
//! # Usage
//!
//! Add a dependency on this library to your `Cargo.toml`:
//!
//! ```toml
//! [dependencies]
//! libloading = "0.5"
//! ```
//!
//! Then inside your project
//!
//! ```no_run
//! extern crate libloading as lib;
//!
//! fn call_dynamic() -> lib::Result<u32> {
//!     let lib = lib::Library::new("/path/to/liblibrary.so")?;
//!     unsafe {
//!         let func: lib::Symbol<unsafe extern fn() -> u32> = lib.get(b"my_func")?;
//!         Ok(func())
//!     }
//! }
//! ```
//!
//! The compiler will ensure that the loaded `function` will not outlive the `Library` it comes
//! from, preventing a common cause of undefined behaviour and memory safety problems.
use std::ffi::OsStr;
use std::fmt;
use std::ops;
use std::marker;

#[cfg(unix)]
use self::os::unix as imp;
#[cfg(windows)]
use self::os::windows as imp;

pub mod os;
pub mod changelog;
mod util;

pub type Result<T> = ::std::io::Result<T>;

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
    /// ## Thread-safety
    ///
    /// The implementation strives to be as MT-safe as sanely possible, however due to certain
    /// error-handling related resources not always being safe, this library is not MT-safe either.
    ///
    /// * On Windows Vista and earlier error handling falls back to [`SetErrorMode`], which is not
    ///   MT-safe. MT-scenarios involving this function may cause a traditional data race;
    /// * On some UNIX targets `dlerror` might not be MT-safe, resulting in garbage error messages
    ///   in certain MT-scenarios.
    ///
    /// [`SetErrorMode`]: https://msdn.microsoft.com/en-us/library/windows/desktop/ms680621(v=vs.85).aspx
    ///
    /// Calling this function from multiple threads is not safe if used in conjunction with
    /// path-less filename and library search path is modified (`SetDllDirectory` function on
    /// Windows, `{DY,}LD_LIBRARY_PATH` environment variable on UNIX).
    ///
    /// ## Platform-specific behaviour
    ///
    /// When a plain library filename is supplied, locations where library is searched for is
    /// platform specific and cannot be adjusted in a portable manner.
    ///
    /// ### Windows
    ///
    /// If the `filename` specifies a library filename without path and with extension omitted,
    /// `.dll` extension is implicitly added. This behaviour may be suppressed by appending a
    /// trailing `.` to the `filename`.
    ///
    /// If the library contains thread local variables (MSVC’s `_declspec(thread)`, Rust’s
    /// `#[thread_local]` attributes), loading the library will fail on versions prior to Windows
    /// Vista.
    ///
    /// ## Tips
    ///
    /// Distributing your dynamic libraries under a filename common to all platforms (e.g.
    /// `awesome.module`) allows to avoid code which has to account for platform’s conventional
    /// library filenames.
    ///
    /// Strive to specify absolute or relative path to your library, unless system-wide libraries
    /// are being loaded.  Platform-dependent library search locations combined with various quirks
    /// related to path-less filenames may cause flaky code.
    ///
    /// ## Examples
    ///
    /// ```no_run
    /// # use ::libloading::Library;
    /// // Any of the following are valid.
    /// let _ = Library::new("/path/to/awesome.module").unwrap();
    /// let _ = Library::new("../awesome.module").unwrap();
    /// let _ = Library::new("libsomelib.so.1").unwrap();
    /// ```
    pub fn new<P: AsRef<OsStr>>(filename: P) -> Result<Library> {
        imp::Library::new(filename).map(From::from)
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
    /// On Linux and Windows, a TLS variable acts just like any regular static variable. OS X uses
    /// some sort of lazy initialization scheme, which makes loading TLS variables this way
    /// impossible. Using a TLS variable loaded this way on OS X is undefined behaviour.
    ///
    /// ## Examples
    ///
    /// Given a loaded library:
    ///
    /// ```no_run
    /// # use ::libloading::Library;
    /// let lib = Library::new("/path/to/awesome.module").unwrap();
    /// ```
    ///
    /// Loading and using a function looks like this:
    ///
    /// ```no_run
    /// # use ::libloading::{Library, Symbol};
    /// # let lib = Library::new("/path/to/awesome.module").unwrap();
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
    /// # let lib = Library::new("/path/to/awesome.module").unwrap();
    /// unsafe {
    ///     let awesome_variable: Symbol<*mut f64> = lib.get(b"awesome_variable\0").unwrap();
    ///     **awesome_variable = 42.0;
    /// };
    /// ```
    pub unsafe fn get<'lib, T>(&'lib self, symbol: &[u8]) -> Result<Symbol<'lib, T>> {
        self.0.get(symbol).map(|from| Symbol::from_raw(from, self))
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
/// unloaded. Primary method to create an instance of a `Symbol` is via `Library::get`.
///
/// Due to implementation of the `Deref` trait, an instance of `Symbol` may be used as if it was a
/// function or variable directly, without taking care to “extract” function or variable manually
/// most of the time.
///
/// See [`Library::get`] for details.
///
/// [`Library::get`]: ./struct.Library.html#method.get
pub struct Symbol<'lib, T: 'lib> {
    inner: imp::Symbol<T>,
    pd: marker::PhantomData<&'lib T>
}

impl<'lib, T> Symbol<'lib, T> {
    /// Extract the wrapped `os::platform::Symbol`.
    ///
    /// ## Unsafety
    /// Using this function relinquishes all the lifetime guarantees. It is up to programmer to
    /// ensure the resulting `Symbol` is not used past the lifetime of the `Library` this symbol
    /// was loaded from.
    ///
    /// ## Examples
    ///
    /// ```no_run
    /// # use ::libloading::{Library, Symbol};
    /// let lib = Library::new("/path/to/awesome.module").unwrap();
    /// unsafe {
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
    /// came from, this function requires reference to the library provided.
    ///
    /// ## Unsafety
    ///
    /// It is invalid to provide a reference to any other value other than the library the `sym`
    /// was loaded from. Doing so invalidates any lifetime guarantees.
    ///
    /// ## Examples
    ///
    /// ```no_run
    /// # use ::libloading::{Library, Symbol};
    /// let lib = Library::new("/path/to/awesome.module").unwrap();
    /// unsafe {
    ///     let symbol: Symbol<*mut u32> = lib.get(b"symbol\0").unwrap();
    ///     let symbol = symbol.into_raw();
    ///     let symbol = Symbol::from_raw(symbol, &lib);
    /// }
    /// ```
    pub unsafe fn from_raw<L>(sym: imp::Symbol<T>, _: &'lib L) -> Symbol<'lib, T> {
        Symbol {
            inner: sym,
            pd: marker::PhantomData
        }
    }
}

impl<'lib, T> Symbol<'lib, Option<T>> {
    /// Lift Option out of the symbol.
    ///
    /// ## Examples
    ///
    /// ```no_run
    /// # use ::libloading::{Library, Symbol};
    /// let lib = Library::new("/path/to/awesome.module").unwrap();
    /// unsafe {
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
