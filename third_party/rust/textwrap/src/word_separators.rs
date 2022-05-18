//! Functionality for finding words.
//!
//! In order to wrap text, we need to know where the legal break
//! points are, i.e., where the words of the text are. This means that
//! we need to define what a "word" is.
//!
//! A simple approach is to simply split the text on whitespace, but
//! this does not work for East-Asian languages such as Chinese or
//! Japanese where there are no spaces between words. Breaking a long
//! sequence of emojis is another example where line breaks might be
//! wanted even if there are no whitespace to be found.
//!
//! The [`WordSeparator`] trait is responsible for determining where
//! there words are in a line of text. Please refer to the trait and
//! the structs which implement it for more information.

#[cfg(feature = "unicode-linebreak")]
use crate::core::skip_ansi_escape_sequence;
use crate::core::Word;

/// Describes where words occur in a line of text.
///
/// The simplest approach is say that words are separated by one or
/// more ASCII spaces (`' '`). This works for Western languages
/// without emojis. A more complex approach is to use the Unicode line
/// breaking algorithm, which finds break points in non-ASCII text.
///
/// The line breaks occur between words, please see the
/// [`WordSplitter`](crate::word_splitters::WordSplitter) trait for
/// options of how to handle hyphenation of individual words.
///
/// # Examples
///
/// ```
/// use textwrap::core::Word;
/// use textwrap::word_separators::{WordSeparator, AsciiSpace};
///
/// let words = AsciiSpace.find_words("Hello World!").collect::<Vec<_>>();
/// assert_eq!(words, vec![Word::from("Hello "), Word::from("World!")]);
/// ```
pub trait WordSeparator: WordSeparatorClone + std::fmt::Debug {
    // This trait should really return impl Iterator<Item = Word>, but
    // this isn't possible until Rust supports higher-kinded types:
    // https://github.com/rust-lang/rfcs/blob/master/text/1522-conservative-impl-trait.md
    /// Find all words in `line`.
    fn find_words<'a>(&self, line: &'a str) -> Box<dyn Iterator<Item = Word<'a>> + 'a>;
}

// The internal `WordSeparatorClone` trait is allows us to implement
// `Clone` for `Box<dyn WordSeparator>`. This in used in the
// `From<&Options<'_, WrapAlgo, WordSep, WordSplit>> for Options<'a,
// WrapAlgo, WordSep, WordSplit>` implementation.
#[doc(hidden)]
pub trait WordSeparatorClone {
    fn clone_box(&self) -> Box<dyn WordSeparator>;
}

impl<T: WordSeparator + Clone + 'static> WordSeparatorClone for T {
    fn clone_box(&self) -> Box<dyn WordSeparator> {
        Box::new(self.clone())
    }
}

impl Clone for Box<dyn WordSeparator> {
    fn clone(&self) -> Box<dyn WordSeparator> {
        use std::ops::Deref;
        self.deref().clone_box()
    }
}

impl WordSeparator for Box<dyn WordSeparator> {
    fn find_words<'a>(&self, line: &'a str) -> Box<dyn Iterator<Item = Word<'a>> + 'a> {
        use std::ops::Deref;
        self.deref().find_words(line)
    }
}

/// Find words by splitting on regions of `' '` characters.
#[derive(Clone, Copy, Debug, Default)]
pub struct AsciiSpace;

/// Split `line` into words separated by regions of `' '` characters.
///
/// # Examples
///
/// ```
/// use textwrap::core::Word;
/// use textwrap::word_separators::{AsciiSpace, WordSeparator};
///
/// let words = AsciiSpace.find_words("Hello   World!").collect::<Vec<_>>();
/// assert_eq!(words, vec![Word::from("Hello   "),
///                        Word::from("World!")]);
/// ```
impl WordSeparator for AsciiSpace {
    fn find_words<'a>(&self, line: &'a str) -> Box<dyn Iterator<Item = Word<'a>> + 'a> {
        let mut start = 0;
        let mut in_whitespace = false;
        let mut char_indices = line.char_indices();

        Box::new(std::iter::from_fn(move || {
            // for (idx, ch) in char_indices does not work, gives this
            // error:
            //
            // > cannot move out of `char_indices`, a captured variable in
            // > an `FnMut` closure
            #[allow(clippy::while_let_on_iterator)]
            while let Some((idx, ch)) = char_indices.next() {
                if in_whitespace && ch != ' ' {
                    let word = Word::from(&line[start..idx]);
                    start = idx;
                    in_whitespace = ch == ' ';
                    return Some(word);
                }

                in_whitespace = ch == ' ';
            }

            if start < line.len() {
                let word = Word::from(&line[start..]);
                start = line.len();
                return Some(word);
            }

            None
        }))
    }
}

/// Find words using the Unicode line breaking algorithm.
#[cfg(feature = "unicode-linebreak")]
#[derive(Clone, Copy, Debug, Default)]
pub struct UnicodeBreakProperties;

