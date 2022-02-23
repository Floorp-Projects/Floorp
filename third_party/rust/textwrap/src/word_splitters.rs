//! Word splitting functionality.
//!
//! To wrap text into lines, long words sometimes need to be split
//! across lines. The [`WordSplitter`] trait defines this
//! functionality. [`HyphenSplitter`] is the default implementation of
//! this treat: it will simply split words on existing hyphens.

use std::ops::Deref;

use crate::core::{display_width, Word};

/// The `WordSplitter` trait describes where words can be split.
///
/// If the textwrap crate has been compiled with the `hyphenation`
/// Cargo feature enabled, you will find an implementation of
/// `WordSplitter` by the `hyphenation::Standard` struct. Use this
/// struct for language-aware hyphenation:
///
/// ```
/// #[cfg(feature = "hyphenation")]
/// {
///     use hyphenation::{Language, Load, Standard};
///     use textwrap::{wrap, Options};
///
///     let text = "Oxidation is the loss of electrons.";
///     let dictionary = Standard::from_embedded(Language::EnglishUS).unwrap();
///     let options = Options::new(8).word_splitter(dictionary);
///     assert_eq!(wrap(text, &options), vec!["Oxida-",
///                                           "tion is",
///                                           "the loss",
///                                           "of elec-",
///                                           "trons."]);
/// }
/// ```
///
/// Please see the documentation for the [hyphenation] crate for more
/// details.
///
/// [hyphenation]: https://docs.rs/hyphenation/
pub trait WordSplitter: WordSplitterClone + std::fmt::Debug {
    /// Return all possible indices where `word` can be split.
    ///
    /// The indices returned must be in range `0..word.len()`. They
    /// should point to the index _after_ the split point, i.e., after
    /// `-` if splitting on hyphens. This way, `word.split_at(idx)`
    /// will break the word into two well-formed pieces.
    ///
    /// # Examples
    ///
    /// ```
    /// use textwrap::word_splitters::{HyphenSplitter, NoHyphenation, WordSplitter};
    /// assert_eq!(NoHyphenation.split_points("cannot-be-split"), vec![]);
    /// assert_eq!(HyphenSplitter.split_points("can-be-split"), vec![4, 7]);
    /// ```
    fn split_points(&self, word: &str) -> Vec<usize>;
}

// The internal `WordSplitterClone` trait is allows us to implement
// `Clone` for `Box<dyn WordSplitter>`. This in used in the
// `From<&Options<'_, WrapAlgo, WordSep, WordSplit>> for Options<'a,
// WrapAlgo, WordSep, WordSplit>` implementation.
#[doc(hidden)]
pub trait WordSplitterClone {
    fn clone_box(&self) -> Box<dyn WordSplitter>;
}

impl<T: WordSplitter + Clone + 'static> WordSplitterClone for T {
    fn clone_box(&self) -> Box<dyn WordSplitter> {
        Box::new(self.clone())
    }
}

impl Clone for Box<dyn WordSplitter> {
    fn clone(&self) -> Box<dyn WordSplitter> {
        self.deref().clone_box()
    }
}

impl WordSplitter for Box<dyn WordSplitter> {
    fn split_points(&self, word: &str) -> Vec<usize> {
        self.deref().split_points(word)
    }
}

/// Use this as a [`Options.word_splitter`] to avoid any kind of
/// hyphenation:
///
/// ```
/// use textwrap::{wrap, Options};
/// use textwrap::word_splitters::NoHyphenation;
///
/// let options = Options::new(8).word_splitter(NoHyphenation);
/// assert_eq!(wrap("foo bar-baz", &options),
///            vec!["foo", "bar-baz"]);
/// ```
///
/// [`Options.word_splitter`]: super::Options::word_splitter
#[derive(Clone, Copy, Debug)]
pub struct NoHyphenation;

/// `NoHyphenation` implements `WordSplitter` by not splitting the
/// word at all.
impl WordSplitter for NoHyphenation {
    fn split_points(&self, _: &str) -> Vec<usize> {
        Vec::new()
    }
}

/// Simple and default way to split words: splitting on existing
/// hyphens only.
///
/// You probably don't need to use this type since it's already used
/// by default by [`Options::new`](super::Options::new).
#[derive(Clone, Copy, Debug)]
pub struct HyphenSplitter;

/// `HyphenSplitter` is the default `WordSplitter` used by
/// [`Options::new`](super::Options::new). It will split words on any
/// existing hyphens in the word.
///
/// It will only use hyphens that are surrounded by alphanumeric
/// characters, which prevents a word like `"--foo-bar"` from being
/// split into `"--"` and `"foo-bar"`.
impl WordSplitter for HyphenSplitter {
    fn split_points(&self, word: &str) -> Vec<usize> {
        let mut splits = Vec::new();

        for (idx, _) in word.match_indices('-') {
            // We only use hyphens that are surrounded by alphanumeric
            // characters. This is to avoid splitting on repeated hyphens,
            // such as those found in --foo-bar.
            let prev = word[..idx].chars().next_back();
            let next = word[idx + 1..].chars().next();

            if prev.filter(|ch| ch.is_alphanumeric()).is_some()
                && next.filter(|ch| ch.is_alphanumeric()).is_some()
            {
                splits.push(idx + 1); // +1 due to width of '-'.
            }
        }

        splits
    }
}

