// Copyright 2012-2013 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::error::Error;
use std::fmt;
use std::str::CharIndices;

use super::version::Version;

use self::VersionComponent::{NumericVersionComponent, WildcardVersionComponent};
use self::Op::{Ex, Gt, GtEq, Lt, LtEq, Tilde, Compatible, Wildcard};
use self::LexState::{LexInit, LexStart, LexVersionComponent, LexSigil, LexErr};
use self::Token::{Sigil, AlphaNum, Comma, Dot};
use self::WildcardVersion::{Major, Minor, Patch};
use self::ReqParseError::{
    InvalidVersionRequirement,
    OpAlreadySet,
    InvalidSigil,
    VersionComponentsMustBeNumeric,
    MajorVersionRequired,
    UnimplementedVersionRequirement
};

/// A `VersionReq` is a struct containing a list of predicates that can apply to ranges of version
/// numbers. Matching operations can then be done with the `VersionReq` against a particular
/// version to see if it satisfies some or all of the constraints.
#[derive(PartialEq,Clone,Debug)]
pub struct VersionReq {
    predicates: Vec<Predicate>
}

enum VersionComponent {
    NumericVersionComponent(u64),
    WildcardVersionComponent
}

#[derive(Clone, PartialEq, Debug)]
enum WildcardVersion {
    Major,
    Minor,
    Patch
}

#[derive(PartialEq,Clone,Debug)]
enum Op {
    Ex,   // Exact
    Gt,   // Greater than
    GtEq, // Greater than or equal to
    Lt,   // Less than
    LtEq, // Less than or equal to
    Tilde, // e.g. ~1.0.0
    Compatible, // compatible by definition of semver, indicated by ^
    Wildcard(WildcardVersion), // x.y.*, x.*, *
}

#[derive(PartialEq,Clone,Debug)]
struct Predicate {
    op: Op,
    major: u64,
    minor: Option<u64>,
    patch: Option<u64>
}

struct PredBuilder {
    op: Option<Op>,
    major: Option<u64>,
    minor: Option<u64>,
    patch: Option<u64>
}

/// A `ReqParseError` is returned from methods which parse a string into a `VersionReq`. Each
/// enumeration is one of the possible errors that can occur.
#[derive(Copy, Clone, Debug, PartialEq)]
pub enum ReqParseError {
    /// The given version requirement is invalid.
    InvalidVersionRequirement,
    /// You have already provided an operation, such as `=`, `~`, or `^`. Only use one.
    OpAlreadySet,
    /// The sigil you have written is not correct.
    InvalidSigil,
    /// All components of a version must be numeric.
    VersionComponentsMustBeNumeric,
    /// At least a major version is required.
    MajorVersionRequired,
    /// An unimplemented version requirement.
    UnimplementedVersionRequirement,
}

impl fmt::Display for ReqParseError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        self.description().fmt(f)
    }
}

impl Error for ReqParseError {
    fn description(&self) -> &str {
        match *self {
            InvalidVersionRequirement => "the given version requirement is invalid",
            OpAlreadySet => "you have already provided an operation, such as =, ~, or ^; only use one",
            InvalidSigil => "the sigil you have written is not correct",
            VersionComponentsMustBeNumeric => "version components must be numeric",
            MajorVersionRequired => "at least a major version number is required",
            UnimplementedVersionRequirement => "the given version requirement is not implemented, yet",
        }
    }
}

impl VersionReq {
    /// `any()` is a factory method which creates a `VersionReq` with no constraints. In other
    /// words, any version will match against it.
    ///
    /// # Examples
    ///
    /// ```
    /// use semver::VersionReq;
    ///
    /// let anything = VersionReq::any();
    /// ```
    pub fn any() -> VersionReq {
        VersionReq { predicates: vec!() }
    }

