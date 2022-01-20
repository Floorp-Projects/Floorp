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

mod legacy;
mod v0;

use core::fmt;

/// Representation of a demangled symbol name.
pub struct Demangle<'a> {
    style: Option<DemangleStyle<'a>>,
    original: &'a str,
    suffix: &'a str,
}

enum DemangleStyle<'a> {
    Legacy(legacy::Demangle<'a>),
    V0(v0::Demangle<'a>),
}

/// De-mangles a Rust symbol into a more readable version
///
/// This function will take a **mangled** symbol and return a value. When printed,
/// the de-mangled version will be written. If the symbol does not look like
/// a mangled symbol, the original value will be written instead.
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
pub fn demangle(mut s: &str) -> Demangle {
    // During ThinLTO LLVM may import and rename internal symbols, so strip out
    // those endings first as they're one of the last manglings applied to symbol
    // names.
    let llvm = ".llvm.";
    if let Some(i) = s.find(llvm) {
        let candidate = &s[i + llvm.len()..];
        let all_hex = candidate.chars().all(|c| match c {
            'A'..='F' | '0'..='9' | '@' => true,
            _ => false,
        });

        if all_hex {
            s = &s[..i];
        }
    }

    let mut suffix = "";
    let mut style = match legacy::demangle(s) {
        Ok((d, s)) => {
            suffix = s;
            Some(DemangleStyle::Legacy(d))
        }
        Err(()) => match v0::demangle(s) {
            Ok((d, s)) => {
                suffix = s;
                Some(DemangleStyle::V0(d))
            }
            Err(v0::Invalid) => None,
        },
    };

    // Output like LLVM IR adds extra period-delimited words. See if
    // we are in that case and save the trailing words if so.
    if !suffix.is_empty() {
        if suffix.starts_with('.') && is_symbol_like(suffix) {
            // Keep the suffix.
        } else {
            // Reset the suffix and invalidate the demangling.
            suffix = "";
            style = None;
        }
    }

    Demangle {
        style,
        original: s,
        suffix,
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
    if sym.style.is_some() {
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

fn is_symbol_like(s: &str) -> bool {
    s.chars().all(|c| {
        // Once `char::is_ascii_punctuation` and `char::is_ascii_alphanumeric`
        // have been stable for long enough, use those instead for clarity
        is_ascii_alphanumeric(c) || is_ascii_punctuation(c)
    })
}

// Copied from the documentation of `char::is_ascii_alphanumeric`
fn is_ascii_alphanumeric(c: char) -> bool {
    match c {
        '\u{0041}'..='\u{005A}' | '\u{0061}'..='\u{007A}' | '\u{0030}'..='\u{0039}' => true,
        _ => false,
    }
}

// Copied from the documentation of `char::is_ascii_punctuation`
fn is_ascii_punctuation(c: char) -> bool {
    match c {
        '\u{0021}'..='\u{002F}'
        | '\u{003A}'..='\u{0040}'
        | '\u{005B}'..='\u{0060}'
        | '\u{007B}'..='\u{007E}' => true,
        _ => false,
    }
}

impl<'a> fmt::Display for Demangle<'a> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match self.style {
            None => f.write_str(self.original)?,
            Some(DemangleStyle::Legacy(ref d)) => fmt::Display::fmt(d, f)?,
            Some(DemangleStyle::V0(ref d)) => fmt::Display::fmt(d, f)?,
        }
        f.write_str(self.suffix)
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
        ($a:expr, $b:expr) => {
            assert!(ok($a, $b))
        };
    }

    macro_rules! t_err {
        ($a:expr) => {
            assert!(ok_err($a))
        };
    }

    macro_rules! t_nohash {
        ($a:expr, $b:expr) => {{
            assert_eq!(format!("{:#}", super::demangle($a)), $b);
        }};
    }

    fn ok(sym: &str, expected: &str) -> bool {
        match super::try_demangle(sym) {
            Ok(s) => {
                if s.to_string() == expected {
                    true
                } else {
                    println!("\n{}\n!=\n{}\n", s, expected);
                    false
                }
            }
            Err(_) => {
                println!("error demangling");
                false
            }
        }
    }

    fn ok_err(sym: &str) -> bool {
        match super::try_demangle(sym) {
            Ok(_) => {
                println!("succeeded in demangling");
                false
            }
            Err(_) => super::demangle(sym).to_string() == sym,
        }
    }

    #[test]
    fn demangle() {
        t_err!("test");
        t!("_ZN4testE", "test");
        t_err!("_ZN4test");
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
        t!(
            "__ZN5alloc9allocator6Layout9for_value17h02a996811f781011E",
            "alloc::allocator::Layout::for_value::h02a996811f781011"
        );
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
        t!(
            "_ZN71_$LT$Test$u20$$u2b$$u20$$u27$static$u20$as$u20$foo..Bar$LT$Test$GT$$GT$3barE",
            "<Test + 'static as foo::Bar<Test>>::bar"
        );
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
        t_nohash!(
            "_ZN9backtrace3foo17hbb467fcdaea5d79bE.llvm.A5310EB9",
            "backtrace::foo"
        );
    }

    #[test]
    fn demangle_llvm_ir_branch_labels() {
        t!("_ZN4core5slice77_$LT$impl$u20$core..ops..index..IndexMut$LT$I$GT$$u20$for$u20$$u5b$T$u5d$$GT$9index_mut17haf9727c2edfbc47bE.exit.i.i", "core::slice::<impl core::ops::index::IndexMut<I> for [T]>::index_mut::haf9727c2edfbc47b.exit.i.i");
        t_nohash!("_ZN4core5slice77_$LT$impl$u20$core..ops..index..IndexMut$LT$I$GT$$u20$for$u20$$u5b$T$u5d$$GT$9index_mut17haf9727c2edfbc47bE.exit.i.i", "core::slice::<impl core::ops::index::IndexMut<I> for [T]>::index_mut.exit.i.i");
    }

    #[test]
    fn demangle_ignores_suffix_that_doesnt_look_like_a_symbol() {
        t_err!("_ZN3fooE.llvm moocow");
    }

    #[test]
    fn dont_panic() {
        super::demangle("_ZN2222222222222222222222EE").to_string();
        super::demangle("_ZN5*70527e27.ll34csaғE").to_string();
        super::demangle("_ZN5*70527a54.ll34_$b.1E").to_string();
        super::demangle(
            "\
             _ZN5~saäb4e\n\
             2734cOsbE\n\
             5usage20h)3\0\0\0\0\0\0\07e2734cOsbE\
             ",
        )
        .to_string();
    }

    #[test]
    fn invalid_no_chop() {
        t_err!("_ZNfooE");
    }

    #[test]
    fn handle_assoc_types() {
        t!("_ZN151_$LT$alloc..boxed..Box$LT$alloc..boxed..FnBox$LT$A$C$$u20$Output$u3d$R$GT$$u20$$u2b$$u20$$u27$a$GT$$u20$as$u20$core..ops..function..FnOnce$LT$A$GT$$GT$9call_once17h69e8f44b3723e1caE", "<alloc::boxed::Box<alloc::boxed::FnBox<A, Output=R> + 'a> as core::ops::function::FnOnce<A>>::call_once::h69e8f44b3723e1ca");
    }

    #[test]
    fn handle_bang() {
        t!(
            "_ZN88_$LT$core..result..Result$LT$$u21$$C$$u20$E$GT$$u20$as$u20$std..process..Termination$GT$6report17hfc41d0da4a40b3e8E",
            "<core::result::Result<!, E> as std::process::Termination>::report::hfc41d0da4a40b3e8"
        );
    }

    #[test]
    fn limit_recursion() {
        use std::fmt::Write;
        let mut s = String::new();
        assert!(write!(s, "{}", super::demangle("_RNvB_1a")).is_err());
    }

    #[test]
    fn limit_output() {
        use std::fmt::Write;
        let mut s = String::new();
        assert!(write!(
            s,
            "{}",
            super::demangle("RYFG_FGyyEvRYFF_EvRYFFEvERLB_B_B_ERLRjB_B_B_")
        )
        .is_err());
    }
}
