//! Demangle Rust compiler symbol names.
//!
//! This crate provides a `demangle` function which will return a `Demangle`
//! sentinel value that can be used to learn about the demangled version of a
//! symbol name. The demangled representation will be the same as the original
//! if it doesn't look like a mangled symbol name.
//!
//! `Demangle` can be formatted with the `Display` trait. The alternate
//! modifier (`#`) can be used to format the symbol name without the
//! trailing hash value.
//!
//! # Examples
//!
//! ```
//! use rustc_demangle::demangle;
//!
//! assert_eq!(demangle("_ZN4testE").to_string(), "test");
//! assert_eq!(demangle("_ZN3foo3barE").to_string(), "foo::bar");
//! assert_eq!(demangle("foo").to_string(), "foo");
//! // With hash
//! assert_eq!(format!("{}", demangle("_ZN3foo17h05af221e174051e9E")), "foo::h05af221e174051e9");
//! // Without hash
//! assert_eq!(format!("{:#}", demangle("_ZN3foo17h05af221e174051e9E")), "foo");
//! ```

#![no_std]
#![deny(missing_docs)]

#[cfg(test)]
#[macro_use]
extern crate std;

use core::fmt;

/// Representation of a demangled symbol name.
pub struct Demangle<'a> {
    original: &'a str,
    inner: &'a str,
    valid: bool,
    /// The number of ::-separated elements in the original name.
    elements: usize,
}

/// De-mangles a Rust symbol into a more readable version
///
/// All rust symbols by default are mangled as they contain characters that
/// cannot be represented in all object files. The mangling mechanism is similar
/// to C++'s, but Rust has a few specifics to handle items like lifetimes in
/// symbols.
///
/// This function will take a **mangled** symbol (typically acquired from a
/// `Symbol` which is in turn resolved from a `Frame`) and then writes the
/// de-mangled version into the given `writer`. If the symbol does not look like
/// a mangled symbol, it is still written to `writer`.
///
/// # Examples
///
/// ```
/// use rustc_demangle::demangle;
///
/// assert_eq!(demangle("_ZN4testE").to_string(), "test");
/// assert_eq!(demangle("_ZN3foo3barE").to_string(), "foo::bar");
/// assert_eq!(demangle("foo").to_string(), "foo");
/// ```

// All rust symbols are in theory lists of "::"-separated identifiers. Some
// assemblers, however, can't handle these characters in symbol names. To get
// around this, we use C++-style mangling. The mangling method is:
//
// 1. Prefix the symbol with "_ZN"
// 2. For each element of the path, emit the length plus the element
// 3. End the path with "E"
//
// For example, "_ZN4testE" => "test" and "_ZN3foo3barE" => "foo::bar".
//
// We're the ones printing our backtraces, so we can't rely on anything else to
// demangle our symbols. It's *much* nicer to look at demangled symbols, so
// this function is implemented to give us nice pretty output.
//
// Note that this demangler isn't quite as fancy as it could be. We have lots
// of other information in our symbols like hashes, version, type information,
// etc. Additionally, this doesn't handle glue symbols at all.
pub fn demangle(mut s: &str) -> Demangle {
    // During ThinLTO LLVM may import and rename internal symbols, so strip out
    // those endings first as they're on of the last manglings applied to symbol
    // names.
    let llvm = ".llvm.";
    if let Some(i) = s.find(llvm) {
        let candidate = &s[i + llvm.len()..];
        let all_hex = candidate.chars().all(|c| {
            match c {
                'A' ... 'F' | '0' ... '9' | '@' => true,
                _ => false,
            }
        });

        if all_hex {
            s = &s[..i];
        }
    }

    // First validate the symbol. If it doesn't look like anything we're
    // expecting, we just print it literally. Note that we must handle non-rust
    // symbols because we could have any function in the backtrace.
    let mut valid = true;
    let mut inner = s;
    if s.len() > 4 && s.starts_with("_ZN") && s.ends_with('E') {
        inner = &s[3..s.len() - 1];
    } else if s.len() > 3 && s.starts_with("ZN") && s.ends_with('E') {
        // On Windows, dbghelp strips leading underscores, so we accept "ZN...E"
        // form too.
        inner = &s[2..s.len() - 1];
    } else if s.len() > 5 && s.starts_with("__ZN") && s.ends_with('E') {
        // On OSX, symbols are prefixed with an extra _
        inner = &s[4..s.len() - 1];
    } else {
        valid = false;
    }

    // only work with ascii text
    if inner.bytes().any(|c| c & 0x80 != 0) {
        valid = false;
    }

    let mut elements = 0;
    if valid {
        let mut chars = inner.chars().peekable();
        while valid {
            let mut i = 0usize;
            while let Some(&c) = chars.peek() {
                if !c.is_digit(10) {
                    break
                }
                chars.next();
                let next = i.checked_mul(10)
                    .and_then(|i| i.checked_add(c as usize - '0' as usize));
                i = match next {
                    Some(i) => i,
                    None => {
                        valid = false;
                        break
                    }
                };
            }

            if i == 0 {
                valid = chars.next().is_none();
                break;
            } else if chars.by_ref().take(i).count() != i {
                valid = false;
            } else {
                elements += 1;
            }
        }
    }

    Demangle {
        inner: inner,
        valid: valid,
        elements: elements,
        original: s,
    }
}