    /// `parse()` is the main constructor of a `VersionReq`. It turns a string like `"^1.2.3"`
    /// and turns it into a `VersionReq` that matches that particular constraint.
    ///
    /// A `Result` is returned which contains a `ReqParseError` if there was a problem parsing the
    /// `VersionReq`.
    ///
    /// # Examples
    ///
    /// ```
    /// use semver::VersionReq;
    ///
    /// let version = VersionReq::parse("=1.2.3");
    /// let version = VersionReq::parse(">1.2.3");
    /// let version = VersionReq::parse("<1.2.3");
    /// let version = VersionReq::parse("~1.2.3");
    /// let version = VersionReq::parse("^1.2.3");
    /// let version = VersionReq::parse("<=1.2.3");
    /// let version = VersionReq::parse(">=1.2.3");
    /// ```
    ///
    /// This example demonstrates error handling, and will panic.
    ///
    /// ```should-panic
    /// use semver::VersionReq;
    ///
    /// let version = match VersionReq::parse("not a version") {
    ///     Ok(version) => version,
    ///     Err(e) => panic!("There was a problem parsing: {}", e),
    /// }
    /// ```
    pub fn parse(input: &str) -> Result<VersionReq, ReqParseError> {
        let mut lexer = Lexer::new(input);
        let mut builder = PredBuilder::new();
        let mut predicates = Vec::new();

        for token in lexer.by_ref() {
            let result = match token {
                Sigil(x) => builder.set_sigil(x),
                AlphaNum(x) => builder.set_version_part(x),
                Dot => Ok(()), // Nothing to do for now
                Comma => {
                    let result = builder.build().map(|p| predicates.push(p));
                    builder = PredBuilder::new();
                    result
                }
            };

            match result {
                Ok(_) => (),
                Err(e) => return Err(e),
            }
        }

        if lexer.is_error() {
            return Err(InvalidVersionRequirement);
        }

        match builder.build() {
            Ok(e) => predicates.push(e),
            Err(e) => return Err(e),
        }

        Ok(VersionReq { predicates: predicates })
    }

    /// `exact()` is a factory method which creates a `VersionReq` with one exact constraint.
    ///
    /// # Examples
    ///
    /// ```
    /// use semver::VersionReq;
    /// use semver::Version;
    ///
    /// let version = Version { major: 1, minor: 1, patch: 1, pre: vec![], build: vec![] };
    /// let exact = VersionReq::exact(&version);
    /// ```
    pub fn exact(version: &Version) -> VersionReq {
        VersionReq { predicates: vec!(Predicate::exact(version)) }
    }

    /// `matches()` matches a given `Version` against this `VersionReq`.
    ///
    /// # Examples
    ///
    /// ```
    /// use semver::VersionReq;
    /// use semver::Version;
    ///
    /// let version = Version { major: 1, minor: 1, patch: 1, pre: vec![], build: vec![] };
    /// let exact = VersionReq::exact(&version);
    ///
    /// assert!(exact.matches(&version));
    /// ```
    pub fn matches(&self, version: &Version) -> bool {
        self.predicates.iter().all(|p| p.matches(version))
    }
}

impl Predicate {
    fn exact(version: &Version) -> Predicate {
        Predicate {
            op: Ex,
            major: version.major,
            minor: Some(version.minor),
            patch: Some(version.patch)
        }
    }

    /// `matches()` takes a `Version` and determines if it matches this particular `Predicate`.
    pub fn matches(&self, ver: &Version) -> bool {
        match self.op {
            Ex => self.is_exact(ver),
            Gt => self.is_greater(ver),
            GtEq => self.is_exact(ver) || self.is_greater(ver),
            Lt => !self.is_exact(ver) && !self.is_greater(ver),
            LtEq => !self.is_greater(ver),
            Tilde => self.matches_tilde(ver),
            Compatible => self.is_compatible(ver),
            Wildcard(_) => self.matches_wildcard(ver)
        }
    }

    fn is_exact(&self, ver: &Version) -> bool {
        if self.major != ver.major {
            return false;
        }

        match self.minor {
            Some(minor) => {
                if minor != ver.minor {
                    return false;
                }
            }
            None => return true
        }

        match self.patch {
            Some(patch) => {
                if patch != ver.patch {
                    return false;
                }
            }
            None => return true
        }

        true
    }

    fn is_greater(&self, ver: &Version) -> bool {
        if self.major != ver.major {
            return ver.major > self.major;
        }

        match self.minor {
            Some(minor) => {
                if minor != ver.minor {
                    return ver.minor > minor
                }
            }
            None => return false
        }

        match self.patch {
            Some(patch) => {
                if patch != ver.patch {
                    return ver.patch > patch
                }
            }
            None => return false
        }

        false
    }

