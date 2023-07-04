//! Definitions of name-related helpers and newtypes, primarily for the
//! component model.

use crate::{ComponentExternName, Result};
use semver::Version;
use std::borrow::Borrow;
use std::fmt;
use std::hash::{Hash, Hasher};
use std::ops::Deref;

/// Represents a kebab string slice used in validation.
///
/// This is a wrapper around `str` that ensures the slice is
/// a valid kebab case string according to the component model
/// specification.
///
/// It also provides an equality and hashing implementation
/// that ignores ASCII case.
#[derive(Debug, Eq)]
#[repr(transparent)]
pub struct KebabStr(str);

impl KebabStr {
    /// Creates a new kebab string slice.
    ///
    /// Returns `None` if the given string is not a valid kebab string.
    pub fn new<'a>(s: impl AsRef<str> + 'a) -> Option<&'a Self> {
        let s = Self::new_unchecked(s);
        if s.is_kebab_case() {
            Some(s)
        } else {
            None
        }
    }

    pub(crate) fn new_unchecked<'a>(s: impl AsRef<str> + 'a) -> &'a Self {
        // Safety: `KebabStr` is a transparent wrapper around `str`
        // Therefore transmuting `&str` to `&KebabStr` is safe.
        unsafe { std::mem::transmute::<_, &Self>(s.as_ref()) }
    }

    /// Gets the underlying string slice.
    pub fn as_str(&self) -> &str {
        &self.0
    }

    /// Converts the slice to an owned string.
    pub fn to_kebab_string(&self) -> KebabString {
        KebabString(self.to_string())
    }

    fn is_kebab_case(&self) -> bool {
        let mut lower = false;
        let mut upper = false;
        for c in self.chars() {
            match c {
                'a'..='z' if !lower && !upper => lower = true,
                'A'..='Z' if !lower && !upper => upper = true,
                'a'..='z' if lower => {}
                'A'..='Z' if upper => {}
                '0'..='9' if lower || upper => {}
                '-' if lower || upper => {
                    lower = false;
                    upper = false;
                }
                _ => return false,
            }
        }

        !self.is_empty() && !self.ends_with('-')
    }
}

impl Deref for KebabStr {
    type Target = str;

    fn deref(&self) -> &str {
        self.as_str()
    }
}

impl PartialEq for KebabStr {
    fn eq(&self, other: &Self) -> bool {
        if self.len() != other.len() {
            return false;
        }

        self.chars()
            .zip(other.chars())
            .all(|(a, b)| a.to_ascii_lowercase() == b.to_ascii_lowercase())
    }
}

impl PartialEq<KebabString> for KebabStr {
    fn eq(&self, other: &KebabString) -> bool {
        self.eq(other.as_kebab_str())
    }
}

impl Hash for KebabStr {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.len().hash(state);

        for b in self.chars() {
            b.to_ascii_lowercase().hash(state);
        }
    }
}

impl fmt::Display for KebabStr {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        (self as &str).fmt(f)
    }
}

impl ToOwned for KebabStr {
    type Owned = KebabString;

    fn to_owned(&self) -> Self::Owned {
        self.to_kebab_string()
    }
}

/// Represents an owned kebab string for validation.
///
/// This is a wrapper around `String` that ensures the string is
/// a valid kebab case string according to the component model
/// specification.
///
/// It also provides an equality and hashing implementation
/// that ignores ASCII case.
#[derive(Debug, Clone, Eq)]
pub struct KebabString(String);

impl KebabString {
    /// Creates a new kebab string.
    ///
    /// Returns `None` if the given string is not a valid kebab string.
    pub fn new(s: impl Into<String>) -> Option<Self> {
        let s = s.into();
        if KebabStr::new(&s).is_some() {
            Some(Self(s))
        } else {
            None
        }
    }

    /// Gets the underlying string.
    pub fn as_str(&self) -> &str {
        self.0.as_str()
    }

    /// Converts the kebab string to a kebab string slice.
    pub fn as_kebab_str(&self) -> &KebabStr {
        // Safety: internal string is always valid kebab-case
        KebabStr::new_unchecked(self.as_str())
    }
}

impl Deref for KebabString {
    type Target = KebabStr;

    fn deref(&self) -> &Self::Target {
        self.as_kebab_str()
    }
}

impl Borrow<KebabStr> for KebabString {
    fn borrow(&self) -> &KebabStr {
        self.as_kebab_str()
    }
}

