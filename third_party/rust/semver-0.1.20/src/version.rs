// Copyright 2012-2013 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! The `version` module gives you tools to create and compare SemVer-compliant
//! versions.

use std::ascii::AsciiExt;
use std::cmp::{self, Ordering};
use std::error::Error;
use std::fmt;
use std::hash;

use self::Identifier::{Numeric, AlphaNumeric};
use self::ParseError::{GenericFailure, IncorrectParse, NonAsciiIdentifier};

/// An identifier in the pre-release or build metadata.
///
/// See sections 9 and 10 of the spec for more about pre-release identifers and
/// build metadata.
#[derive(Clone, PartialEq, Eq, PartialOrd, Ord, Hash, Debug)]
pub enum Identifier {
    /// An identifier that's solely numbers.
    Numeric(u64),
    /// An identifier with letters and numbers.
    AlphaNumeric(String)
}

impl fmt::Display for Identifier {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            Numeric(ref n) => fmt::Display::fmt(n, f),
            AlphaNumeric(ref s) => fmt::Display::fmt(s, f),
        }
    }
}


/// Represents a version number conforming to the semantic versioning scheme.
#[derive(Clone, Eq, Debug)]
pub struct Version {
    /// The major version, to be incremented on incompatible changes.
    pub major: u64,
    /// The minor version, to be incremented when functionality is added in a
    /// backwards-compatible manner.
    pub minor: u64,
    /// The patch version, to be incremented when backwards-compatible bug
    /// fixes are made.
    pub patch: u64,
    /// The pre-release version identifier, if one exists.
    pub pre: Vec<Identifier>,
    /// The build metadata, ignored when determining version precedence.
    pub build: Vec<Identifier>,
}

/// A `ParseError` is returned as the `Err` side of a `Result` when a version is
/// attempted to be parsed.
#[derive(Clone,PartialEq,Debug,PartialOrd)]
pub enum ParseError {
    /// All identifiers must be ASCII.
    NonAsciiIdentifier,
    /// The version was mis-parsed.
    IncorrectParse(Version, String),
    /// Any other failure.
    GenericFailure,
}

impl Version {
    /// Parse a string into a semver object.
    pub fn parse(s: &str) -> Result<Version, ParseError> {
        if !s.is_ascii() {
            return Err(NonAsciiIdentifier)
        }
        let s = s.trim();
        let v = parse_iter(&mut s.chars());
        match v {
            Some(v) => {
                if v.to_string() == s {
                    Ok(v)
                } else {
                    Err(IncorrectParse(v, s.to_string()))
                }
            }
            None => Err(GenericFailure)
        }
    }

    /// Clears the build metadata
    fn clear_metadata(&mut self) {
        self.build = Vec::new();
        self.pre = Vec::new();
    }

    /// Increments the patch number for this Version (Must be mutable)
    pub fn increment_patch(&mut self) {
        self.patch += 1;
        self.clear_metadata();
    }

    /// Increments the minor version number for this Version (Must be mutable)
    ///
    /// As instructed by section 7 of the spec, the patch number is reset to 0.
    pub fn increment_minor(&mut self) {
        self.minor += 1;
        self.patch = 0;
        self.clear_metadata();
    }

    /// Increments the major version number for this Version (Must be mutable)
    ///
    /// As instructed by section 8 of the spec, the minor and patch numbers are
    /// reset to 0
    pub fn increment_major(&mut self) {
        self.major += 1;
        self.minor = 0;
        self.patch = 0;
        self.clear_metadata();
    }
}


impl fmt::Display for Version {
    #[inline]
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        try!(write!(f, "{}.{}.{}", self.major, self.minor, self.patch));
        if !self.pre.is_empty() {
            try!(write!(f, "-"));
            for (i, x) in self.pre.iter().enumerate() {
                if i != 0 { try!(write!(f, ".")) };
                try!(write!(f, "{}", x));
            }
        }
        if !self.build.is_empty() {
            try!(write!(f, "+"));
            for (i, x) in self.build.iter().enumerate() {
                if i != 0 { try!(write!(f, ".")) };
                try!(write!(f, "{}", x));
            }
        }
        Ok(())
    }
}