/// Split `line` into words using Unicode break properties.
///
/// This word separator uses the Unicode line breaking algorithm
/// described in [Unicode Standard Annex
/// #14](https://www.unicode.org/reports/tr14/) to find legal places
/// to break lines. There is a small difference in that the U+002D
/// (Hyphen-Minus) and U+00AD (Soft Hyphen) don’t create a line break:
/// to allow a line break at a hyphen, use the
/// [`HyphenSplitter`](crate::word_splitters::HyphenSplitter). Soft
/// hyphens are not currently supported.
///
/// # Examples
///
/// Unlike [`AsciiSpace`], the Unicode line breaking algorithm will
/// find line break opportunities between some characters with no
/// intervening whitespace:
///
/// ```
/// #[cfg(feature = "unicode-linebreak")] {
/// use textwrap::word_separators::{WordSeparator, UnicodeBreakProperties};
/// use textwrap::core::Word;
///
/// assert_eq!(UnicodeBreakProperties.find_words("Emojis: 😂😍").collect::<Vec<_>>(),
///            vec![Word::from("Emojis: "),
///                 Word::from("😂"),
///                 Word::from("😍")]);
///
/// assert_eq!(UnicodeBreakProperties.find_words("CJK: 你好").collect::<Vec<_>>(),
///            vec![Word::from("CJK: "),
///                 Word::from("你"),
///                 Word::from("好")]);
/// }
/// ```
///
/// A U+2060 (Word Joiner) character can be inserted if you want to
/// manually override the defaults and keep the characters together:
///
/// ```
/// #[cfg(feature = "unicode-linebreak")] {
/// use textwrap::word_separators::{UnicodeBreakProperties, WordSeparator};
/// use textwrap::core::Word;
///
/// assert_eq!(UnicodeBreakProperties.find_words("Emojis: 😂\u{2060}😍").collect::<Vec<_>>(),
///            vec![Word::from("Emojis: "),
///                 Word::from("😂\u{2060}😍")]);
/// }
/// ```
///
/// The Unicode line breaking algorithm will also automatically
/// suppress break breaks around certain punctuation characters::
///
/// ```
/// #[cfg(feature = "unicode-linebreak")] {
/// use textwrap::word_separators::{UnicodeBreakProperties, WordSeparator};
/// use textwrap::core::Word;
///
/// assert_eq!(UnicodeBreakProperties.find_words("[ foo ] bar !").collect::<Vec<_>>(),
///            vec![Word::from("[ foo ] "),
///                 Word::from("bar !")]);
/// }
/// ```
#[cfg(feature = "unicode-linebreak")]
impl WordSeparator for UnicodeBreakProperties {
    fn find_words<'a>(&self, line: &'a str) -> Box<dyn Iterator<Item = Word<'a>> + 'a> {
        // Construct an iterator over (original index, stripped index)
        // tuples. We find the Unicode linebreaks on a stripped string,
        // but we need the original indices so we can form words based on
        // the original string.
        let mut last_stripped_idx = 0;
        let mut char_indices = line.char_indices();
        let mut idx_map = std::iter::from_fn(move || match char_indices.next() {
            Some((orig_idx, ch)) => {
                let stripped_idx = last_stripped_idx;
                if !skip_ansi_escape_sequence(ch, &mut char_indices.by_ref().map(|(_, ch)| ch)) {
                    last_stripped_idx += ch.len_utf8();
                }
                Some((orig_idx, stripped_idx))
            }
            None => None,
        });

        let stripped = strip_ansi_escape_sequences(&line);
        let mut opportunities = unicode_linebreak::linebreaks(&stripped)
            .filter(|(idx, _)| {
                #[allow(clippy::match_like_matches_macro)]
                match &stripped[..*idx].chars().next_back() {
                    // We suppress breaks at ‘-’ since we want to control
                    // this via the WordSplitter.
                    Some('-') => false,
                    // Soft hyphens are currently not supported since we
                    // require all `Word` fragments to be continuous in
                    // the input string.
                    Some(SHY) => false,
                    // Other breaks should be fine!
                    _ => true,
                }
            })
            .collect::<Vec<_>>()
            .into_iter();

        // Remove final break opportunity, we will add it below using
        // &line[start..]; This ensures that we correctly include a
        // trailing ANSI escape sequence.
        opportunities.next_back();

        let mut start = 0;
        Box::new(std::iter::from_fn(move || {
            #[allow(clippy::while_let_on_iterator)]
            while let Some((idx, _)) = opportunities.next() {
                if let Some((orig_idx, _)) = idx_map.find(|&(_, stripped_idx)| stripped_idx == idx)
                {
                    let word = Word::from(&line[start..orig_idx]);
                    start = orig_idx;
                    return Some(word);
                }
            }

            if start < line.len() {
                let word = Word::from(&line[start..]);
                start = line.len();
                return Some(word);
            }

            None
        }))
    }
}

