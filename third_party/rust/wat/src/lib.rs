//! A Rust parser for the [WebAssembly Text format][wat]
//!
//! This crate contains a stable interface to the parser for the [WAT][wat]
//! format of WebAssembly text files. The format parsed by this crate follows
//! the [online specification][wat].
//!
//! # Examples
//!
//! Parse an in-memory string:
//!
//! ```
//! # fn foo() -> wat::Result<()> {
//! let wat = r#"
//!     (module
//!         (func $foo)
//!
//!         (func (export "bar")
//!             call $foo
//!         )
//!     )
//! "#;
//!
//! let binary = wat::parse_str(wat)?;
//! // ...
//! # Ok(())
//! # }
//! ```
//!
//! Parse an on-disk file:
//!
//! ```
//! # fn foo() -> wat::Result<()> {
//! let binary = wat::parse_file("./foo.wat")?;
//! // ...
//! # Ok(())
//! # }
//! ```
//!
//! ## Evolution of the WAT Format
//!
//! WebAssembly, and the WAT format, are an evolving specification. Features are
//! added to WAT, WAT changes, and sometimes WAT breaks. The policy of this
//! crate is that it will always follow the [official specification][wat] for
//! WAT files.
//!
//! Future WebAssembly features will be accepted to this parser **and they will
//! not require a feature gate to opt-in**. All implemented WebAssembly features
//! will be enabled at all times. Using a future WebAssembly feature in the WAT
//! format may cause breakage because while specifications are in development
//! the WAT syntax (and/or binary encoding) will often change. This crate will
//! do its best to keep up with these proposals, but breaking textual changes
//! will be published as non-breaking semver changes to this crate.
//!
//! ## Stability
//!
//! This crate is intended to be a very stable shim over the `wast` crate
//! which is expected to be much more unstable. The `wast` crate contains
//! AST data structures for parsing `*.wat` files and they will evolve was the
//! WAT and WebAssembly specifications evolve over time.
//!
//! This crate is currently at version 1.x.y, and it is intended that it will
//! remain here for quite some time. Breaking changes to the WAT format will be
//! landed as a non-semver-breaking version change in this crate. This crate
//! will always follow the [official specification for WAT][wat].
//!
//! [wat]: http://webassembly.github.io/spec/core/text/index.html

#![deny(missing_docs)]

use std::borrow::Cow;
use std::fmt;
use std::path::{Path, PathBuf};
use std::str;
use wast::parser::{self, ParseBuffer};

/// Parses a file on disk as a [WebAssembly Text format][wat] file, or a binary
/// WebAssembly file
///
/// This function will read the bytes on disk and delegate them to the
/// [`parse_bytes`] function. For more information on the behavior of parsing
/// see [`parse_bytes`].
///
/// # Errors
///
/// For information about errors, see the [`parse_bytes`] documentation.
///
/// # Examples
///
/// ```
/// # fn foo() -> wat::Result<()> {
/// let binary = wat::parse_file("./foo.wat")?;
/// // ...
/// # Ok(())
/// # }
/// ```
///
/// [wat]: http://webassembly.github.io/spec/core/text/index.html
pub fn parse_file(file: impl AsRef<Path>) -> Result<Vec<u8>> {
    _parse_file(file.as_ref())
}

fn _parse_file(file: &Path) -> Result<Vec<u8>> {
    let contents = std::fs::read(file).map_err(|err| Error {
        kind: Box::new(ErrorKind::Io {
            err,
            file: Some(file.to_owned()),
        }),
    })?;
    match parse_bytes(&contents) {
        Ok(bytes) => Ok(bytes.into_owned()),
        Err(mut e) => {
            e.set_path(file);
            Err(e)
        }
    }
}

/// Parses in-memory bytes as either the [WebAssembly Text format][wat], or a
/// binary WebAssembly module.
///
/// This function will attempt to interpret the given bytes as one of two
/// options:
///
/// * A utf-8 string which is a `*.wat` file to be parsed.
/// * A binary WebAssembly file starting with `b"\0asm"`
///
/// If the input is a string then it will be parsed as `*.wat`, and then after
/// parsing it will be encoded back into a WebAssembly binary module. If the
/// input is a binary that starts with `b"\0asm"` it will be returned verbatim.
/// Everything that doesn't start with `b"\0asm"` will be parsed as a utf-8
/// `*.wat` file, returning errors as appropriate.
///
/// For more information about parsing wat files, see [`parse_str`].
///
/// # Errors
///
/// In addition to all of the errors that can be returned from [`parse_str`],
/// this function will also return an error if the input does not start with
/// `b"\0asm"` and is invalid utf-8. (failed to even try to call [`parse_str`]).
///
/// # Examples
///
/// ```
/// # fn foo() -> wat::Result<()> {
/// // Parsing bytes that are actually `*.wat` files
/// assert_eq!(&*wat::parse_bytes(b"(module)")?, b"\0asm\x01\0\0\0");
/// assert!(wat::parse_bytes(b"module").is_err());
/// assert!(wat::parse_bytes(b"binary\0file\0\that\0is\0not\0wat").is_err());
///
/// // Pass through binaries that look like real wasm files
/// assert_eq!(&*wat::parse_bytes(b"\0asm\x01\0\0\0")?, b"\0asm\x01\0\0\0");
/// # Ok(())
/// # }
/// ```
///
/// [wat]: http://webassembly.github.io/spec/core/text/index.html
pub fn parse_bytes(bytes: &[u8]) -> Result<Cow<'_, [u8]>> {
    if bytes.starts_with(b"\0asm") {
        return Ok(bytes.into());
    }
    match str::from_utf8(bytes) {
        Ok(s) => _parse_str(s).map(|s| s.into()),
        Err(_) => Err(Error {
            kind: Box::new(ErrorKind::Custom {
                msg: "input bytes aren't valid utf-8".to_string(),
                file: None,
            }),
        }),
    }
}