impl cmp::PartialEq for Version {
    #[inline]
    fn eq(&self, other: &Version) -> bool {
        // We should ignore build metadata here, otherwise versions v1 and v2
        // can exist such that !(v1 < v2) && !(v1 > v2) && v1 != v2, which
        // violate strict total ordering rules.
        self.major == other.major &&
            self.minor == other.minor &&
            self.patch == other.patch &&
            self.pre == other.pre
    }
}

impl cmp::PartialOrd for Version {
    fn partial_cmp(&self, other: &Version) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl cmp::Ord for Version {
    fn cmp(&self, other: &Version) -> Ordering {
        match self.major.cmp(&other.major) {
            Ordering::Equal => {}
            r => return r,
        }

        match self.minor.cmp(&other.minor) {
            Ordering::Equal => {}
            r => return r,
        }

        match self.patch.cmp(&other.patch) {
            Ordering::Equal => {}
            r => return r,
        }

        // NB: semver spec says 0.0.0-pre < 0.0.0
        // but the version of ord defined for vec
        // says that [] < [pre] so we alter it here
        match (self.pre.len(), other.pre.len()) {
            (0, 0) => Ordering::Equal,
            (0, _) => Ordering::Greater,
            (_, 0) => Ordering::Less,
            (_, _) => self.pre.cmp(&other.pre)
        }
    }
}

impl Error for ParseError {
    fn description(&self) -> &str {
        match *self {
            ParseError::NonAsciiIdentifier
                => "identifiers can only contain ascii characters",
            ParseError::GenericFailure
                | ParseError::IncorrectParse(..)
                => "failed to parse semver from string",
        }
    }
}

impl fmt::Display for ParseError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            ParseError::NonAsciiIdentifier => {
                write!(f, "{}", self.description())
            }
            ParseError::GenericFailure => {
                write!(f, "{}", self.description())
            }
            ParseError::IncorrectParse(ref a, ref b) => {
                write!(f, "{}: {} could not be parsed from {:?}", self.description(), a, b)
            }
        }
    }
}

impl hash::Hash for Version {
    fn hash<H: hash::Hasher>(&self, into: &mut H) {
        self.major.hash(into);
        self.minor.hash(into);
        self.patch.hash(into);
        self.pre.hash(into);
    }
}

fn take_nonempty_prefix<T, F>(rdr: &mut T, pred: F) -> (String, Option<char>) where
    T: Iterator<Item = char>,
    F: Fn(char) -> bool
{
    let mut buf = String::new();
    let mut ch = rdr.next();
    loop {
        match ch {
            None => break,
            Some(c) if !pred(c) => break,
            Some(c) => {
                buf.push(c);
                ch = rdr.next();
            }
        }
    }
    (buf, ch)
}

fn take_num<T: Iterator<Item=char>>(rdr: &mut T) -> Option<(u64, Option<char>)> {
    let (s, ch) = take_nonempty_prefix(rdr, |c| c.is_digit(10));
    match s.parse::<u64>().ok() {
        None => None,
        Some(i) => Some((i, ch))
    }
}

fn take_ident<T: Iterator<Item=char>>(rdr: &mut T) -> Option<(Identifier, Option<char>)> {
    let (s,ch) = take_nonempty_prefix(rdr, |c| c.is_alphanumeric());

    if s.len() == 0 {
        None
    } else if s.chars().all(|c| c.is_digit(10)) && s.chars().next() != Some('0') {
        match s.parse::<u64>().ok() {
            None => None,
            Some(i) => Some((Numeric(i), ch))
        }
    } else {
        Some((AlphaNumeric(s), ch))
    }
}

fn expect(ch: Option<char>, c: char) -> Option<()> {
    if ch != Some(c) {
        None
    } else {
        Some(())
    }
}

