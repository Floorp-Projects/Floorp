//! `textwrap` provides functions for word wrapping and filling text.
//!
//! Wrapping text can be very useful in commandline programs where you
//! want to format dynamic output nicely so it looks good in a
//! terminal. A quick example:
//!
//! ```
//! extern crate textwrap;
//! use textwrap::fill;
//!
//! fn main() {
//!    let text = "textwrap: a small library for wrapping text.";
//!    println!("{}", fill(text, 18));
//! }
//! ```
//!
//! This will display the following output:
//! ```text
//! textwrap: a small
//! library for
//! wrapping text.
//! ```
//!
//! # Displayed Width vs Byte Size
//!
//! To word wrap text, one must know the width of each word so one can
//! know when to break lines. This library measures the width of text
//! using the [displayed width][unicode-width], not the size in bytes.
//!
//! This is important for non-ASCII text. ASCII characters such as `a`
//! and `!` are simple and take up one column each. This means that
//! the displayed width is equal to the string length in bytes.
//! However, non-ASCII characters and symbols take up more than one
//! byte when UTF-8 encoded: `é` is `0xc3 0xa9` (two bytes) and `⚙` is
//! `0xe2 0x9a 0x99` (three bytes) in UTF-8, respectively.
//!
//! This is why we take care to use the displayed width instead of the
//! byte count when computing line lengths. All functions in this
//! library handle Unicode characters like this.
//!
//! [unicode-width]: https://docs.rs/unicode-width/


extern crate unicode_width;
extern crate term_size;
#[cfg(feature = "hyphenation")]
extern crate hyphenation;

use unicode_width::UnicodeWidthStr;
use unicode_width::UnicodeWidthChar;
#[cfg(feature = "hyphenation")]
use hyphenation::{Hyphenation, Corpus};

/// An interface for splitting words.
///
/// When the [`wrap`] method will try to fit text into a line, it will
/// eventually find a word that it too large the current text width.
/// It will then call the currently configured `WordSplitter` to have
/// it attempt to split the word into smaller parts. This trait
/// describes that functionality via the [`split`] method.
///
/// If the `textwrap` crate has been compiled with the `hyphenation`
/// feature enabled, you will find an implementation of `WordSplitter`
/// by the `hyphenation::language::Corpus` struct. Use this struct for
/// language-aware hyphenation. See the [`hyphenation` documentation]
/// for details.
///
/// [`wrap`]: struct.Wrapper.html#method.wrap
/// [`split`]: #tymethod.split
/// [`hyphenation` documentation]: https://docs.rs/hyphenation/
pub trait WordSplitter {
    /// Return all possible splits of word. Each split is a triple
    /// with a head, a hyphen, and a tail where `head + &hyphen +
    /// &tail == word`. The hyphen can be empty if there is already a
    /// hyphen in the head.
    ///
    /// The splits should go from smallest to longest and should
    /// include no split at all. So the word "technology" could be
    /// split into
    ///
    /// ```
    /// vec![("tech", "-", "nology"),
    ///      ("technol", "-", "ogy"),
    ///      ("technolo", "-", "gy"),
    ///      ("technology", "", "")];
    /// ```
    fn split<'w>(&self, word: &'w str) -> Vec<(&'w str, &'w str, &'w str)>;
}

/// Use this as a [`Wrapper.splitter`] to avoid any kind of
/// hyphenation:
/// ```
/// use textwrap::{Wrapper, NoHyphenation};
///
/// let wrapper = Wrapper::new(8).word_splitter(Box::new(NoHyphenation));
/// assert_eq!(wrapper.wrap("foo bar-baz"), vec!["foo", "bar-baz"]);
/// ```
///
/// [`Wrapper.splitter`]: struct.Wrapper.html#structfield.splitter
pub struct NoHyphenation;

/// `NoHyphenation` implements `WordSplitter` by not splitting the
/// word at all.
impl WordSplitter for NoHyphenation {
    fn split<'w>(&self, word: &'w str) -> Vec<(&'w str, &'w str, &'w str)> {
        vec![(word, "", "")]
    }
}

/// Simple and default way to split words: splitting on existing
/// hyphens only.
///
/// You probably don't need to use this type since it's already used
/// by default by `Wrapper::new`.
pub struct HyphenSplitter;