/// A hyphenation dictionary can be used to do language-specific
/// hyphenation using patterns from the [hyphenation] crate.
///
/// **Note:** Only available when the `hyphenation` Cargo feature is
/// enabled.
///
/// [hyphenation]: https://docs.rs/hyphenation/
#[cfg(feature = "hyphenation")]
impl WordSplitter for hyphenation::Standard {
    fn split_points(&self, word: &str) -> Vec<usize> {
        use hyphenation::Hyphenator;
        self.hyphenate(word).breaks
    }
}

/// Split words into smaller words according to the split points given
/// by `word_splitter`.
///
/// Note that we split all words, regardless of their length. This is
/// to more cleanly separate the business of splitting (including
/// automatic hyphenation) from the business of word wrapping.
///
/// # Examples
///
/// ```
/// use textwrap::core::Word;
/// use textwrap::word_splitters::{split_words, NoHyphenation, HyphenSplitter};
///
/// assert_eq!(
///     split_words(vec![Word::from("foo-bar")], &HyphenSplitter).collect::<Vec<_>>(),
///     vec![Word::from("foo-"), Word::from("bar")]
/// );
///
/// // The NoHyphenation splitter ignores the '-':
/// assert_eq!(
///     split_words(vec![Word::from("foo-bar")], &NoHyphenation).collect::<Vec<_>>(),
///     vec![Word::from("foo-bar")]
/// );
/// ```
pub fn split_words<'a, I, WordSplit>(
    words: I,
    word_splitter: &'a WordSplit,
) -> impl Iterator<Item = Word<'a>>
where
    I: IntoIterator<Item = Word<'a>>,
    WordSplit: WordSplitter,
{
    words.into_iter().flat_map(move |word| {
        let mut prev = 0;
        let mut split_points = word_splitter.split_points(&word).into_iter();
        std::iter::from_fn(move || {
            if let Some(idx) = split_points.next() {
                let need_hyphen = !word[..idx].ends_with('-');
                let w = Word {
                    word: &word.word[prev..idx],
                    width: display_width(&word[prev..idx]),
                    whitespace: "",
                    penalty: if need_hyphen { "-" } else { "" },
                };
                prev = idx;
                return Some(w);
            }

            if prev < word.word.len() || prev == 0 {
                let w = Word {
                    word: &word.word[prev..],
                    width: display_width(&word[prev..]),
                    whitespace: word.whitespace,
                    penalty: word.penalty,
                };
                prev = word.word.len() + 1;
                return Some(w);
            }

            None
        })
    })
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
    fn split_words_no_words() {
        assert_iter_eq!(split_words(vec![], &HyphenSplitter), vec![]);
    }

    #[test]
    fn split_words_empty_word() {
        assert_iter_eq!(
            split_words(vec![Word::from("   ")], &HyphenSplitter),
            vec![Word::from("   ")]
        );
    }

    #[test]
    fn split_words_single_word() {
        assert_iter_eq!(
            split_words(vec![Word::from("foobar")], &HyphenSplitter),
            vec![Word::from("foobar")]
        );
    }

    #[test]
    fn split_words_hyphen_splitter() {
        assert_iter_eq!(
            split_words(vec![Word::from("foo-bar")], &HyphenSplitter),
            vec![Word::from("foo-"), Word::from("bar")]
        );
    }

    #[test]
    fn split_words_adds_penalty() {
        #[derive(Clone, Debug)]
        struct FixedSplitPoint;
        impl WordSplitter for FixedSplitPoint {
            fn split_points(&self, _: &str) -> Vec<usize> {
                vec![3]
            }
        }

        assert_iter_eq!(
            split_words(vec![Word::from("foobar")].into_iter(), &FixedSplitPoint),
            vec![
                Word {
                    word: "foo",
                    width: 3,
                    whitespace: "",
                    penalty: "-"
                },
                Word {
                    word: "bar",
                    width: 3,
                    whitespace: "",
                    penalty: ""
                }
            ]
        );

        assert_iter_eq!(
            split_words(vec![Word::from("fo-bar")].into_iter(), &FixedSplitPoint),
            vec![
                Word {
                    word: "fo-",
                    width: 3,
                    whitespace: "",
                    penalty: ""
                },
                Word {
                    word: "bar",
                    width: 3,
                    whitespace: "",
                    penalty: ""
                }
            ]
        );
    }
}