fn parse_iter<T: Iterator<Item=char>>(rdr: &mut T) -> Option<Version> {
    let maybe_vers = take_num(rdr).and_then(|(major, ch)| {
        expect(ch, '.').and_then(|_| Some(major))
    }).and_then(|major| {
        take_num(rdr).and_then(|(minor, ch)| {
            expect(ch, '.').and_then(|_| Some((major, minor)))
        })
    }).and_then(|(major, minor)| {
        take_num(rdr).and_then(|(patch, ch)| {
           Some((major, minor, patch, ch))
        })
    });

    let (major, minor, patch, ch) = match maybe_vers {
        Some((a, b, c, d)) => (a, b, c, d),
        None => return None
    };

    let mut pre = vec!();
    let mut build = vec!();

    let mut ch = ch;
    if ch == Some('-') {
        loop {
            let (id, c) = match take_ident(rdr) {
                Some((id, c)) => (id, c),
                None => return None
            };
            pre.push(id);
            ch = c;
            if ch != Some('.') { break; }
        }
    }

    if ch == Some('+') {
        loop {
            let (id, c) = match take_ident(rdr) {
                Some((id, c)) => (id, c),
                None => return None
            };
            build.push(id);
            ch = c;
            if ch != Some('.') { break; }
        }
    }

    Some(Version {
        major: major,
        minor: minor,
        patch: patch,
        pre: pre,
        build: build,
    })
}

#[cfg(test)]
mod test {
    use super::{Version};
    use super::ParseError::{IncorrectParse, GenericFailure};
    use super::Identifier::{AlphaNumeric, Numeric};

    #[test]
    fn test_parse() {
        assert_eq!(Version::parse(""), Err(GenericFailure));
        assert_eq!(Version::parse("  "), Err(GenericFailure));
        assert_eq!(Version::parse("1"), Err(GenericFailure));
        assert_eq!(Version::parse("1.2"), Err(GenericFailure));
        assert_eq!(Version::parse("1.2.3-"), Err(GenericFailure));
        assert_eq!(Version::parse("a.b.c"), Err(GenericFailure));

        let version = Version {
            major: 1,
            minor: 2,
            patch: 3,
            pre: vec!(),
            build: vec!(),
        };
        let error = Err(IncorrectParse(version, "1.2.3 abc".to_string()));
        assert_eq!(Version::parse("1.2.3 abc"), error);

        assert!(Version::parse("1.2.3") == Ok(Version {
            major: 1,
            minor: 2,
            patch: 3,
            pre: vec!(),
            build: vec!(),
        }));
        assert!(Version::parse("  1.2.3  ") == Ok(Version {
            major: 1,
            minor: 2,
            patch: 3,
            pre: vec!(),
            build: vec!(),
        }));
        assert!(Version::parse("1.2.3-alpha1") == Ok(Version {
            major: 1,
            minor: 2,
            patch: 3,
            pre: vec!(AlphaNumeric("alpha1".to_string())),
            build: vec!(),
        }));
        assert!(Version::parse("  1.2.3-alpha1  ") == Ok(Version {
            major: 1,
            minor: 2,
            patch: 3,
            pre: vec!(AlphaNumeric("alpha1".to_string())),
            build: vec!()
        }));
        assert!(Version::parse("1.2.3+build5") == Ok(Version {
            major: 1,
            minor: 2,
            patch: 3,
            pre: vec!(),
            build: vec!(AlphaNumeric("build5".to_string()))
        }));
        assert!(Version::parse("  1.2.3+build5  ") == Ok(Version {
            major: 1,
            minor: 2,
            patch: 3,
            pre: vec!(),
            build: vec!(AlphaNumeric("build5".to_string()))
        }));
        assert!(Version::parse("1.2.3-alpha1+build5") == Ok(Version {
            major: 1,
            minor: 2,
            patch: 3,
            pre: vec!(AlphaNumeric("alpha1".to_string())),
            build: vec!(AlphaNumeric("build5".to_string()))
        }));
        assert!(Version::parse("  1.2.3-alpha1+build5  ") == Ok(Version {
            major: 1,
            minor: 2,
            patch: 3,
            pre: vec!(AlphaNumeric("alpha1".to_string())),
            build: vec!(AlphaNumeric("build5".to_string()))
        }));
        assert!(Version::parse("1.2.3-1.alpha1.9+build5.7.3aedf  ") == Ok(Version {
            major: 1,
            minor: 2,
            patch: 3,
            pre: vec!(Numeric(1),AlphaNumeric("alpha1".to_string()),Numeric(9)),
            build: vec!(AlphaNumeric("build5".to_string()),
                     Numeric(7),
                     AlphaNumeric("3aedf".to_string()))
        }));
        assert_eq!(Version::parse("0.4.0-beta.1+0851523"), Ok(Version {
            major: 0,
            minor: 4,
            patch: 0,
            pre: vec![AlphaNumeric("beta".to_string()), Numeric(1)],
            build: vec![AlphaNumeric("0851523".to_string())],
        }));

    }