/// HyphenSplitter is the default WordSplitter used by `Wrapper::new`.
/// It will split words on any existing hyphens in the word.
///
/// It will only use hyphens that are surrounded by alphanumeric
/// characters, which prevents a word like "--foo-bar" from being
/// split on the first or second hyphen.
impl WordSplitter for HyphenSplitter {
    fn split<'w>(&self, word: &'w str) -> Vec<(&'w str, &'w str, &'w str)> {
        let mut triples = Vec::new();
        // Split on hyphens, smallest split first. We only use hyphens
        // that are surrounded by alphanumeric characters. This is to
        // avoid splitting on repeated hyphens, such as those found in
        // --foo-bar.
        let char_indices = word.char_indices().collect::<Vec<_>>();
        for w in char_indices.windows(3) {
            let ((_, prev), (n, c), (_, next)) = (w[0], w[1], w[2]);
            if prev.is_alphanumeric() && c == '-' && next.is_alphanumeric() {
                let (head, tail) = word.split_at(n + 1);
                triples.push((head, "", tail));
            }
        }

        // Finally option is no split at all.
        triples.push((word, "", ""));

        triples
    }
}

/// A hyphenation Corpus can be used to do language-specific
/// hyphenation using patterns from the hyphenation crate.
#[cfg(feature = "hyphenation")]
impl WordSplitter for Corpus {
    fn split<'w>(&self, word: &'w str) -> Vec<(&'w str, &'w str, &'w str)> {
        // Find splits based on language corpus.
        let mut triples = Vec::new();
        for n in word.opportunities(&self) {
            let (head, tail) = word.split_at(n);
            let hyphen = if head.ends_with('-') { "" } else { "-" };
            triples.push((head, hyphen, tail));
        }
        // Finally option is no split at all.
        triples.push((word, "", ""));

        triples
    }
}

struct IndentedString {
    value: String,
    empty_len: usize,
}

impl IndentedString {
    /// Create a new indented string. The string will initially have
    /// the content `indent` and the given capacity.
    #[inline]
    fn new(indent: &str, capacity: usize) -> IndentedString {
        let mut value = String::with_capacity(capacity);
        value.push_str(indent);
        IndentedString {
            value: value,
            empty_len: indent.len(),
        }
    }

    /// Returns `true` if the string has no other content apart from
    /// the indentation.
    #[inline]
    fn is_empty(&self) -> bool {
        self.value.len() == self.empty_len
    }

    /// Appends the given `char` to the end of this string.
    #[inline]
    fn push(&mut self, ch: char) {
        self.value.push(ch);
    }

    /// Appends the given string slice to the end of this string.
    #[inline]
    fn push_str(&mut self, s: &str) {
        self.value.push_str(s);
    }

    /// Return the inner `String`.
    fn into_string(self) -> String {
        self.value
    }
}

/// A Wrapper holds settings for wrapping and filling text. Use it
/// when the convenience [`wrap`] and [`fill`] functions are not
/// flexible enough.
///
/// [`wrap`]: fn.wrap.html
/// [`fill`]: fn.fill.html
///
/// The algorithm used by the `wrap` method works by doing a single
/// scan over words in the input string and splitting them into one or
/// more lines. The time and memory complexity is O(*n*) where *n* is
/// the length of the input string.
pub struct Wrapper<'a> {
    /// The width in columns at which the text will be wrapped.
    pub width: usize,
    /// Indentation used for the first line of output.
    pub initial_indent: &'a str,
    /// Indentation used for subsequent lines of output.
    pub subsequent_indent: &'a str,
    /// Allow long words to be broken if they cannot fit on a line.
    /// When set to `false`, some lines be being longer than
    /// `self.width`.
    pub break_words: bool,
    /// This crate treats every whitespace character (space, newline,
    /// TAB, ...) as a space character. When this option is set to
    /// `true`, the whitespace between words will be squeezed into a
    /// single space character. Otherwise, the whitespace is
    /// significant and will be kept in the output. Whitespace
    /// characters such as TAB will still be turned into a single
    /// space -- consider expanding TABs manually if this is a
    /// concern.
    pub squeeze_whitespace: bool,
    /// The method for splitting words. If the `hyphenation` feature
    /// is enabled, you can use a `hyphenation::language::Corpus` here
    /// to get language-aware hyphenation.
    pub splitter: Box<WordSplitter>,
}