impl PartialEq for KebabString {
    fn eq(&self, other: &Self) -> bool {
        self.as_kebab_str().eq(other.as_kebab_str())
    }
}

impl PartialEq<KebabStr> for KebabString {
    fn eq(&self, other: &KebabStr) -> bool {
        self.as_kebab_str().eq(other)
    }
}

impl Hash for KebabString {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.as_kebab_str().hash(state)
    }
}

impl fmt::Display for KebabString {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.as_kebab_str().fmt(f)
    }
}

impl From<KebabString> for String {
    fn from(s: KebabString) -> String {
        s.0
    }
}

/// A "kebab name" in the component model which is backed by `T`, which defaults
/// to `String`.
///
/// This name can be either:
///
/// * a `KebabStr`: `a-b-c`
/// * a method name : `[method]a-b.c-d`
/// * a static method name : `[static]a-b.c-d`
/// * a constructor: `[constructor]a-b`
///
/// # Equality and hashing
///
/// Note that this type the `Method` and `Static` variants are considered equal
/// and hash to the same value. This enables disallowing clashes between the two
/// where method name overlap cannot happen.
#[derive(Clone)]
pub struct KebabName {
    raw: String,
    parsed: ParsedKebabName,
}

#[derive(Copy, Clone)]
enum ParsedKebabName {
    Normal,
    Constructor,
    Method {
        dot: u32,
    },
    Static {
        dot: u32,
    },
    Id {
        colon: u32,
        slash: u32,
        at: Option<u32>,
    },
}

/// Created via [`KebabName::kind`] and classifies a name.
#[derive(Debug, Clone)]
pub enum KebabNameKind<'a> {
    /// `a-b-c`
    Normal(&'a KebabStr),
    /// `[constructor]a-b`
    Constructor(&'a KebabStr),
    /// `[method]a-b.c-d`
    #[allow(missing_docs)]
    Method {
        resource: &'a KebabStr,
        name: &'a KebabStr,
    },
    /// `[static]a-b.c-d`
    #[allow(missing_docs)]
    Static {
        resource: &'a KebabStr,
        name: &'a KebabStr,
    },
    /// `wasi:http/types@2.0`
    #[allow(missing_docs)]
    Id {
        namespace: &'a KebabStr,
        package: &'a KebabStr,
        interface: &'a KebabStr,
        version: Option<Version>,
    },
}

const CONSTRUCTOR: &str = "[constructor]";
const METHOD: &str = "[method]";
const STATIC: &str = "[static]";

impl KebabName {
    /// Attempts to parse `name` as a kebab name, returning `None` if it's not
    /// valid.
    pub fn new(name: ComponentExternName<'_>, offset: usize) -> Result<KebabName> {
        let validate_kebab = |s: &str| {
            if KebabStr::new(s).is_none() {
                bail!(offset, "`{s}` is not in kebab case")
            } else {
                Ok(())
            }
        };
        let find = |s: &str, c: char| match s.find(c) {
            Some(i) => Ok(i),
            None => bail!(offset, "failed to find `{c}` character"),
        };
        let parsed = match name {
            ComponentExternName::Kebab(s) => {
                if let Some(s) = s.strip_prefix(CONSTRUCTOR) {
                    validate_kebab(s)?;
                    ParsedKebabName::Constructor
                } else if let Some(s) = s.strip_prefix(METHOD) {
                    let dot = find(s, '.')?;
                    validate_kebab(&s[..dot])?;
                    validate_kebab(&s[dot + 1..])?;
                    ParsedKebabName::Method { dot: dot as u32 }
                } else if let Some(s) = s.strip_prefix(STATIC) {
                    let dot = find(s, '.')?;
                    validate_kebab(&s[..dot])?;
                    validate_kebab(&s[dot + 1..])?;
                    ParsedKebabName::Static { dot: dot as u32 }
                } else {
                    validate_kebab(s)?;
                    ParsedKebabName::Normal
                }
            }
            ComponentExternName::Interface(s) => {
                let colon = find(s, ':')?;
                validate_kebab(&s[..colon])?;
                let slash = find(s, '/')?;
                let at = s[slash..].find('@').map(|i| i + slash);
                validate_kebab(&s[colon + 1..slash])?;
                validate_kebab(&s[slash + 1..at.unwrap_or(s.len())])?;
                if let Some(at) = at {
                    let version = &s[at + 1..];
                    if let Err(e) = version.parse::<Version>() {
                        bail!(offset, "failed to parse version: {e}")
                    }
                }
                ParsedKebabName::Id {
                    colon: colon as u32,
                    slash: slash as u32,
                    at: at.map(|i| i as u32),
                }
            }
        };
        Ok(KebabName {
            raw: name.as_str().to_string(),
            parsed,
        })
    }

    /// Returns the [`KebabNameKind`] corresponding to this name.
    pub fn kind(&self) -> KebabNameKind<'_> {
        match self.parsed {
            ParsedKebabName::Normal => KebabNameKind::Normal(KebabStr::new_unchecked(&self.raw)),
            ParsedKebabName::Constructor => {
                let kebab = &self.raw[CONSTRUCTOR.len()..];
                KebabNameKind::Constructor(KebabStr::new_unchecked(kebab))
            }
            ParsedKebabName::Method { dot } => {
                let dotted = &self.raw[METHOD.len()..];
                let resource = KebabStr::new_unchecked(&dotted[..dot as usize]);
                let name = KebabStr::new_unchecked(&dotted[dot as usize + 1..]);
                KebabNameKind::Method { resource, name }
            }
            ParsedKebabName::Static { dot } => {
                let dotted = &self.raw[METHOD.len()..];
                let resource = KebabStr::new_unchecked(&dotted[..dot as usize]);
                let name = KebabStr::new_unchecked(&dotted[dot as usize + 1..]);
                KebabNameKind::Static { resource, name }
            }
            ParsedKebabName::Id { colon, slash, at } => {
                let colon = colon as usize;
                let slash = slash as usize;
                let at = at.map(|i| i as usize);
                let namespace = KebabStr::new_unchecked(&self.raw[..colon]);
                let package = KebabStr::new_unchecked(&self.raw[colon + 1..slash]);
                let interface =
                    KebabStr::new_unchecked(&self.raw[slash + 1..at.unwrap_or(self.raw.len())]);
                let version = at.map(|i| Version::parse(&self.raw[i + 1..]).unwrap());
                KebabNameKind::Id {
                    namespace,
                    package,
                    interface,
                    version,
                }
            }
        }
    }

    /// Returns the raw underlying name as a string.
    pub fn as_str(&self) -> &str {
        &self.raw
    }
}