/// Error returned from the `try_demangle` function below when demangling fails.
#[derive(Debug, Clone)]
pub struct TryDemangleError {
    _priv: (),
}

/// The same as `demangle`, except return an `Err` if the string does not appear
/// to be a Rust symbol, rather than "demangling" the given string as a no-op.
///
/// ```
/// extern crate rustc_demangle;
///
/// let not_a_rust_symbol = "la la la";
///
/// // The `try_demangle` function will reject strings which are not Rust symbols.
/// assert!(rustc_demangle::try_demangle(not_a_rust_symbol).is_err());
///
/// // While `demangle` will just pass the non-symbol through as a no-op.
/// assert_eq!(rustc_demangle::demangle(not_a_rust_symbol).as_str(), not_a_rust_symbol);
/// ```
pub fn try_demangle(s: &str) -> Result<Demangle, TryDemangleError> {
    let sym = demangle(s);
    if sym.valid {
        Ok(sym)
    } else {
        Err(TryDemangleError { _priv: () })
    }
}

impl<'a> Demangle<'a> {
    /// Returns the underlying string that's being demangled.
    pub fn as_str(&self) -> &'a str {
        self.original
    }
}

// Rust hashes are hex digits with an `h` prepended.
fn is_rust_hash(s: &str) -> bool {
    s.starts_with('h') && s[1..].chars().all(|c| c.is_digit(16))
}

impl<'a> fmt::Display for Demangle<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        // Alright, let's do this.
        if !self.valid {
            return f.write_str(self.inner);
        }

        let mut inner = self.inner;
        for element in 0..self.elements {
            let mut rest = inner;
            while rest.chars().next().unwrap().is_digit(10) {
                rest = &rest[1..];
            }
            let i: usize = inner[..(inner.len() - rest.len())].parse().unwrap();
            inner = &rest[i..];
            rest = &rest[..i];
            // Skip printing the hash if alternate formatting
            // was requested.
            if f.alternate() && element+1 == self.elements && is_rust_hash(&rest) {
                break;
            }
            if element != 0 {
                try!(f.write_str("::"));
            }
            if rest.starts_with("_$") {
                rest = &rest[1..];
            }
            while !rest.is_empty() {
                if rest.starts_with('.') {
                    if let Some('.') = rest[1..].chars().next() {
                        try!(f.write_str("::"));
                        rest = &rest[2..];
                    } else {
                        try!(f.write_str("."));
                        rest = &rest[1..];
                    }
                } else if rest.starts_with('$') {
                    macro_rules! demangle {
                        ($($pat:expr => $demangled:expr),*) => ({
                            $(if rest.starts_with($pat) {
                                try!(f.write_str($demangled));
                                rest = &rest[$pat.len()..];
                              } else)*
                            {
                                try!(f.write_str(rest));
                                break;
                            }

                        })
                    }

                    // see src/librustc/back/link.rs for these mappings
                    demangle! {
                        "$SP$" => "@",
                        "$BP$" => "*",
                        "$RF$" => "&",
                        "$LT$" => "<",
                        "$GT$" => ">",
                        "$LP$" => "(",
                        "$RP$" => ")",
                        "$C$" => ",",

                        // in theory we can demangle any Unicode code point, but
                        // for simplicity we just catch the common ones.
                        "$u7e$" => "~",
                        "$u20$" => " ",
                        "$u27$" => "'",
                        "$u5b$" => "[",
                        "$u5d$" => "]",
                        "$u7b$" => "{",
                        "$u7d$" => "}",
                        "$u3b$" => ";",
                        "$u2b$" => "+",
                        "$u22$" => "\""
                    }
                } else {
                    let idx = match rest.char_indices().find(|&(_, c)| c == '$' || c == '.') {
                        None => rest.len(),
                        Some((i, _)) => i,
                    };
                    try!(f.write_str(&rest[..idx]));
                    rest = &rest[idx..];
                }
            }
        }

        Ok(())
    }
}

impl<'a> fmt::Debug for Demangle<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        fmt::Display::fmt(self, f)
    }
}

#[cfg(test)]
mod tests {
    use std::prelude::v1::*;

    macro_rules! t {
        ($a:expr, $b:expr) => ({
            assert_eq!(super::demangle($a).to_string(), $b);
        })
    }