    #[test]
    fn test_increment_patch() {
        let mut buggy_release = Version::parse("0.1.0").unwrap();
        buggy_release.increment_patch();
        assert_eq!(buggy_release, Version::parse("0.1.1").unwrap());
    }

    #[test]
    fn test_increment_minor() {
        let mut feature_release = Version::parse("1.4.6").unwrap();
        feature_release.increment_minor();
        assert_eq!(feature_release, Version::parse("1.5.0").unwrap());
    }

    #[test]
    fn test_increment_major() {
        let mut chrome_release = Version::parse("46.1.246773").unwrap();
        chrome_release.increment_major();
        assert_eq!(chrome_release, Version::parse("47.0.0").unwrap());
    }

    #[test]
    fn test_increment_keep_prerelease() {
        let mut release = Version::parse("1.0.0-alpha").unwrap();
        release.increment_patch();

        assert_eq!(release, Version::parse("1.0.1").unwrap());

        release.increment_minor();

        assert_eq!(release, Version::parse("1.1.0").unwrap());

        release.increment_major();

        assert_eq!(release, Version::parse("2.0.0").unwrap());
    }


    #[test]
    fn test_increment_clear_metadata() {
        let mut release = Version::parse("1.0.0+4442").unwrap();
        release.increment_patch();

        assert_eq!(release, Version::parse("1.0.1").unwrap());
        release = Version::parse("1.0.1+hello").unwrap();

        release.increment_minor();

        assert_eq!(release, Version::parse("1.1.0").unwrap());
        release = Version::parse("1.1.3747+hello").unwrap();

        release.increment_major();

        assert_eq!(release, Version::parse("2.0.0").unwrap());
    }

    #[test]
    fn test_eq() {
        assert_eq!(Version::parse("1.2.3"), Version::parse("1.2.3"));
        assert_eq!(Version::parse("1.2.3-alpha1"), Version::parse("1.2.3-alpha1"));
        assert_eq!(Version::parse("1.2.3+build.42"), Version::parse("1.2.3+build.42"));
        assert_eq!(Version::parse("1.2.3-alpha1+42"), Version::parse("1.2.3-alpha1+42"));
        assert_eq!(Version::parse("1.2.3+23"), Version::parse("1.2.3+42"));
    }

    #[test]
    fn test_ne() {
        assert!(Version::parse("0.0.0")       != Version::parse("0.0.1"));
        assert!(Version::parse("0.0.0")       != Version::parse("0.1.0"));
        assert!(Version::parse("0.0.0")       != Version::parse("1.0.0"));
        assert!(Version::parse("1.2.3-alpha") != Version::parse("1.2.3-beta"));
    }

    #[test]
    fn test_show() {
        assert_eq!(format!("{}", Version::parse("1.2.3").unwrap()),
                   "1.2.3".to_string());
        assert_eq!(format!("{}", Version::parse("1.2.3-alpha1").unwrap()),
                   "1.2.3-alpha1".to_string());
        assert_eq!(format!("{}", Version::parse("1.2.3+build.42").unwrap()),
                   "1.2.3+build.42".to_string());
        assert_eq!(format!("{}", Version::parse("1.2.3-alpha1+42").unwrap()),
                   "1.2.3-alpha1+42".to_string());
    }