impl From<KebabName> for String {
    fn from(name: KebabName) -> String {
        name.raw
    }
}

impl Hash for KebabName {
    fn hash<H: Hasher>(&self, hasher: &mut H) {
        self.kind().hash(hasher)
    }
}

impl PartialEq for KebabName {
    fn eq(&self, other: &KebabName) -> bool {
        self.kind().eq(&other.kind())
    }
}

impl Eq for KebabName {}

impl fmt::Display for KebabName {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.raw.fmt(f)
    }
}

impl fmt::Debug for KebabName {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        self.raw.fmt(f)
    }
}

impl Hash for KebabNameKind<'_> {
    fn hash<H: Hasher>(&self, hasher: &mut H) {
        match self {
            KebabNameKind::Normal(name) => {
                hasher.write_u8(0);
                name.hash(hasher);
            }
            KebabNameKind::Constructor(name) => {
                hasher.write_u8(1);
                name.hash(hasher);
            }
            // for hashing method == static
            KebabNameKind::Method { resource, name } | KebabNameKind::Static { resource, name } => {
                hasher.write_u8(2);
                resource.hash(hasher);
                name.hash(hasher);
            }
            KebabNameKind::Id {
                namespace,
                package,
                interface,
                version,
            } => {
                hasher.write_u8(3);
                namespace.hash(hasher);
                package.hash(hasher);
                interface.hash(hasher);
                version.hash(hasher);
            }
        }
    }
}

