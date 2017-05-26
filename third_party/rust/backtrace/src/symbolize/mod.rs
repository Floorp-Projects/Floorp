use std::fmt;
#[cfg(not(feature = "cpp_demangle"))]
use std::marker::PhantomData;
use std::os::raw::c_void;
use std::path::Path;
use std::str;
use rustc_demangle::{try_demangle, Demangle};

/// Resolve an address to a symbol, passing the symbol to the specified
/// closure.
///
/// This function will look up the given address in areas such as the local
/// symbol table, dynamic symbol table, or DWARF debug info (depending on the
/// activated implementation) to find symbols to yield.
///
/// The closure may not be called if resolution could not be performed, and it
/// also may be called more than once in the case of inlined functions.
///
/// Symbols yielded represent the execution at the specified `addr`, returning
/// file/line pairs for that addres (if available).
///
/// # Example
///
/// ```
/// extern crate backtrace;
///
/// fn main() {
///     backtrace::trace(|frame| {
///         let ip = frame.ip();
///
///         backtrace::resolve(ip, |symbol| {
///             // ...
///         });
///
///         false // only look at the top frame
///     });
/// }
/// ```
pub fn resolve<F: FnMut(&Symbol)>(addr: *mut c_void, mut cb: F) {
    resolve_imp(addr, &mut cb)
}

/// A trait representing the resolution of a symbol in a file.
///
/// This trait is yielded as a trait object to the closure given to the
/// `backtrace::resolve` function, and it is virtually dispatched as it's
/// unknown which implementation is behind it.
///
/// A symbol can give contextual information about a funciton, for example the
/// name, filename, line number, precise address, etc. Not all information is
/// always available in a symbol, however, so all methods return an `Option`.
pub struct Symbol {
    inner: SymbolImp,
}

impl Symbol {
    /// Returns the name of this function.
    ///
    /// The returned structure can be used to query various properties about the
    /// symbol name:
    ///
    /// * The `Display` implementation will print out the demangled symbol.
    /// * The raw `str` value of the symbol can be accessed (if it's valid
    ///   utf-8).
    /// * The raw bytes for the symbol name can be accessed.
    pub fn name(&self) -> Option<SymbolName> {
        self.inner.name()
    }

    /// Returns the starting address of this function.
    pub fn addr(&self) -> Option<*mut c_void> {
        self.inner.addr()
    }

    /// Returns the file name where this function was defined.
    ///
    /// This is currently only available when libbacktrace is being used (e.g.
    /// unix platforms other than OSX) and when a binary is compiled with
    /// debuginfo. If neither of these conditions is met then this will likely
    /// return `None`.
    pub fn filename(&self) -> Option<&Path> {
        self.inner.filename()
    }

    /// Returns the line number for where this symbol is currently executing.
    ///
    /// This return value is typically `Some` if `filename` returns `Some`, and
    /// is consequently subject to similar caveats.
    pub fn lineno(&self) -> Option<u32> {
        self.inner.lineno()
    }
}

impl fmt::Debug for Symbol {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let mut d = f.debug_struct("Symbol");
        if let Some(name) = self.name() {
            d.field("name", &name);
        }
        if let Some(addr) = self.addr() {
            d.field("addr", &addr);
        }
        if let Some(filename) = self.filename() {
            d.field("filename", &filename);
        }
        if let Some(lineno) = self.lineno() {
            d.field("lineno", &lineno);
        }
        d.finish()
    }
}


cfg_if! {
    if #[cfg(feature = "cpp_demangle")] {
        // Maybe a parsed C++ symbol, if parsing the mangled symbol as Rust
        // failed.
        struct OptionCppSymbol<'a>(Option<::cpp_demangle::BorrowedSymbol<'a>>);

        impl<'a> OptionCppSymbol<'a> {
            fn parse(input: &'a [u8]) -> OptionCppSymbol<'a> {
                OptionCppSymbol(::cpp_demangle::BorrowedSymbol::new(input).ok())
            }

            fn none() -> OptionCppSymbol<'a> {
                OptionCppSymbol(None)
            }
        }
    } else {
        // Make sure to keep this zero-sized, so that the `cpp_demangle` feature
        // has no cost when disabled.
        struct OptionCppSymbol<'a>(PhantomData<&'a ()>);

        impl<'a> OptionCppSymbol<'a> {
            fn parse(_: &'a [u8]) -> OptionCppSymbol<'a> {
                OptionCppSymbol(PhantomData)
            }

            fn none() -> OptionCppSymbol<'a> {
                OptionCppSymbol(PhantomData)
            }
        }
    }
}

