// Copyright 2014-2015 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

/// The set of user configurable options for compiling zero or more regexes.
#[derive(Clone, Debug)]
#[allow(missing_docs)]
pub struct RegexOptions {
    pub pats: Vec<String>,
    pub size_limit: usize,
    pub dfa_size_limit: usize,
    pub case_insensitive: bool,
    pub multi_line: bool,
    pub dot_matches_new_line: bool,
    pub swap_greed: bool,
    pub ignore_whitespace: bool,
    pub unicode: bool,
}

impl Default for RegexOptions {
    fn default() -> Self {
        RegexOptions {
            pats: vec![],
            size_limit: 10 * (1<<20),
            dfa_size_limit: 2 * (1<<20),
            case_insensitive: false,
            multi_line: false,
            dot_matches_new_line: false,
            swap_greed: false,
            ignore_whitespace: false,
            unicode: true,
        }
    }
}

macro_rules! define_builder {
    ($name:ident, $regex_mod:ident, $only_utf8:expr) => {
        pub mod $name {
            use error::Error;
            use exec::ExecBuilder;
            use super::RegexOptions;

            use $regex_mod::Regex;

/// A configurable builder for a regular expression.
///
/// A builder can be used to configure how the regex is built, for example, by
/// setting the default flags (which can be overridden in the expression
/// itself) or setting various limits.
pub struct RegexBuilder(RegexOptions);

impl RegexBuilder {
    /// Create a new regular expression builder with the given pattern.
    ///
    /// If the pattern is invalid, then an error will be returned when
    /// `compile` is called.
    pub fn new(pattern: &str) -> RegexBuilder {
        let mut builder = RegexBuilder(RegexOptions::default());
        builder.0.pats.push(pattern.to_owned());
        builder
    }

    /// Consume the builder and compile the regular expression.
    ///
    /// Note that calling `as_str` on the resulting `Regex` will produce the
    /// pattern given to `new` verbatim. Notably, it will not incorporate any
    /// of the flags set on this builder.
    pub fn build(&self) -> Result<Regex, Error> {
        ExecBuilder::new_options(self.0.clone())
            .only_utf8($only_utf8)
            .build()
            .map(Regex::from)
    }

    /// Set the value for the case insensitive (`i`) flag.
    pub fn case_insensitive(&mut self, yes: bool) -> &mut RegexBuilder {
        self.0.case_insensitive = yes;
        self
    }

    /// Set the value for the multi-line matching (`m`) flag.
    pub fn multi_line(&mut self, yes: bool) -> &mut RegexBuilder {
        self.0.multi_line = yes;
        self
    }

    /// Set the value for the any character (`s`) flag, where in `.` matches
    /// anything when `s` is set and matches anything except for new line when
    /// it is not set (the default).
    ///
    /// N.B. "matches anything" means "any byte" for `regex::bytes::Regex`
    /// expressions and means "any Unicode scalar value" for `regex::Regex`
    /// expressions.
    pub fn dot_matches_new_line(&mut self, yes: bool) -> &mut RegexBuilder {
        self.0.dot_matches_new_line = yes;
        self
    }

    /// Set the value for the greedy swap (`U`) flag.
    pub fn swap_greed(&mut self, yes: bool) -> &mut RegexBuilder {
        self.0.swap_greed = yes;
        self
    }

    /// Set the value for the ignore whitespace (`x`) flag.
    pub fn ignore_whitespace(&mut self, yes: bool) -> &mut RegexBuilder {
        self.0.ignore_whitespace = yes;
        self
    }

    /// Set the value for the Unicode (`u`) flag.
    pub fn unicode(&mut self, yes: bool) -> &mut RegexBuilder {
        self.0.unicode = yes;
        self
    }

    /// Set the approximate size limit of the compiled regular expression.
    ///
    /// This roughly corresponds to the number of bytes occupied by a single
    /// compiled program. If the program exceeds this number, then a
    /// compilation error is returned.
    pub fn size_limit(&mut self, limit: usize) -> &mut RegexBuilder {
        self.0.size_limit = limit;
        self
    }

    /// Set the approximate size of the cache used by the DFA.
    ///
    /// This roughly corresponds to the number of bytes that the DFA will
    /// use while searching.
    ///
    /// Note that this is a *per thread* limit. There is no way to set a global
    /// limit. In particular, if a regex is used from multiple threads
    /// simulanteously, then each thread may use up to the number of bytes
    /// specified here.
    pub fn dfa_size_limit(&mut self, limit: usize) -> &mut RegexBuilder {
        self.0.dfa_size_limit = limit;
        self
    }
}
        }
    }
}

