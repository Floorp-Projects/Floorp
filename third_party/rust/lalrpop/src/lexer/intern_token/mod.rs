/*!

Generates an iterator type `Matcher` that looks roughly like

```ignore
mod intern_token {
    extern crate regex as regex;

    #[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]
    pub struct Token<'input>(pub usize, pub &'input str);
    //                           ~~~~~~     ~~~~~~~~~~~
    //                           token      token
    //                           index      text
    //                           (type)

    impl<'a> fmt::Display for Token<'a> { ... }

    pub struct MatcherBuilder {
        regex_set: regex::RegexSet,
        regex_vec: Vec<regex::Regex>,
    }

    impl MatcherBuilder {
        fn new() -> MatchBuilder { ... }
        fn matcher<'input, 'builder>(&'builder self, s: &'input str) -> Matcher<'input, 'builder> { ... }
    }

    pub struct Matcher<'input, 'builder> {
        text: &'input str,
        consumed: usize,
        regex_set: &'builder regex::RegexSet,
        regex_vec: &'builder Vec<regex::Regex>,
    }

    impl Matcher<'input> {
        fn tokenize(&self, text: &str) -> Option<(usize, usize)> { ... }
    }

    impl<'input> Iterator for Matcher<'input> {
        type Item = Result<(usize, Token<'input>, usize), ParseError>;
        //                  ~~~~~  ~~~~~~~~~~~~~  ~~~~~
        //                  start  token          end
    }
}
```

 */

use grammar::parse_tree::InternToken;
use grammar::repr::{Grammar, TerminalLiteral};
use lexer::re;
use rust::RustWrite;
use std::io::{self, Write};