    #[test]
    fn test_to_string() {
        assert_eq!(Version::parse("1.2.3").unwrap().to_string(), "1.2.3".to_string());
        assert_eq!(Version::parse("1.2.3-alpha1").unwrap().to_string(), "1.2.3-alpha1".to_string());
        assert_eq!(Version::parse("1.2.3+build.42").unwrap().to_string(), "1.2.3+build.42".to_string());
        assert_eq!(Version::parse("1.2.3-alpha1+42").unwrap().to_string(), "1.2.3-alpha1+42".to_string());
    }

    #[test]
    fn test_lt() {
        assert!(Version::parse("0.0.0")          < Version::parse("1.2.3-alpha2"));
        assert!(Version::parse("1.0.0")          < Version::parse("1.2.3-alpha2"));
        assert!(Version::parse("1.2.0")          < Version::parse("1.2.3-alpha2"));
        assert!(Version::parse("1.2.3-alpha1")   < Version::parse("1.2.3"));
        assert!(Version::parse("1.2.3-alpha1")   < Version::parse("1.2.3-alpha2"));
        assert!(!(Version::parse("1.2.3-alpha2") < Version::parse("1.2.3-alpha2")));
        assert!(!(Version::parse("1.2.3+23")     < Version::parse("1.2.3+42")));
    }

    #[test]
    fn test_le() {
        assert!(Version::parse("0.0.0")        <= Version::parse("1.2.3-alpha2"));
        assert!(Version::parse("1.0.0")        <= Version::parse("1.2.3-alpha2"));
        assert!(Version::parse("1.2.0")        <= Version::parse("1.2.3-alpha2"));
        assert!(Version::parse("1.2.3-alpha1") <= Version::parse("1.2.3-alpha2"));
        assert!(Version::parse("1.2.3-alpha2") <= Version::parse("1.2.3-alpha2"));
        assert!(Version::parse("1.2.3+23")     <= Version::parse("1.2.3+42"));
    }

    #[test]
    fn test_gt() {
        assert!(Version::parse("1.2.3-alpha2")   > Version::parse("0.0.0"));
        assert!(Version::parse("1.2.3-alpha2")   > Version::parse("1.0.0"));
        assert!(Version::parse("1.2.3-alpha2")   > Version::parse("1.2.0"));
        assert!(Version::parse("1.2.3-alpha2")   > Version::parse("1.2.3-alpha1"));
        assert!(Version::parse("1.2.3")          > Version::parse("1.2.3-alpha2"));
        assert!(!(Version::parse("1.2.3-alpha2") > Version::parse("1.2.3-alpha2")));
        assert!(!(Version::parse("1.2.3+23")     > Version::parse("1.2.3+42")));
    }

    #[test]
    fn test_ge() {
        assert!(Version::parse("1.2.3-alpha2") >= Version::parse("0.0.0"));
        assert!(Version::parse("1.2.3-alpha2") >= Version::parse("1.0.0"));
        assert!(Version::parse("1.2.3-alpha2") >= Version::parse("1.2.0"));
        assert!(Version::parse("1.2.3-alpha2") >= Version::parse("1.2.3-alpha1"));
        assert!(Version::parse("1.2.3-alpha2") >= Version::parse("1.2.3-alpha2"));
        assert!(Version::parse("1.2.3+23")     >= Version::parse("1.2.3+42"));
    }

    #[test]
    fn test_spec_order() {
        let vs = ["1.0.0-alpha",
                  "1.0.0-alpha.1",
                  "1.0.0-alpha.beta",
                  "1.0.0-beta",
                  "1.0.0-beta.2",
                  "1.0.0-beta.11",
                  "1.0.0-rc.1",
                  "1.0.0"];
        let mut i = 1;
        while i < vs.len() {
            let a = Version::parse(vs[i-1]).unwrap();
            let b = Version::parse(vs[i]).unwrap();
            assert!(a < b);
            i += 1;
        }
    }
}