define_builder!(bytes, re_bytes, false);
define_builder!(unicode, re_unicode, true);

macro_rules! define_set_builder {
    ($name:ident, $regex_mod:ident, $only_utf8:expr) => {
        pub mod $name {
            use error::Error;
            use exec::ExecBuilder;
            use super::RegexOptions;

            use re_set::$regex_mod::RegexSet;

/// A configurable builder for a set of regular expressions.
///
/// A builder can be used to configure how the regexes are built, for example,
/// by setting the default flags (which can be overridden in the expression
/// itself) or setting various limits.
pub struct RegexSetBuilder(RegexOptions);

impl RegexSetBuilder {
    /// Create a new regular expression builder with the given pattern.
    ///
    /// If the pattern is invalid, then an error will be returned when
    /// `compile` is called.
    pub fn new<I, S>(patterns: I) -> RegexSetBuilder
            where S: AsRef<str>, I: IntoIterator<Item=S> {
        let mut builder = RegexSetBuilder(RegexOptions::default());
        for pat in patterns {
            builder.0.pats.push(pat.as_ref().to_owned());
        }
        builder
    }

    /// Consume the builder and compile the regular expressions into a set.
    pub fn build(&self) -> Result<RegexSet, Error> {
        ExecBuilder::new_options(self.0.clone())
            .only_utf8($only_utf8)
            .build()
            .map(RegexSet::from)
    }

    /// Set the value for the case insensitive (`i`) flag.
    pub fn case_insensitive(&mut self, yes: bool) -> &mut RegexSetBuilder {
        self.0.case_insensitive = yes;
        self
    }

    /// Set the value for the multi-line matching (`m`) flag.
    pub fn multi_line(&mut self, yes: bool) -> &mut RegexSetBuilder {
        self.0.multi_line = yes;
        self
    }

    /// Set the value for the any character (`s`) flag, where in `.` matches
    /// anything when `s` is set and matches anything except for new line when
    /// it is not set (the default).
    ///
    /// N.B. "matches anything" means "any byte" for `regex::bytes::RegexSet`
    /// expressions and means "any Unicode scalar value" for `regex::RegexSet`
    /// expressions.
    pub fn dot_matches_new_line(&mut self, yes: bool) -> &mut RegexSetBuilder {
        self.0.dot_matches_new_line = yes;
        self
    }

    /// Set the value for the greedy swap (`U`) flag.
    pub fn swap_greed(&mut self, yes: bool) -> &mut RegexSetBuilder {
        self.0.swap_greed = yes;
        self
    }

    /// Set the value for the ignore whitespace (`x`) flag.
    pub fn ignore_whitespace(&mut self, yes: bool) -> &mut RegexSetBuilder {
        self.0.ignore_whitespace = yes;
        self
    }

    /// Set the value for the Unicode (`u`) flag.
    pub fn unicode(&mut self, yes: bool) -> &mut RegexSetBuilder {
        self.0.unicode = yes;
        self
    }

    /// Set the approximate size limit of the compiled regular expression.
    ///
    /// This roughly corresponds to the number of bytes occupied by a single
    /// compiled program. If the program exceeds this number, then a
    /// compilation error is returned.
    pub fn size_limit(&mut self, limit: usize) -> &mut RegexSetBuilder {
        self.0.size_limit = limit;
        self
    }

    /// Set the approximate size of the cache used by the DFA.
    ///
    /// This roughly corresponds to the number of bytes that the DFA will
    /// use while searching.
    ///
    /// Note that this is a *per thread* limit. There is no way to set a global
    /// limit. In particular, if a regex is used from multiple threads
    /// simulanteously, then each thread may use up to the number of bytes
    /// specified here.
    pub fn dfa_size_limit(&mut self, limit: usize) -> &mut RegexSetBuilder {
        self.0.dfa_size_limit = limit;
        self
    }
}
        }
    }
}

define_set_builder!(set_bytes, bytes, false);
define_set_builder!(set_unicode, unicode, true);