    // see https://www.npmjs.org/doc/misc/semver.html for behavior
    fn matches_tilde(&self, ver: &Version) -> bool {
        let minor = match self.minor {
            Some(n) => n,
            None => return self.major == ver.major
        };

        match self.patch {
            Some(patch) => {
                self.major == ver.major && minor == ver.minor && ver.patch >= patch
            }
            None => {
                self.major == ver.major && minor == ver.minor
            }
        }
    }

    // see https://www.npmjs.org/doc/misc/semver.html for behavior
    fn is_compatible(&self, ver: &Version) -> bool {
        if self.major != ver.major {
            return false;
        }

        let minor = match self.minor {
            Some(n) => n,
            None => return self.major == ver.major
        };

        match self.patch {
            Some(patch) => if self.major == 0 {
                if minor == 0 {
                    ver.minor == minor && ver.patch == patch
                } else {
                    ver.minor == minor && ver.patch >= patch
                }
            } else {
                ver.minor > minor || (ver.minor == minor && ver.patch >= patch)
            },
            None => if self.major == 0 {
                ver.minor == minor
            } else {
                ver.minor >= minor
            }
        }
    }

    // see https://www.npmjs.org/doc/misc/semver.html for behavior
    fn matches_wildcard(&self, ver: &Version) -> bool {
        match self.op {
            Wildcard(Major) => true,
            Wildcard(Minor) => self.major == ver.major,
            Wildcard(Patch) => {
                match self.minor {
                    Some(minor) => self.major == ver.major && minor == ver.minor,
                    None => false  // unreachable
                }
            }
            _ => false  // unreachable
        }
    }
}

impl PredBuilder {
    fn new() -> PredBuilder {
        PredBuilder {
            op: None,
            major: None,
            minor: None,
            patch: None
        }
    }

    fn set_sigil(&mut self, sigil: &str) -> Result<(), ReqParseError> {
        if self.op.is_some() {
            return Err(OpAlreadySet);
        }

        match Op::from_sigil(sigil) {
            Some(op) => self.op = Some(op),
            _ => return Err(InvalidSigil),
        }

        Ok(())
    }

    fn set_version_part(&mut self, part: &str) -> Result<(), ReqParseError> {
        if self.op.is_none() {
            // If no op is specified, then the predicate is an exact match on
            // the version
            self.op = Some(Compatible);
        }

        if self.major.is_none() {
            match parse_version_part(part) {
                Ok(NumericVersionComponent(e)) => self.major = Some(e),
                Ok(WildcardVersionComponent) => {
                    self.major = Some(0);
                    self.op = Some(Wildcard(Major))
                }
                Err(e) => return Err(e),
            }
        } else if self.minor.is_none() {
            match parse_version_part(part) {
                Ok(NumericVersionComponent(e)) => self.minor = Some(e),
                Ok(WildcardVersionComponent) => self.op = Some(Wildcard(Minor)),
                Err(e) => return Err(e),
            }
        }
        else if self.patch.is_none() {
            match parse_version_part(part) {
                Ok(NumericVersionComponent(e)) => self.patch = Some(e),
                Ok(WildcardVersionComponent) => self.op = Some(Wildcard(Patch)),
                Err(e) => return Err(e),
            }
        }

        Ok(())
    }

    /// Validates that a version predicate can be created given the present
    /// information.
    fn build(&self) -> Result<Predicate, ReqParseError> {
        let op = match self.op {
            Some(ref x) => x.clone(),
            None => return Err(InvalidVersionRequirement),
        };

        let major = match self.major {
            Some(x) => x,
            None => return Err(MajorVersionRequired),
        };

        Ok(Predicate {
            op: op,
            major: major,
            minor: self.minor,
            patch: self.patch
        })
    }
}

struct Lexer<'a> {
    c: char,
    idx: usize,
    iter: CharIndices<'a>,
    mark: Option<usize>,
    input: &'a str,
    state: LexState
}

#[derive(Copy, Clone, Debug, PartialEq)]
enum LexState {
    LexInit,
    LexStart,
    LexVersionComponent,
    LexSigil,
    LexErr,
}