/// Soft hyphen, also knows as a “shy hyphen”. Should show up as ‘-’
/// if a line is broken at this point, and otherwise be invisible.
/// Textwrap does not currently support breaking words at soft
/// hyphens.
#[cfg(feature = "unicode-linebreak")]
const SHY: char = '\u{00ad}';

// Strip all ANSI escape sequences from `text`.
#[cfg(feature = "unicode-linebreak")]
fn strip_ansi_escape_sequences(text: &str) -> String {
    let mut result = String::with_capacity(text.len());

    let mut chars = text.chars();
    while let Some(ch) = chars.next() {
        if skip_ansi_escape_sequence(ch, &mut chars) {
            continue;
        }
        result.push(ch);
    }

    result
}

#[cfg(test)]
mod tests {
    use super::*;

    // Like assert_eq!, but the left expression is an iterator.
    macro_rules! assert_iter_eq {
        ($left:expr, $right:expr) => {
            assert_eq!($left.collect::<Vec<_>>(), $right);
        };
    }

    #[test]
    fn ascii_space_empty() {
        assert_iter_eq!(AsciiSpace.find_words(""), vec![]);
    }

    #[test]
    fn ascii_space_single_word() {
        assert_iter_eq!(AsciiSpace.find_words("foo"), vec![Word::from("foo")]);
    }

    #[test]
    fn ascii_space_two_words() {
        assert_iter_eq!(
            AsciiSpace.find_words("foo bar"),
            vec![Word::from("foo "), Word::from("bar")]
        );
    }

    #[test]
    fn ascii_space_multiple_words() {
        assert_iter_eq!(
            AsciiSpace.find_words("foo bar baz"),
            vec![Word::from("foo "), Word::from("bar "), Word::from("baz")]
        );
    }

    #[test]
    fn ascii_space_only_whitespace() {
        assert_iter_eq!(AsciiSpace.find_words("    "), vec![Word::from("    ")]);
    }

    #[test]
    fn ascii_space_inter_word_whitespace() {
        assert_iter_eq!(
            AsciiSpace.find_words("foo   bar"),
            vec![Word::from("foo   "), Word::from("bar")]
        )
    }

    #[test]
    fn ascii_space_trailing_whitespace() {
        assert_iter_eq!(AsciiSpace.find_words("foo   "), vec![Word::from("foo   ")]);
    }

    #[test]
    fn ascii_space_leading_whitespace() {
        assert_iter_eq!(
            AsciiSpace.find_words("   foo"),
            vec![Word::from("   "), Word::from("foo")]
        );
    }

    #[test]
    fn ascii_space_multi_column_char() {
        assert_iter_eq!(
            AsciiSpace.find_words("\u{1f920}"), // cowboy emoji 🤠
            vec![Word::from("\u{1f920}")]
        );
    }

    #[test]
    fn ascii_space_hyphens() {
        assert_iter_eq!(
            AsciiSpace.find_words("foo-bar"),
            vec![Word::from("foo-bar")]
        );
        assert_iter_eq!(
            AsciiSpace.find_words("foo- bar"),
            vec![Word::from("foo- "), Word::from("bar")]
        );
        assert_iter_eq!(
            AsciiSpace.find_words("foo - bar"),
            vec![Word::from("foo "), Word::from("- "), Word::from("bar")]
        );
        assert_iter_eq!(
            AsciiSpace.find_words("foo -bar"),
            vec![Word::from("foo "), Word::from("-bar")]
        );
    }

    #[test]
    #[cfg(unix)]
    fn ascii_space_colored_text() {
        use termion::color::{Blue, Fg, Green, Reset};

        let green_hello = format!("{}Hello{} ", Fg(Green), Fg(Reset));
        let blue_world = format!("{}World!{}", Fg(Blue), Fg(Reset));
        assert_iter_eq!(
            AsciiSpace.find_words(&format!("{}{}", green_hello, blue_world)),
            vec![Word::from(&green_hello), Word::from(&blue_world)]
        );

        #[cfg(feature = "unicode-linebreak")]
        assert_iter_eq!(
            UnicodeBreakProperties.find_words(&format!("{}{}", green_hello, blue_world)),
            vec![Word::from(&green_hello), Word::from(&blue_world)]
        );
    }

    #[test]
    fn ascii_space_color_inside_word() {
        let text = "foo\u{1b}[0m\u{1b}[32mbar\u{1b}[0mbaz";
        assert_iter_eq!(AsciiSpace.find_words(&text), vec![Word::from(text)]);

        #[cfg(feature = "unicode-linebreak")]
        assert_iter_eq!(
            UnicodeBreakProperties.find_words(&text),
            vec![Word::from(text)]
        );
    }
}