/// Parses an in-memory string as the [WebAssembly Text format][wat], returning
/// the file as a binary WebAssembly file.
///
/// This function is intended to be a stable convenience function for parsing a
/// wat file into a WebAssembly binary file. This is a high-level operation
/// which does not expose any parsing internals, for that you'll want to use the
/// `wast` crate.
///
/// # Errors
///
/// This function can fail for a number of reasons, including (but not limited
/// to):
///
/// * The `wat` input may fail to lex, such as having invalid tokens or syntax
/// * The `wat` input may fail to parse, such as having incorrect syntactical
///   structure
/// * The `wat` input may contain names that could not be resolved
///
/// # Examples
///
/// ```
/// # fn foo() -> wat::Result<()> {
/// assert_eq!(wat::parse_str("(module)")?, b"\0asm\x01\0\0\0");
/// assert!(wat::parse_str("module").is_err());
///
/// let wat = r#"
///     (module
///         (func $foo)
///
///         (func (export "bar")
///             call $foo
///         )
///     )
/// "#;
///
/// let binary = wat::parse_str(wat)?;
/// // ...
/// # Ok(())
/// # }
/// ```
///
/// [wat]: http://webassembly.github.io/spec/core/text/index.html
pub fn parse_str(wat: impl AsRef<str>) -> Result<Vec<u8>> {
    _parse_str(wat.as_ref())
}

fn _parse_str(wat: &str) -> Result<Vec<u8>> {
    let buf = ParseBuffer::new(&wat).map_err(|e| Error::cvt(e, wat))?;
    let mut ast = parser::parse::<wast::Wat>(&buf).map_err(|e| Error::cvt(e, wat))?;
    Ok(ast.module.encode().map_err(|e| Error::cvt(e, wat))?)
}

/// A convenience type definition for `Result` where the error is [`Error`]
pub type Result<T> = std::result::Result<T, Error>;

/// Errors from this crate related to parsing WAT files
///
/// An error can during example phases like:
///
/// * Lexing can fail if the document is syntactically invalid.
/// * A string may not be utf-8
/// * The syntactical structure of the wat file may be invalid
/// * The wat file may be semantically invalid such as having name resolution
///   failures
#[derive(Debug)]
pub struct Error {
    kind: Box<ErrorKind>,
}

#[derive(Debug)]
enum ErrorKind {
    Wast(wast::Error),
    Io {
        err: std::io::Error,
        file: Option<PathBuf>,
    },
    Custom {
        msg: String,
        file: Option<PathBuf>,
    },
}

impl Error {
    fn cvt<E: Into<wast::Error>>(e: E, contents: &str) -> Error {
        let mut err = e.into();
        err.set_text(contents);
        Error {
            kind: Box::new(ErrorKind::Wast(err)),
        }
    }

    /// To provide a more useful error this function can be used to set
    /// the file name that this error is associated with.
    ///
    /// The `file` here will be stored in this error and later rendered in the
    /// `Display` implementation.
    pub fn set_path<P: AsRef<Path>>(&mut self, file: P) {
        let file = file.as_ref();
        match &mut *self.kind {
            ErrorKind::Wast(e) => e.set_path(file),
            ErrorKind::Custom { file: f, .. } => *f = Some(file.to_owned()),
            ErrorKind::Io { file: f, .. } => *f = Some(file.to_owned()),
        }
    }
}

impl fmt::Display for Error {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        match &*self.kind {
            ErrorKind::Wast(err) => err.fmt(f),
            ErrorKind::Custom { msg, file, .. } => match file {
                Some(file) => {
                    write!(f, "failed to parse `{}`: {}", file.display(), msg)
                }
                None => msg.fmt(f),
            },
            ErrorKind::Io { err, file, .. } => match file {
                Some(file) => {
                    write!(f, "failed to read from `{}`: {}", file.display(), err)
                }
                None => err.fmt(f),
            },
        }
    }
}

impl std::error::Error for Error {
    fn source(&self) -> Option<&(dyn std::error::Error + 'static)> {
        match &*self.kind {
            ErrorKind::Wast(_) => None,
            ErrorKind::Custom { .. } => None,
            ErrorKind::Io { err, .. } => Some(err),
        }
    }
}

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn test_set_path() {
        let mut e = parse_bytes(&[0xFF]).unwrap_err();
        e.set_path("foo");
        assert_eq!(
            e.to_string(),
            "failed to parse `foo`: input bytes aren't valid utf-8"
        );

        let e = parse_file("_does_not_exist_").unwrap_err();
        assert!(e
            .to_string()
            .starts_with("failed to read from `_does_not_exist_`: "));

        let mut e = parse_bytes("()".as_bytes()).unwrap_err();
        e.set_path("foo");
        assert_eq!(
            e.to_string(),
            "expected valid module field\n     --> foo:1:2\n      |\n    1 | ()\n      |  ^"
        );
    }
}