impl<'a> Wrapper<'a> {
    /// Create a new Wrapper for wrapping at the specified width. By
    /// default, we allow words longer than `width` to be broken. A
    /// [`HyphenSplitter`] will be used by default for splitting
    /// words. See the [`WordSplitter`] trait for other options.
    ///
    /// [`HyphenSplitter`]: struct.HyphenSplitter.html
    /// [`WordSplitter`]: trait.WordSplitter.html
    pub fn new(width: usize) -> Wrapper<'a> {
        Wrapper {
            width: width,
            initial_indent: "",
            subsequent_indent: "",
            break_words: true,
            squeeze_whitespace: false,
            splitter: Box::new(HyphenSplitter),
        }
    }

    /// Crate a new Wrapper for wrapping text at the current terminal
    /// width. If the terminal width cannot be determined (typically
    /// because the standard input and output is not connected to a
    /// terminal), a width of 80 characters will be used. Other
    /// settings use the same defaults as `Wrapper::new`.
    pub fn with_termwidth() -> Wrapper<'a> {
        Wrapper::new(term_size::dimensions_stdout().map_or(80, |(w, _)| w))
    }

    /// Change [`self.initial_indent`]. The initial indentation is
    /// used on the very first line of output. Setting it to something
    /// like `"* "` can be useful if you are formatting an item in a
    /// bulleted list. You will then probably want to set
    /// `self.subsequent_indent` to `"  "`.
    ///
    /// [`self.initial_indent`]: #structfield.initial_indent
    pub fn initial_indent(self, indent: &'a str) -> Wrapper<'a> {
        Wrapper { initial_indent: indent, ..self }
    }

    /// Change [`self.subsequent_indent`]. The subsequent indentation
    /// is used on lines following the first line of output. Setting
    /// it to something like `"  "` can be useful if you are
    /// formatting an item in a bulleted list.
    ///
    /// [`self.subsequent_indent`]: #structfield.subsequent_indent
    pub fn subsequent_indent(self, indent: &'a str) -> Wrapper<'a> {
        Wrapper { subsequent_indent: indent, ..self }
    }

    /// Change [`self.break_words`]. This controls if words longer
    /// than `self.width` can be broken, or if they will be left
    /// sticking out into the right margin.
    ///
    /// [`self.break_words`]: #structfield.break_words
    pub fn break_words(self, setting: bool) -> Wrapper<'a> {
        Wrapper { break_words: setting, ..self }
    }

    /// Change [`self.squeeze_whitespace`]. This controls if
    /// whitespace betweee words is squeezed together to a single
    /// space. Regardless of this setting, all whitespace characters
    /// are converted to space (`' '`) characters.
    ///
    /// [`self.squeeze_whitespace`]: #structfield.squeeze_whitespace
    pub fn squeeze_whitespace(self, setting: bool) -> Wrapper<'a> {
        Wrapper { squeeze_whitespace: setting, ..self }
    }

    /// Change [`self.splitter`]. The word splitter is consulted when
    /// a word is too wide to fit the current line. By changing this,
    /// you can decide if such words should be hyphenated or left
    /// alone. Hyphenation can be done using existing hyphens (see
    /// [`HyphenSplitter`]) or it can be based on TeX hyphenation
    /// patterns, if the `hyphenation` feature is enabled.
    ///
    /// [`self.splitter`]: #structfield.splitter
    pub fn word_splitter(self, splitter: Box<WordSplitter>) -> Wrapper<'a> {
        Wrapper { splitter: splitter, ..self }
    }

    /// Fill a line of text at `self.width` characters. Strings are
    /// wrapped based on their displayed width, not their size in
    /// bytes.
    ///
    /// The result is a string with newlines between each line. Use
    /// the `wrap` method if you need access to the individual lines.
    ///
    /// ```
    /// use textwrap::Wrapper;
    ///
    /// let wrapper = Wrapper::new(15);
    /// assert_eq!(wrapper.fill("Memory safety without garbage collection."),
    ///            "Memory safety\nwithout garbage\ncollection.");
    /// ```
    ///
    /// This method simply joins the lines produced by `wrap`. As
    /// such, it inherits the O(*n*) time and memory complexity where
    /// *n* is the input string length.
    pub fn fill(&self, s: &str) -> String {
        self.wrap(s).join("\n")
    }

    /// Wrap a line of text at `self.width` characters. Strings are
    /// wrapped based on their displayed width, not their size in
    /// bytes.
    ///
    /// ```
    /// use textwrap::Wrapper;
    ///
    /// let wrap15 = Wrapper::new(15);
    /// assert_eq!(wrap15.wrap("Concurrency without data races."),
    ///            vec!["Concurrency",
    ///                 "without data",
    ///                 "races."]);
    ///
    /// let wrap20 = Wrapper::new(20);
    /// assert_eq!(wrap20.wrap("Concurrency without data races."),
    ///            vec!["Concurrency without",
    ///                 "data races."]);
    /// ```
    ///
    /// The [`WordSplitter`] stored in [`self.splitter`] is used
    /// whenever when a word is too large to fit on the current line.
    /// By changing the field, different hyphenation strategies can be
    /// implemented.
    ///
    /// This method does a single scan over the input string, it has
    /// an O(*n*) time and memory complexity where *n* is the input
    /// string length.
    ///
    /// [`self.splitter`]: #structfield.splitter
    /// [`WordSplitter`]: trait.WordSplitter.html
    ///
    pub fn wrap(&self, s: &str) -> Vec<String> {
        let mut lines = Vec::with_capacity(s.len() / (self.width + 1));
        let mut line = IndentedString::new(self.initial_indent, self.width);
        let mut remaining = self.width - self.initial_indent.width();
        const NBSP: char = '\u{a0}'; // non-breaking space

        for mut word in s.split(|c: char| c.is_whitespace() && c != NBSP) {
            // Skip over adjacent whitespace characters.
            if self.squeeze_whitespace && word.is_empty() {
                continue;
            }

            // Attempt to fit the word without any splitting.
            if self.fit_part(word, "", &mut remaining, &mut line) {
                continue;
            }

            // If that failed, loop until nothing remains to be added.
            while !word.is_empty() {
                let splits = self.splitter.split(word);
                let (smallest, hyphen, longest) = splits[0];
                let min_width = smallest.width() + hyphen.len();

                // Add a new line if even the smallest split doesn't
                // fit.
                if !line.is_empty() && 1 + min_width > remaining {
                    lines.push(line.into_string());
                    line = IndentedString::new(self.subsequent_indent, self.width);
                    remaining = self.width - self.subsequent_indent.width();
                }

                // Find a split that fits on the current line.
                for &(head, hyphen, tail) in splits.iter().rev() {
                    if self.fit_part(head, hyphen, &mut remaining, &mut line) {
                        word = tail;
                        break;
                    }
                }

                // If even the smallest split doesn't fit on the line,
                // we might have to break the word.
                if line.is_empty() {
                    if self.break_words && self.width > 1 {
                        // Break word on a character boundary as close
                        // to self.width as possible. Characters are
                        // at most 2 columns wide, so we will chop off
                        // at least one character.
                        let mut head_width = 0;
                        for (idx, c) in word.char_indices() {
                            head_width += c.width().unwrap_or(0);
                            if head_width > remaining {
                                let (head, tail) = word.split_at(idx);
                                line.push_str(head);
                                lines.push(line.into_string());
                                line = IndentedString::new(self.subsequent_indent, self.width);
                                word = tail;
                                break;
                            }
                        }
                    } else {
                        // We forcibly add the smallest split and
                        // continue with the longest tail. This will
                        // result in a line longer than self.width.
                        lines.push(String::from(smallest) + hyphen);
                        remaining = self.width;
                        word = longest;
                    }
                }
            }
        }
        if !line.is_empty() {
            lines.push(line.into_string());
        }
        lines
    }

    /// Try to fit a word (or part of a word) onto a line. The line
    /// and the remaining width is updated as appropriate if the word
    /// or part fits.
    fn fit_part<'b>(&self,
                    part: &'b str,
                    hyphen: &'b str,
                    remaining: &mut usize,
                    line: &mut IndentedString)
                    -> bool {
        let space = if line.is_empty() { 0 } else { 1 };
        let space_needed = space + part.width() + hyphen.len();
        let fits_in_line = space_needed <= *remaining;
        if fits_in_line {
            if !line.is_empty() {
                line.push(' ');
            }
            line.push_str(part);
            line.push_str(hyphen);
            *remaining -= space_needed;
        }

        fits_in_line
    }
}