#[derive(Debug)]
enum Token<'a> {
    Sigil(&'a str),
    AlphaNum(&'a str),
    Comma,
    Dot
}

impl<'a> Lexer<'a> {
    fn new(input: &'a str) -> Lexer<'a> {
        Lexer {
            c: '\0',
            idx: 0,
            iter: input.char_indices(),
            mark: None,
            input: input,
            state: LexInit
        }
    }

    fn is_error(&self) -> bool {
        self.state == LexErr
    }

    fn mark(&mut self, at: usize) {
        self.mark = Some(at)
    }

    fn flush(&mut self, to: usize, kind: LexState) -> Option<Token<'a>> {
        match self.mark {
            Some(mark) => {
                if to <= mark {
                    return None;
                }

                let s = &self.input[mark..to];

                self.mark = None;

                match kind {
                    LexVersionComponent => Some(AlphaNum(s)),
                    LexSigil => Some(Sigil(s)),
                    _ => None // bug
                }
            }
            None => None
        }
    }
}

impl<'a> Iterator for Lexer<'a> {
    type Item = Token<'a>;

    fn next(&mut self) -> Option<Token<'a>> {
        let mut c;
        let mut idx = 0;

        macro_rules! next {
            () => (
                match self.iter.next() {
                    Some((n_idx, n_char)) => {
                        c = n_char;
                        idx = n_idx;
                    }
                    _ => {
                      let s = self.state;
                      return self.flush(idx + 1, s)
                    }
                }
            )
        }

        macro_rules! flush {
            ($s:expr) => ({
                self.c = c;
                self.idx = idx;
                self.flush(idx, $s)
            })
        }


        if self.state == LexInit {
            self.state = LexStart;
            next!();
        } else {
            c = self.c;
            idx = self.idx;
        }

        loop {
            match self.state {
                LexStart => {
                    if c.is_whitespace() {
                        next!(); // Ignore
                    }
                    else if c.is_alphanumeric() || c == '*' {
                        self.mark(idx);
                        self.state = LexVersionComponent;
                        next!();
                    }
                    else if is_sigil(c) {
                        self.mark(idx);
                        self.state = LexSigil;
                        next!();
                    }
                    else if c == '.' {
                        self.state = LexInit;
                        return Some(Dot);
                    }
                    else if c == ',' {
                        self.state = LexInit;
                        return Some(Comma);
                    } else {
                        self.state = LexErr;
                        return None;
                    }
                }
                LexVersionComponent => {
                    if c.is_alphanumeric() {
                        next!();
                    } else {
                        self.state = LexStart;
                        return flush!(LexVersionComponent);
                    }
                }
                LexSigil => {
                    if is_sigil(c) {
                        next!();
                    } else {
                        self.state = LexStart;
                        return flush!(LexSigil);
                    }
                }
                LexErr => return None,
                LexInit => return None // bug
            }
        }
    }
}

impl Op {
    fn from_sigil(sigil: &str) -> Option<Op> {
        match sigil {
            "=" => Some(Ex),
            ">" => Some(Gt),
            ">=" => Some(GtEq),
            "<" => Some(Lt),
            "<=" => Some(LtEq),
            "~" => Some(Tilde),
            "^" => Some(Compatible),
            _ => None
        }
    }
}

fn parse_version_part(s: &str) -> Result<VersionComponent, ReqParseError> {
    let mut ret = 0;

    if s == "*" {
        return Ok(WildcardVersionComponent)
    }

    for c in s.chars() {
        let n = (c as u64) - ('0' as u64);

        if n > 9 {
            return Err(VersionComponentsMustBeNumeric);
        }

        ret *= 10;
        ret +=  n;
    }

    Ok(NumericVersionComponent(ret))
}

fn is_sigil(c: char) -> bool {
    match c {
        '>' | '<' | '=' | '~' | '^' => true,
        _ => false
    }
}

impl fmt::Display for VersionReq {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        if self.predicates.is_empty() {
            try!(write!(fmt, "*"));
        } else {
            for (i, ref pred) in self.predicates.iter().enumerate() {
                if i == 0 {
                    try!(write!(fmt, "{}", pred));
                } else {
                    try!(write!(fmt, ", {}", pred));
                }
            }
        }