impl PartialEq for KebabNameKind<'_> {
    fn eq(&self, other: &KebabNameKind<'_>) -> bool {
        match (self, other) {
            (KebabNameKind::Normal(a), KebabNameKind::Normal(b)) => a == b,
            (KebabNameKind::Normal(_), _) => false,
            (KebabNameKind::Constructor(a), KebabNameKind::Constructor(b)) => a == b,
            (KebabNameKind::Constructor(_), _) => false,

            // method == static for the purposes of hashing so equate them here
            // as well.
            (
                KebabNameKind::Method {
                    resource: ar,
                    name: an,
                },
                KebabNameKind::Method {
                    resource: br,
                    name: bn,
                },
            )
            | (
                KebabNameKind::Static {
                    resource: ar,
                    name: an,
                },
                KebabNameKind::Static {
                    resource: br,
                    name: bn,
                },
            )
            | (
                KebabNameKind::Method {
                    resource: ar,
                    name: an,
                },
                KebabNameKind::Static {
                    resource: br,
                    name: bn,
                },
            )
            | (
                KebabNameKind::Static {
                    resource: ar,
                    name: an,
                },
                KebabNameKind::Method {
                    resource: br,
                    name: bn,
                },
            ) => ar == br && an == bn,

            (KebabNameKind::Method { .. }, _) => false,
            (KebabNameKind::Static { .. }, _) => false,

            (
                KebabNameKind::Id {
                    namespace: an,
                    package: ap,
                    interface: ai,
                    version: av,
                },
                KebabNameKind::Id {
                    namespace: bn,
                    package: bp,
                    interface: bi,
                    version: bv,
                },
            ) => an == bn && ap == bp && ai == bi && av == bv,
            (KebabNameKind::Id { .. }, _) => false,
        }
    }
}

impl Eq for KebabNameKind<'_> {}

#[cfg(test)]
mod tests {
    use super::*;
    use std::collections::HashSet;

    fn parse_kebab_name(s: &str) -> Option<KebabName> {
        KebabName::new(ComponentExternName::Kebab(s), 0).ok()
    }

    #[test]
    fn kebab_smoke() {
        assert!(KebabStr::new("").is_none());
        assert!(KebabStr::new("a").is_some());
        assert!(KebabStr::new("aB").is_none());
        assert!(KebabStr::new("a-B").is_some());
        assert!(KebabStr::new("a-").is_none());
        assert!(KebabStr::new("-").is_none());
        assert!(KebabStr::new("¶").is_none());
        assert!(KebabStr::new("0").is_none());
        assert!(KebabStr::new("a0").is_some());
        assert!(KebabStr::new("a-0").is_none());
    }

    #[test]
    fn name_smoke() {
        assert!(parse_kebab_name("a").is_some());
        assert!(parse_kebab_name("[foo]a").is_none());
        assert!(parse_kebab_name("[constructor]a").is_some());
        assert!(parse_kebab_name("[method]a").is_none());
        assert!(parse_kebab_name("[method]a.b").is_some());
        assert!(parse_kebab_name("[method]a.b.c").is_none());
        assert!(parse_kebab_name("[static]a.b").is_some());
        assert!(parse_kebab_name("[static]a").is_none());
    }

    #[test]
    fn name_equality() {
        assert_eq!(parse_kebab_name("a"), parse_kebab_name("a"));
        assert_ne!(parse_kebab_name("a"), parse_kebab_name("b"));
        assert_eq!(
            parse_kebab_name("[constructor]a"),
            parse_kebab_name("[constructor]a")
        );
        assert_ne!(
            parse_kebab_name("[constructor]a"),
            parse_kebab_name("[constructor]b")
        );
        assert_eq!(
            parse_kebab_name("[method]a.b"),
            parse_kebab_name("[method]a.b")
        );
        assert_ne!(
            parse_kebab_name("[method]a.b"),
            parse_kebab_name("[method]b.b")
        );
        assert_eq!(
            parse_kebab_name("[static]a.b"),
            parse_kebab_name("[static]a.b")
        );
        assert_ne!(
            parse_kebab_name("[static]a.b"),
            parse_kebab_name("[static]b.b")
        );

        assert_eq!(
            parse_kebab_name("[static]a.b"),
            parse_kebab_name("[method]a.b")
        );
        assert_eq!(
            parse_kebab_name("[method]a.b"),
            parse_kebab_name("[static]a.b")
        );

        assert_ne!(
            parse_kebab_name("[method]b.b"),
            parse_kebab_name("[static]a.b")
        );

        let mut s = HashSet::new();
        assert!(s.insert(parse_kebab_name("a")));
        assert!(s.insert(parse_kebab_name("[constructor]a")));
        assert!(s.insert(parse_kebab_name("[method]a.b")));
        assert!(!s.insert(parse_kebab_name("[static]a.b")));
        assert!(s.insert(parse_kebab_name("[static]b.b")));
    }
}