/// Fill a line of text at `width` characters. Strings are wrapped
/// based on their displayed width, not their size in bytes.
///
/// The result is a string with newlines between each line. Use `wrap`
/// if you need access to the individual lines.
///
/// ```
/// use textwrap::fill;
///
/// assert_eq!(fill("Memory safety without garbage collection.", 15),
///            "Memory safety\nwithout garbage\ncollection.");
/// ```
///
/// This function creates a Wrapper on the fly with default settings.
/// If you need to set a language corpus for automatic hyphenation, or
/// need to fill many strings, then it is suggested to create Wrapper
/// and call its [`fill` method](struct.Wrapper.html#method.fill).
pub fn fill(s: &str, width: usize) -> String {
    wrap(s, width).join("\n")
}

/// Wrap a line of text at `width` characters. Strings are wrapped
/// based on their displayed width, not their size in bytes.
///
/// ```
/// use textwrap::wrap;
///
/// assert_eq!(wrap("Concurrency without data races.", 15),
///            vec!["Concurrency",
///                 "without data",
///                 "races."]);
///
/// assert_eq!(wrap("Concurrency without data races.", 20),
///            vec!["Concurrency without",
///                 "data races."]);
/// ```
///
/// This function creates a Wrapper on the fly with default settings.
/// If you need to set a language corpus for automatic hyphenation, or
/// need to wrap many strings, then it is suggested to create Wrapper
/// and call its [`wrap` method](struct.Wrapper.html#method.wrap).
pub fn wrap(s: &str, width: usize) -> Vec<String> {
    Wrapper::new(width).wrap(s)
}