pub fn compile<W: Write>(
    grammar: &Grammar,
    intern_token: &InternToken,
    out: &mut RustWrite<W>,
) -> io::Result<()> {
    let prefix = &grammar.prefix;

    rust!(out, "#[cfg_attr(rustfmt, rustfmt_skip)]");
    rust!(out, "mod {}intern_token {{", prefix);
    rust!(out, "#![allow(unused_imports)]");
    try!(out.write_uses("", &grammar));
    rust!(out, "extern crate regex as {}regex;", prefix);
    rust!(out, "use std::fmt as {}fmt;", prefix);
    rust!(out, "");
    rust!(
        out,
        "#[derive(Clone, Debug, PartialEq, Eq, PartialOrd, Ord)]"
    );
    rust!(out, "pub struct Token<'input>(pub usize, pub &'input str);");
    rust!(out, "impl<'a> {}fmt::Display for Token<'a> {{", prefix);
    rust!(
        out,
        "fn fmt(&self, formatter: &mut {}fmt::Formatter) -> Result<(), {}fmt::Error> {{",
        prefix,
        prefix
    );
    rust!(out, "{}fmt::Display::fmt(self.1, formatter)", prefix);
    rust!(out, "}}");
    rust!(out, "}}");
    rust!(out, "");
    rust!(out, "pub struct {}MatcherBuilder {{", prefix);
    rust!(out, "regex_set: {}regex::RegexSet,", prefix);
    rust!(out, "regex_vec: Vec<{}regex::Regex>,", prefix);
    rust!(out, "}}");
    rust!(out, "");
    rust!(out, "impl {}MatcherBuilder {{", prefix);
    rust!(out, "pub fn new() -> {}MatcherBuilder {{", prefix);

    // create a vector of rust string literals with the text of each
    // regular expression
    let regex_strings: Vec<String> = {
        intern_token
            .match_entries
            .iter()
            .map(|match_entry| match match_entry.match_literal {
                TerminalLiteral::Quoted(ref s) => re::parse_literal(&s),
                TerminalLiteral::Regex(ref s) => re::parse_regex(&s).unwrap(),
            })
            .map(|regex| {
                // make sure all regex are anchored at the beginning of the input
                format!("^({})", regex)
            })
            .map(|regex_str| {
                // create a rust string with text of the regex; the Debug impl
                // will add quotes and escape
                format!("{:?}", regex_str)
            })
            .collect()
    };

    rust!(out, "let {}strs: &[&str] = &[", prefix);
    for literal in &regex_strings {
        rust!(out, "{},", literal);
    }
    rust!(out, "];");
    rust!(
        out,
        "let {}regex_set = {}regex::RegexSet::new({}strs).unwrap();",
        prefix,
        prefix,
        prefix
    );

    rust!(out, "let {}regex_vec = vec![", prefix);
    for literal in &regex_strings {
        rust!(out, "{}regex::Regex::new({}).unwrap(),", prefix, literal);
    }
    rust!(out, "];");

    rust!(
        out,
        "{0}MatcherBuilder {{ regex_set: {0}regex_set, regex_vec: {0}regex_vec }}",
        prefix
    );
    rust!(out, "}}"); // fn new()
    rust!(
        out,
        "pub fn matcher<'input, 'builder>(&'builder self, s: &'input str) \
         -> {}Matcher<'input, 'builder> {{",
        prefix
    );
    rust!(out, "{}Matcher {{", prefix);
    rust!(out, "text: s,");
    rust!(out, "consumed: 0,");
    rust!(out, "regex_set: &self.regex_set,");
    rust!(out, "regex_vec: &self.regex_vec,");
    rust!(out, "}}"); // struct literal
    rust!(out, "}}"); // fn matcher()
    rust!(out, "}}"); // impl MatcherBuilder
    rust!(out, "");
    rust!(out, "pub struct {}Matcher<'input, 'builder> {{", prefix);
    rust!(out, "text: &'input str,"); // remaining input
    rust!(out, "consumed: usize,"); // number of chars consumed thus far
    rust!(out, "regex_set: &'builder {}regex::RegexSet,", prefix);
    rust!(out, "regex_vec: &'builder Vec<{}regex::Regex>,", prefix);
    rust!(out, "}}");
    rust!(out, "");
    rust!(
        out,
        "impl<'input, 'builder> Iterator for {}Matcher<'input, 'builder> {{",
        prefix
    );
    rust!(
        out,
        "type Item = Result<(usize, Token<'input>, usize), \
         {}lalrpop_util::ParseError<usize,Token<'input>,{}>>;",
        prefix,
        grammar.types.error_type()
    );
    rust!(out, "");
    rust!(out, "fn next(&mut self) -> Option<Self::Item> {{");

    // start by trimming whitespace from left
    rust!(out, "let {}text = self.text.trim_left();", prefix);
    rust!(
        out,
        "let {}whitespace = self.text.len() - {}text.len();",
        prefix,
        prefix
    );
    rust!(
        out,
        "let {}start_offset = self.consumed + {}whitespace;",
        prefix,
        prefix
    );

    // if nothing left, return None
    rust!(out, "if {}text.is_empty() {{", prefix);
    rust!(out, "self.text = {}text;", prefix);
    rust!(out, "self.consumed = {}start_offset;", prefix);
    rust!(out, "None");
    rust!(out, "}} else {{");

    // otherwise, use regex-set to find list of matching tokens
    rust!(
        out,
        "let {}matches = self.regex_set.matches({}text);",
        prefix,
        prefix
    );

    // if nothing matched, return an error
    rust!(out, "if !{}matches.matched_any() {{", prefix);
    rust!(
        out,
        "Some(Err({}lalrpop_util::ParseError::InvalidToken {{",
        prefix
    );
    rust!(out, "location: {}start_offset,", prefix);
    rust!(out, "}}))");
    rust!(out, "}} else {{");

    // otherwise, have to find longest, highest-priority match. We have the literals
    // sorted in order of increasing precedence, so we'll iterate over them one by one,
    // checking if each one matches, and remembering the longest one.
    rust!(out, "let mut {}longest_match = 0;", prefix); // length of longest match
    rust!(out, "let mut {}index = 0;", prefix); // index of longest match
    rust!(
        out,
        "for {}i in 0 .. {} {{",
        prefix,
        intern_token.match_entries.len()
    );
    rust!(out, "if {}matches.matched({}i) {{", prefix, prefix);

    // re-run the regex to find out how long this particular match
    // was, then compare that against the longest-match so far. Note
    // that the order of the tuple is carefully constructed to ensure
    // that (a) we get the longest-match but (b) if two matches are
    // equal, we get the largest index. This is because the indices
    // are sorted in order of increasing priority, and because we know
    // that indices of equal priority cannot both match (because of
    // the DFA check).
    rust!(
        out,
        "let {}match = self.regex_vec[{}i].find({}text).unwrap();",
        prefix,
        prefix,
        prefix
    );
    rust!(out, "let {}len = {}match.end();", prefix, prefix);
    rust!(out, "if {}len >= {}longest_match {{", prefix, prefix);
    rust!(out, "{}longest_match = {}len;", prefix, prefix);
    rust!(out, "{}index = {}i;", prefix, prefix);
    rust!(out, "}}"); // if is longest match
    rust!(out, "}}"); // if matches.matched(i)
    rust!(out, "}}"); // for loop

    // transform the result into the expected return value
    rust!(
        out,
        "let {}result = &{}text[..{}longest_match];",
        prefix,
        prefix,
        prefix
    );
    rust!(
        out,
        "let {}remaining = &{}text[{}longest_match..];",
        prefix,
        prefix,
        prefix
    );
    rust!(
        out,
        "let {}end_offset = {}start_offset + {}longest_match;",
        prefix,
        prefix,
        prefix
    );
    rust!(out, "self.text = {}remaining;", prefix);
    rust!(out, "self.consumed = {}end_offset;", prefix);
    rust!(
        out,
        "Some(Ok(({}start_offset, Token({}index, {}result), {}end_offset)))",
        prefix,
        prefix,
        prefix,
        prefix
    );

    rust!(out, "}}"); // else
    rust!(out, "}}"); // else
    rust!(out, "}}"); // fn
    rust!(out, "}}"); // impl
    rust!(out, "}}"); // mod
    Ok(())
}
