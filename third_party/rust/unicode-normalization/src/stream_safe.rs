use normalize::{
    hangul_decomposition_length,
    is_hangul_syllable,
};
use tables;

pub(crate) const MAX_NONSTARTERS: usize = 30;
const COMBINING_GRAPHEME_JOINER: char = '\u{034F}';

/// UAX15-D4: This iterator keeps track of how many non-starters there have been
/// since the last starter in *NFKD* and will emit a Combining Grapheme Joiner
/// (U+034F) if the count exceeds 30.
pub struct StreamSafe<I> {
    iter: I,
    nonstarter_count: usize,
    buffer: Option<char>,
}

impl<I> StreamSafe<I> {
    pub(crate) fn new(iter: I) -> Self {
        Self { iter, nonstarter_count: 0, buffer: None }
    }
}

impl<I: Iterator<Item=char>> Iterator for StreamSafe<I> {
    type Item = char;

    #[inline]
    fn next(&mut self) -> Option<char> {
        if let Some(ch) = self.buffer.take() {
            return Some(ch);
        }
        let next_ch = match self.iter.next() {
            None => return None,
            Some(c) => c,
        };
        let d = classify_nonstarters(next_ch);
        if self.nonstarter_count + d.leading_nonstarters > MAX_NONSTARTERS {
            self.buffer = Some(next_ch);
            self.nonstarter_count = 0;
            return Some(COMBINING_GRAPHEME_JOINER);
        }

        // No starters in the decomposition, so keep accumulating
        if d.leading_nonstarters == d.decomposition_len {
            self.nonstarter_count += d.decomposition_len;
        }
        // Otherwise, restart the nonstarter counter.
        else {
            self.nonstarter_count = d.trailing_nonstarters;
        }
        Some(next_ch)
    }
}

#[derive(Debug)]
pub(crate) struct Decomposition {
    pub(crate) leading_nonstarters: usize,
    pub(crate) trailing_nonstarters: usize,
    pub(crate) decomposition_len: usize,
}

#[inline]
pub(crate) fn classify_nonstarters(c: char) -> Decomposition {
    // As usual, fast path for ASCII (which is always a starter)
    if c <= '\x7f' {
        return Decomposition {
            leading_nonstarters: 0,
            trailing_nonstarters: 0,
            decomposition_len: 1,
        }
    }
    // Next, special case Hangul, since it's not handled by our tables.
    if is_hangul_syllable(c) {
        return Decomposition {
            leading_nonstarters: 0,
            trailing_nonstarters: 0,
            decomposition_len: hangul_decomposition_length(c),
        };
    }
    let decomp = tables::compatibility_fully_decomposed(c)
        .or_else(|| tables::canonical_fully_decomposed(c));
    match decomp {
        Some(decomp) => {
            Decomposition {
                leading_nonstarters: tables::stream_safe_leading_nonstarters(c),
                trailing_nonstarters: tables::stream_safe_trailing_nonstarters(c),
                decomposition_len: decomp.len(),
            }
        },
        None => {
            let is_nonstarter = tables::canonical_combining_class(c) != 0;
            let nonstarter = if is_nonstarter { 1 } else { 0 };
            Decomposition {
                leading_nonstarters: nonstarter,
                trailing_nonstarters: nonstarter,
                decomposition_len: 1,
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::{
        StreamSafe,
        classify_nonstarters,
    };
    use std::char;
    use normalization_tests::NORMALIZATION_TESTS;
    use normalize::decompose_compatible;
    use tables;

    fn stream_safe(s: &str) -> String {
        StreamSafe::new(s.chars()).collect()
    }

    #[test]
    fn test_normalization_tests_unaffected() {
        for test in NORMALIZATION_TESTS {
            for &s in &[test.source, test.nfc, test.nfd, test.nfkc, test.nfkd] {
                assert_eq!(stream_safe(s), s);
            }
        }
    }

    #[test]
    fn test_simple() {
        let technically_okay = "Da\u{0300}\u{0301}\u{0302}\u{0303}\u{0304}\u{0305}\u{0306}\u{0307}\u{0308}\u{0309}\u{030a}\u{030b}\u{030c}\u{030d}\u{030e}\u{030f}\u{0310}\u{0311}\u{0312}\u{0313}\u{0314}\u{0315}\u{0316}\u{0317}\u{0318}\u{0319}\u{031a}\u{031b}\u{031c}\u{031d}ngerzone";
        assert_eq!(stream_safe(technically_okay), technically_okay);

        let too_much = "Da\u{0300}\u{0301}\u{0302}\u{0303}\u{0304}\u{0305}\u{0306}\u{0307}\u{0308}\u{0309}\u{030a}\u{030b}\u{030c}\u{030d}\u{030e}\u{030f}\u{0310}\u{0311}\u{0312}\u{0313}\u{0314}\u{0315}\u{0316}\u{0317}\u{0318}\u{0319}\u{031a}\u{031b}\u{031c}\u{031d}\u{032e}ngerzone";
        assert_ne!(stream_safe(too_much), too_much);
    }

    #[test]
    fn test_classify_nonstarters() {
        // Highest character in the `compat_fully_decomp` table is 2FA1D
        for ch in 0..0x2FA1E {
            let ch = match char::from_u32(ch) {
                Some(c) => c,
                None => continue,
            };
            let c = classify_nonstarters(ch);
            let mut s = vec![];
            decompose_compatible(ch, |c| s.push(c));

            assert_eq!(s.len(), c.decomposition_len);

            let num_leading = s
                .iter()
                .take_while(|&c| tables::canonical_combining_class(*c) != 0)
                .count();
            let num_trailing = s
                .iter()
                .rev()
                .take_while(|&c| tables::canonical_combining_class(*c) != 0)
                .count();

            assert_eq!(num_leading, c.leading_nonstarters);
            assert_eq!(num_trailing, c.trailing_nonstarters);
        }
    }
}