/// Add prefix to each non-empty line.
///
/// ```
/// use textwrap::indent;
///
/// assert_eq!(indent("Foo\nBar\n", "  "), "  Foo\n  Bar\n");
/// ```
///
/// Empty lines (lines consisting only of whitespace) are not indented
/// and the whitespace is replaced by a single newline (`\n`):
///
/// ```
/// use textwrap::indent;
///
/// assert_eq!(indent("Foo\n\nBar\n  \t  \nBaz\n", "  "),
///            "  Foo\n\n  Bar\n\n  Baz\n");
/// ```
///
/// Leading and trailing whitespace on non-empty lines is kept
/// unchanged:
///
/// ```
/// use textwrap::indent;
///
/// assert_eq!(indent(" \t  Foo   ", "  "), "   \t  Foo   \n");
/// ```
pub fn indent(s: &str, prefix: &str) -> String {
    let mut result = String::new();
    for line in s.lines() {
        if line.chars().any(|c| !c.is_whitespace()) {
            result.push_str(prefix);
            result.push_str(line);
        }
        result.push('\n');
    }
    result
}

/// Removes common leading whitespace from each line.
///
/// This will look at each non-empty line and determine the maximum
/// amount of whitespace that can be removed from the line.
///
/// ```
/// use textwrap::dedent;
///
/// assert_eq!(dedent("  1st line\n  2nd line\n"),
///            "1st line\n2nd line\n");
/// ```
pub fn dedent(s: &str) -> String {
    let mut prefix = String::new();
    let mut lines = s.lines();

    // We first search for a non-empty line to find a prefix.
    for line in &mut lines {
        let whitespace = line.chars()
            .take_while(|c| c.is_whitespace())
            .collect::<String>();
        // Check if the line had anything but whitespace
        if whitespace.len() < line.len() {
            prefix = whitespace;
            break;
        }
    }

    // We then continue looking through the remaining lines to
    // possibly shorten the prefix.
    for line in &mut lines {
        let whitespace = line.chars()
            .zip(prefix.chars())
            .take_while(|&(a, b)| a == b)
            .map(|(_, b)| b)
            .collect::<String>();
        // Check if we have found a shorter prefix
        if whitespace.len() < prefix.len() {
            prefix = whitespace;
        }
    }

    // We now go over the lines a second time to build the result.
    let mut result = String::new();
    for line in s.lines() {
        if line.starts_with(&prefix) && line.chars().any(|c| !c.is_whitespace()) {
            let (_, tail) = line.split_at(prefix.len());
            result.push_str(tail);
        }
        result.push('\n');
    }
    result
}

#[cfg(test)]
mod tests {
    #[cfg(feature = "hyphenation")]
    extern crate hyphenation;

    #[cfg(feature = "hyphenation")]
    use hyphenation::Language;
    use super::*;

    /// Add newlines. Ensures that the final line in the vector also
    /// has a newline.
    fn add_nl(lines: &Vec<&str>) -> String {
        lines.join("\n") + "\n"
    }

    #[test]
    fn no_wrap() {
        assert_eq!(wrap("foo", 10), vec!["foo"]);
    }