    macro_rules! t_nohash {
        ($a:expr, $b:expr) => ({
            assert_eq!(format!("{:#}", super::demangle($a)), $b);
        })
    }

    #[test]
    fn demangle() {
        t!("test", "test");
        t!("_ZN4testE", "test");
        t!("_ZN4test", "_ZN4test");
        t!("_ZN4test1a2bcE", "test::a::bc");
    }

    #[test]
    fn demangle_dollars() {
        t!("_ZN4$RP$E", ")");
        t!("_ZN8$RF$testE", "&test");
        t!("_ZN8$BP$test4foobE", "*test::foob");
        t!("_ZN9$u20$test4foobE", " test::foob");
        t!("_ZN35Bar$LT$$u5b$u32$u3b$$u20$4$u5d$$GT$E", "Bar<[u32; 4]>");
    }

    #[test]
    fn demangle_many_dollars() {
        t!("_ZN13test$u20$test4foobE", "test test::foob");
        t!("_ZN12test$BP$test4foobE", "test*test::foob");
    }


    #[test]
    fn demangle_osx() {
        t!("__ZN5alloc9allocator6Layout9for_value17h02a996811f781011E", "alloc::allocator::Layout::for_value::h02a996811f781011");
        t!("__ZN38_$LT$core..option..Option$LT$T$GT$$GT$6unwrap18_MSG_FILE_LINE_COL17haf7cb8d5824ee659E", "<core::option::Option<T>>::unwrap::_MSG_FILE_LINE_COL::haf7cb8d5824ee659");
        t!("__ZN4core5slice89_$LT$impl$u20$core..iter..traits..IntoIterator$u20$for$u20$$RF$$u27$a$u20$$u5b$T$u5d$$GT$9into_iter17h450e234d27262170E", "core::slice::<impl core::iter::traits::IntoIterator for &'a [T]>::into_iter::h450e234d27262170");
    }

    #[test]
    fn demangle_windows() {
        t!("ZN4testE", "test");
        t!("ZN13test$u20$test4foobE", "test test::foob");
        t!("ZN12test$RF$test4foobE", "test&test::foob");
    }

    #[test]
    fn demangle_elements_beginning_with_underscore() {
        t!("_ZN13_$LT$test$GT$E", "<test>");
        t!("_ZN28_$u7b$$u7b$closure$u7d$$u7d$E", "{{closure}}");
        t!("_ZN15__STATIC_FMTSTRE", "__STATIC_FMTSTR");
    }

    #[test]
    fn demangle_trait_impls() {
        t!("_ZN71_$LT$Test$u20$$u2b$$u20$$u27$static$u20$as$u20$foo..Bar$LT$Test$GT$$GT$3barE",
           "<Test + 'static as foo::Bar<Test>>::bar");
    }

    #[test]
    fn demangle_without_hash() {
        let s = "_ZN3foo17h05af221e174051e9E";
        t!(s, "foo::h05af221e174051e9");
        t_nohash!(s, "foo");
    }

    #[test]
    fn demangle_without_hash_edgecases() {
        // One element, no hash.
        t_nohash!("_ZN3fooE", "foo");
        // Two elements, no hash.
        t_nohash!("_ZN3foo3barE", "foo::bar");
        // Longer-than-normal hash.
        t_nohash!("_ZN3foo20h05af221e174051e9abcE", "foo");
        // Shorter-than-normal hash.
        t_nohash!("_ZN3foo5h05afE", "foo");
        // Valid hash, but not at the end.
        t_nohash!("_ZN17h05af221e174051e93fooE", "h05af221e174051e9::foo");
        // Not a valid hash, missing the 'h'.
        t_nohash!("_ZN3foo16ffaf221e174051e9E", "foo::ffaf221e174051e9");
        // Not a valid hash, has a non-hex-digit.
        t_nohash!("_ZN3foo17hg5af221e174051e9E", "foo::hg5af221e174051e9");
    }

    #[test]
    fn demangle_thinlto() {
        // One element, no hash.
        t!("_ZN3fooE.llvm.9D1C9369", "foo");
        t!("_ZN3fooE.llvm.9D1C9369@@16", "foo");
        t_nohash!("_ZN9backtrace3foo17hbb467fcdaea5d79bE.llvm.A5310EB9", "backtrace::foo");
    }

    #[test]
    fn dont_panic() {
        super::demangle("_ZN2222222222222222222222EE").to_string();
        super::demangle("_ZN5*70527e27.ll34csaғE").to_string();
        super::demangle("_ZN5*70527a54.ll34_$b.1E").to_string();
        super::demangle("\
            _ZN5~saäb4e\n\
            2734cOsbE\n\
            5usage20h)3\0\0\0\0\0\0\07e2734cOsbE\
        ").to_string();
    }
}