        Ok(())
    }
}

impl fmt::Display for Predicate {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match self.op {
            Wildcard(Major) => try!(write!(fmt, "*")),
            Wildcard(Minor) => try!(write!(fmt, "{}.*", self.major)),
            Wildcard(Patch) => try!(write!(fmt, "{}.{}.*", self.major, self.minor.unwrap())),
            _ => {
                try!(write!(fmt, "{}{}", self.op, self.major));

                match self.minor {
                    Some(v) => try!(write!(fmt, ".{}", v)),
                    None => ()
                }

                match self.patch {
                    Some(v) => try!(write!(fmt, ".{}", v)),
                    None => ()
                }
            },
        }

        Ok(())
    }
}

impl fmt::Display for Op {
    fn fmt(&self, fmt: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            Ex          => try!(write!(fmt, "= ")),
            Gt          => try!(write!(fmt, "> ")),
            GtEq        => try!(write!(fmt, ">= ")),
            Lt          => try!(write!(fmt, "< ")),
            LtEq        => try!(write!(fmt, "<= ")),
            Tilde       => try!(write!(fmt, "~")),
            Compatible  => try!(write!(fmt, "^")),
             // gets handled specially in Predicate::fmt
            Wildcard(_) => try!(write!(fmt, "")),
        }
        Ok(())
    }
}

#[cfg(test)]
mod test {
    use super::VersionReq;
    use super::super::version::Version;
    use super::ReqParseError::{
        InvalidVersionRequirement,
        OpAlreadySet,
        InvalidSigil,
        VersionComponentsMustBeNumeric,
        MajorVersionRequired,
    };

    fn req(s: &str) -> VersionReq {
        VersionReq::parse(s).unwrap()
    }

    fn version(s: &str) -> Version {
        match Version::parse(s) {
            Ok(v) => v,
            Err(e) => panic!("`{}` is not a valid version. Reason: {}", s, e)
        }
    }

    fn assert_match(req: &VersionReq, vers: &[&str]) {
        for ver in vers.iter() {
            assert!(req.matches(&version(*ver)), "did not match {}", ver);
        }
    }

    fn assert_not_match(req: &VersionReq, vers: &[&str]) {
        for ver in vers.iter() {
            assert!(!req.matches(&version(*ver)), "matched {}", ver);
        }
    }

    #[test]
    fn test_parsing_default() {
        let r = req("1.0.0");

        assert_eq!(r.to_string(), "^1.0.0".to_string());

        assert_match(&r, &["1.0.0", "1.0.1"]);
        assert_not_match(&r, &["0.9.9", "0.10.0", "0.1.0"]);
    }

    #[test]
    fn test_parsing_exact() {
        let r = req("=1.0.0");

        assert!(r.to_string() == "= 1.0.0".to_string());
        assert_eq!(r.to_string(), "= 1.0.0".to_string());

        assert_match(&r, &["1.0.0"]);
        assert_not_match(&r, &["1.0.1", "0.9.9", "0.10.0", "0.1.0"]);

        let r = req("=0.9.0");

        assert_eq!(r.to_string(), "= 0.9.0".to_string());

        assert_match(&r, &["0.9.0"]);
        assert_not_match(&r, &["0.9.1", "1.9.0", "0.0.9"]);
    }

    #[test]
    pub fn test_parsing_greater_than() {
        let r = req(">= 1.0.0");

        assert_eq!(r.to_string(), ">= 1.0.0".to_string());

        assert_match(&r, &["1.0.0"]);
    }