    #[test]
    fn simple() {
        assert_eq!(wrap("foo bar baz", 5), vec!["foo", "bar", "baz"]);
    }

    #[test]
    fn multi_word_on_line() {
        assert_eq!(wrap("foo bar baz", 10), vec!["foo bar", "baz"]);
    }

    #[test]
    fn long_word() {
        assert_eq!(wrap("foo", 0), vec!["foo"]);
    }

    #[test]
    fn long_words() {
        assert_eq!(wrap("foo bar", 0), vec!["foo", "bar"]);
    }

    #[test]
    fn whitespace_is_significant() {
        assert_eq!(wrap("foo:   bar baz", 10), vec!["foo:   bar", "baz"]);
    }

    #[test]
    fn extra_whitespace_start_of_line() {
        // Whitespace is only significant inside a line. After a line
        // gets too long and is broken, the first word starts in
        // column zero and is not indented. The line before might end
        // up with trailing whitespace.
        assert_eq!(wrap("foo               bar", 5), vec!["foo  ", "bar"]);
    }

    #[test]
    fn whitespace_is_squeezed() {
        let wrapper = Wrapper::new(10).squeeze_whitespace(true);
        assert_eq!(wrapper.wrap(" foo \t  bar  "), vec!["foo bar"]);
    }

    #[test]
    fn wide_character_handling() {
        assert_eq!(wrap("Hello, World!", 15), vec!["Hello, World!"]);
        assert_eq!(wrap("Ｈｅｌｌｏ, Ｗｏｒｌｄ!", 15),
                   vec!["Ｈｅｌｌｏ,", "Ｗｏｒｌｄ!"]);
    }

    #[test]
    fn indent_empty() {
        let wrapper = Wrapper::new(10).initial_indent("!!!");
        assert_eq!(wrapper.fill(""), "");
    }

    #[test]
    fn indent_single_line() {
        let wrapper = Wrapper::new(10).initial_indent(">>>"); // No trailing space
        assert_eq!(wrapper.fill("foo"), ">>>foo");
    }

    #[test]
    fn indent_multiple_lines() {
        let wrapper = Wrapper::new(6).initial_indent("* ").subsequent_indent("  ");
        assert_eq!(wrapper.wrap("foo bar baz"), vec!["* foo", "  bar", "  baz"]);
    }

    #[test]
    fn indent_break_words() {
        let wrapper = Wrapper::new(5).initial_indent("* ").subsequent_indent("  ");
        assert_eq!(wrapper.wrap("foobarbaz"), vec!["* foo", "  bar", "  baz"]);
    }

    #[test]
    fn hyphens() {
        assert_eq!(wrap("foo-bar", 5), vec!["foo-", "bar"]);
    }

    #[test]
    fn trailing_hyphen() {
        let wrapper = Wrapper::new(5).break_words(false);
        assert_eq!(wrapper.wrap("foobar-"), vec!["foobar-"]);
    }

    #[test]
    fn multiple_hyphens() {
        assert_eq!(wrap("foo-bar-baz", 5), vec!["foo-", "bar-", "baz"]);
    }

    #[test]
    fn hyphens_flag() {
        let wrapper = Wrapper::new(5).break_words(false);
        assert_eq!(wrapper.wrap("The --foo-bar flag."),
                   vec!["The", "--foo-", "bar", "flag."]);
    }

    #[test]
    fn repeated_hyphens() {
        let wrapper = Wrapper::new(4).break_words(false);
        assert_eq!(wrapper.wrap("foo--bar"), vec!["foo--bar"]);
    }

    #[test]
    fn hyphens_alphanumeric() {
        assert_eq!(wrap("Na2-CH4", 5), vec!["Na2-", "CH4"]);
    }

    #[test]
    fn hyphens_non_alphanumeric() {
        let wrapper = Wrapper::new(5).break_words(false);
        assert_eq!(wrapper.wrap("foo(-)bar"), vec!["foo(-)bar"]);
    }

    #[test]
    fn multiple_splits() {
        assert_eq!(wrap("foo-bar-baz", 9), vec!["foo-bar-", "baz"]);
    }

    #[test]
    fn forced_split() {
        let wrapper = Wrapper::new(5).break_words(false);
        assert_eq!(wrapper.wrap("foobar-baz"), vec!["foobar-", "baz"]);
    }