/// A wrapper around a symbol name to provide ergonomic accessors to the
/// demangled name, the raw bytes, the raw string, etc.
// Allow dead code for when the `cpp_demangle` feature is not enabled.
#[allow(dead_code)]
pub struct SymbolName<'a> {
    bytes: &'a [u8],
    demangled: Option<Demangle<'a>>,
    cpp_demangled: OptionCppSymbol<'a>,
}

impl<'a> SymbolName<'a> {
    /// Creates a new symbol name from the raw underlying bytes.
    pub fn new(bytes: &'a [u8]) -> SymbolName<'a> {
        let str_bytes = str::from_utf8(bytes).ok();
        let demangled = str_bytes.and_then(|s| try_demangle(s).ok());

        let cpp = if demangled.is_none() {
            OptionCppSymbol::parse(bytes)
        } else {
            OptionCppSymbol::none()
        };

        SymbolName {
            bytes: bytes,
            demangled: demangled,
            cpp_demangled: cpp,
        }
    }

    /// Returns the raw symbol name as a `str` if the symbols is valid utf-8.
    pub fn as_str(&self) -> Option<&'a str> {
        self.demangled
            .as_ref()
            .map(|s| s.as_str())
            .or_else(|| {
                str::from_utf8(self.bytes).ok()
            })
    }

    /// Returns the raw symbol name as a list of bytes
    pub fn as_bytes(&self) -> &'a [u8] {
        self.bytes
    }
}

cfg_if! {
    if #[cfg(feature = "cpp_demangle")] {
        impl<'a> fmt::Display for SymbolName<'a> {
            fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
                if let Some(ref s) = self.demangled {
                    s.fmt(f)
                } else if let Some(ref cpp) = self.cpp_demangled.0 {
                    cpp.fmt(f)
                } else {
                    String::from_utf8_lossy(self.bytes).fmt(f)
                }
            }
        }
    } else {
        impl<'a> fmt::Display for SymbolName<'a> {
            fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
                if let Some(ref s) = self.demangled {
                    s.fmt(f)
                } else {
                    String::from_utf8_lossy(self.bytes).fmt(f)
                }
            }
        }
    }
}

cfg_if! {
    if #[cfg(feature = "cpp_demangle")] {
        impl<'a> fmt::Debug for SymbolName<'a> {
            fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
                use std::fmt::Write;

                if let Some(ref s) = self.demangled {
                    return s.fmt(f)
                }

                // This may to print if the demangled symbol isn't actually
                // valid, so handle the error here gracefully by not propagating
                // it outwards.
                if let Some(ref cpp) = self.cpp_demangled.0 {
                    let mut s = String::new();
                    if write!(s, "{}", cpp).is_ok() {
                        return s.fmt(f)
                    }
                }

                String::from_utf8_lossy(self.bytes).fmt(f)
            }
        }
    } else {
        impl<'a> fmt::Debug for SymbolName<'a> {
            fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
                if let Some(ref s) = self.demangled {
                    s.fmt(f)
                } else {
                    String::from_utf8_lossy(self.bytes).fmt(f)
                }
            }
        }
    }
}

cfg_if! {
    if #[cfg(all(windows, feature = "dbghelp"))] {
        mod dbghelp;
        use self::dbghelp::resolve as resolve_imp;
        use self::dbghelp::Symbol as SymbolImp;
    } else if #[cfg(all(feature = "libbacktrace",
                        unix,
                        not(target_os = "emscripten"),
                        not(target_os = "macos"),
                        not(target_os = "ios")))] {
        mod libbacktrace;
        use self::libbacktrace::resolve as resolve_imp;
        use self::libbacktrace::Symbol as SymbolImp;
    } else if #[cfg(all(feature = "coresymbolication",
                        any(target_os = "macos",
                            target_os = "ios")))] {
        mod coresymbolication;
        use self::coresymbolication::resolve as resolve_imp;
        use self::coresymbolication::Symbol as SymbolImp;
    } else if #[cfg(all(unix,
                        not(target_os = "emscripten"),
                        feature = "dladdr"))] {
        mod dladdr;
        use self::dladdr::resolve as resolve_imp;
        use self::dladdr::Symbol as SymbolImp;
    } else {
        mod noop;
        use self::noop::resolve as resolve_imp;
        use self::noop::Symbol as SymbolImp;
    }
}