    #[test]
    pub fn test_multiple() {
        let r = req("> 0.0.9, <= 2.5.3");
        assert_eq!(r.to_string(), "> 0.0.9, <= 2.5.3".to_string());
        assert_match(&r, &["0.0.10", "1.0.0", "2.5.3"]);
        assert_not_match(&r, &["0.0.8", "2.5.4"]);

        let r = req("0.3.0, 0.4.0");
        assert_eq!(r.to_string(), "^0.3.0, ^0.4.0".to_string());
        assert_not_match(&r, &["0.0.8", "0.3.0", "0.4.0"]);

        let r = req("<= 0.2.0, >= 0.5.0");
        assert_eq!(r.to_string(), "<= 0.2.0, >= 0.5.0".to_string());
        assert_not_match(&r, &["0.0.8", "0.3.0", "0.5.1"]);

        let r = req("0.1.0, 0.1.4, 0.1.6");
        assert_eq!(r.to_string(), "^0.1.0, ^0.1.4, ^0.1.6".to_string());
        assert_match(&r, &["0.1.6", "0.1.9"]);
        assert_not_match(&r, &["0.1.0", "0.1.4", "0.2.0"]);

        assert!(VersionReq::parse("> 0.1.0,").is_err());
        assert!(VersionReq::parse("> 0.3.0, ,").is_err());
    }

    #[test]
    pub fn test_parsing_tilde() {
        let r = req("~1");
        assert_match(&r, &["1.0.0", "1.0.1", "1.1.1"]);
        assert_not_match(&r, &["0.9.1", "2.9.0", "0.0.9"]);

        let r = req("~1.2");
        assert_match(&r, &["1.2.0", "1.2.1"]);
        assert_not_match(&r, &["1.1.1", "1.3.0", "0.0.9"]);

        let r = req("~1.2.2");
        assert_match(&r, &["1.2.2", "1.2.4"]);
        assert_not_match(&r, &["1.2.1", "1.9.0", "1.0.9", "2.0.1", "0.1.3"]);
    }

    #[test]
    pub fn test_parsing_compatible() {
        let r = req("^1");
        assert_match(&r, &["1.1.2", "1.1.0", "1.2.1", "1.0.1"]);
        assert_not_match(&r, &["0.9.1", "2.9.0", "0.1.4"]);

        let r = req("^1.1");
        assert_match(&r, &["1.1.2", "1.1.0", "1.2.1"]);
        assert_not_match(&r, &["0.9.1", "2.9.0", "1.0.1", "0.1.4"]);

        let r = req("^1.1.2");
        assert_match(&r, &["1.1.2", "1.1.4", "1.2.1"]);
        assert_not_match(&r, &["0.9.1", "2.9.0", "1.1.1", "0.0.1"]);

        let r = req("^0.1.2");
        assert_match(&r, &["0.1.2", "0.1.4"]);
        assert_not_match(&r, &["0.9.1", "2.9.0", "1.1.1", "0.0.1"]);

        let r = req("^0.0.2");
        assert_match(&r, &["0.0.2"]);
        assert_not_match(&r, &["0.9.1", "2.9.0", "1.1.1", "0.0.1", "0.1.4"]);

        let r = req("^0.0");
        assert_match(&r, &["0.0.2", "0.0.0"]);
        assert_not_match(&r, &["0.9.1", "2.9.0", "1.1.1", "0.1.4"]);

        let r = req("^0");
        assert_match(&r, &["0.9.1", "0.0.2", "0.0.0"]);
        assert_not_match(&r, &["2.9.0", "1.1.1"]);
    }

    #[test]
    pub fn test_parsing_wildcard() {
        let r = req("*");
        assert_match(&r, &["0.9.1", "2.9.0", "0.0.9", "1.0.1", "1.1.1"]);
        assert_not_match(&r, &[]);

        let r = req("1.*");
        assert_match(&r, &["1.2.0", "1.2.1", "1.1.1", "1.3.0"]);
        assert_not_match(&r, &["0.0.9"]);

        let r = req("1.2.*");
        assert_match(&r, &["1.2.0", "1.2.2", "1.2.4"]);
        assert_not_match(&r, &["1.9.0", "1.0.9", "2.0.1", "0.1.3"]);
    }


    #[test]
    pub fn test_parse_errors() {
        assert_eq!(Err(InvalidVersionRequirement), VersionReq::parse("0-0.1"));
        assert_eq!(Err(OpAlreadySet), VersionReq::parse(">= >= 0.0.2"));
        assert_eq!(Err(InvalidSigil), VersionReq::parse(">== 0.0.2"));
        assert_eq!(Err(VersionComponentsMustBeNumeric), VersionReq::parse("a.0.0"));
        assert_eq!(Err(MajorVersionRequired), VersionReq::parse(">="));
    }


    /* TODO:
     * - Handle pre releases
     */
}