    #[test]
    fn no_hyphenation() {
        let wrapper = Wrapper::new(8).word_splitter(Box::new(NoHyphenation));
        assert_eq!(wrapper.wrap("foo bar-baz"), vec!["foo", "bar-baz"]);
    }

    #[test]
    #[cfg(feature = "hyphenation")]
    fn auto_hyphenation() {
        let corpus = hyphenation::load(Language::English_US).unwrap();
        let wrapper = Wrapper::new(10);
        assert_eq!(wrapper.wrap("Internationalization"),
                   vec!["Internatio", "nalization"]);

        let wrapper = wrapper.word_splitter(Box::new(corpus));
        assert_eq!(wrapper.wrap("Internationalization"),
                   vec!["Interna-", "tionaliza-", "tion"]);
    }

    #[test]
    #[cfg(feature = "hyphenation")]
    fn auto_hyphenation_with_hyphen() {
        let corpus = hyphenation::load(Language::English_US).unwrap();
        let wrapper = Wrapper::new(8).break_words(false);
        assert_eq!(wrapper.wrap("over-caffinated"), vec!["over-", "caffinated"]);

        let wrapper = wrapper.word_splitter(Box::new(corpus));
        assert_eq!(wrapper.wrap("over-caffinated"),
                   vec!["over-", "caffi-", "nated"]);
    }

    #[test]
    fn break_words() {
        assert_eq!(wrap("foobarbaz", 3), vec!["foo", "bar", "baz"]);
    }

    #[test]
    fn break_words_wide_characters() {
        assert_eq!(wrap("Ｈｅｌｌｏ", 5), vec!["Ｈｅ", "ｌｌ", "ｏ"]);
    }

    #[test]
    fn break_words_zero_width() {
        assert_eq!(wrap("foobar", 0), vec!["foobar"]);
    }

    #[test]
    fn test_non_breaking_space() {
        let wrapper = Wrapper::new(5).break_words(false);
        assert_eq!(wrapper.fill("foo bar baz"), "foo bar baz");
    }

    #[test]
    fn test_non_breaking_hyphen() {
        let wrapper = Wrapper::new(5).break_words(false);
        assert_eq!(wrapper.fill("foo‑bar‑baz"), "foo‑bar‑baz");
    }

    #[test]
    fn test_fill() {
        assert_eq!(fill("foo bar baz", 10), "foo bar\nbaz");
    }

    #[test]
    fn test_indent_empty() {
        assert_eq!(indent("\n", "  "), "\n");
    }

    #[test]
    #[cfg_attr(rustfmt, rustfmt_skip)]
    fn test_indent_nonempty() {
        let x = vec!["  foo",
                     "bar",
                     "  baz"];
        let y = vec!["//  foo",
                     "//bar",
                     "//  baz"];
        assert_eq!(indent(&add_nl(&x), "//"), add_nl(&y));
    }

    #[test]
    #[cfg_attr(rustfmt, rustfmt_skip)]
    fn test_indent_empty_line() {
        let x = vec!["  foo",
                     "bar",
                     "",
                     "  baz"];
        let y = vec!["//  foo",
                     "//bar",
                     "",
                     "//  baz"];
        assert_eq!(indent(&add_nl(&x), "//"), add_nl(&y));
    }

    #[test]
    fn test_dedent_empty() {
        assert_eq!(dedent(""), "");
    }

    #[test]
    #[cfg_attr(rustfmt, rustfmt_skip)]
    fn test_dedent_multi_line() {
        let x = vec!["    foo",
                     "  bar",
                     "    baz"];
        let y = vec!["  foo",
                     "bar",
                     "  baz"];
        assert_eq!(dedent(&add_nl(&x)), add_nl(&y));
    }

    #[test]
    #[cfg_attr(rustfmt, rustfmt_skip)]
    fn test_dedent_empty_line() {
        let x = vec!["    foo",
                     "  bar",
                     "   ",
                     "    baz"];
        let y = vec!["  foo",
                     "bar",
                     "",
                     "  baz"];
        assert_eq!(dedent(&add_nl(&x)), add_nl(&y));
    }

    #[test]
    #[cfg_attr(rustfmt, rustfmt_skip)]
    fn test_dedent_mixed_whitespace() {
        let x = vec!["\tfoo",
                     "  bar"];
        let y = vec!["\tfoo",
                     "  bar"];
        assert_eq!(dedent(&add_nl(&x)), add_nl(&y));
    }
}
