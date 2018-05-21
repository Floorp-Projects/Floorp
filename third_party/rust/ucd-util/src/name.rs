/// Normalize the given character name in place according to UAX44-LM2.
///
/// See: http://unicode.org/reports/tr44/#UAX44-LM2
pub fn character_name_normalize(string: &mut String) {
    let bytes = unsafe {
        // SAFETY: `character_name_normalize_bytes` guarantees that
        // `bytes[..len]` is valid UTF-8.
        string.as_mut_vec()
    };
    let len = character_name_normalize_bytes(bytes).len();
    bytes.truncate(len);
}

/// Normalize the given character name in place according to UAX44-LM2.
///
/// The slice returned is guaranteed to be valid UTF-8 for all possible values
/// of `slice`.
///
/// See: http://unicode.org/reports/tr44/#UAX44-LM2
fn character_name_normalize_bytes(slice: &mut [u8]) -> &mut [u8] {
    // According to Unicode 4.8, character names consist only of Latin
    // capital letters A to Z, ASCII digits, ASCII space or ASCII hypen.
    // Therefore, we can do very simplistic case folding and operate on the
    // raw bytes, since everything is ASCII. Note that we don't actually know
    // whether `slice` is all ASCII or not, so we drop all non-ASCII bytes.
    let mut next_write = 0;
    let mut prev_space = true;
    for i in 0..slice.len() {
        // SAFETY ARGUMENT: To guarantee that the resulting slice is valid
        // UTF-8, we ensure that the slice contains only ASCII bytes. In
        // particular, we drop every non-ASCII byte from the normalized string.
        let b = slice[i];
        if b == b' ' {
            prev_space = true;
            continue;
        } else if b == b'_' {
            // Drop the underscore.
        } else if b == b'-' {
            let mut keep_hyphen = prev_space || slice.get(i+1) == Some(&b' ');
            // We want to keep the hypen only if it isn't medial, which means
            // it has at least one adjacent space character. However, there
            // is one exception. We need to keep the hypen in the character
            // (U+1180) named `HANGUL JUNGSEONG O-E`. So we check for that
            // here.
            let rest_e = slice[i+1..] == b"E"[..] || slice[i+1..] == b"e"[..];
            if !keep_hyphen && rest_e {
                keep_hyphen = slice[..next_write] == b"hanguljungseongo"[..];
            }
            if keep_hyphen {
                slice[next_write] = b;
                next_write += 1;
            }
        } else if b'A' <= b && b <= b'Z' {
            slice[next_write] = b + (b'a' - b'A');
            next_write += 1;
        } else if b <= 0x7F {
            slice[next_write] = b;
            next_write += 1;
        }
        prev_space = false;
    }
    &mut slice[..next_write]
}

/// Normalize the given symbolic name in place according to UAX44-LM3.
///
/// A "symbolic name" typically corresponds to property names and property
/// value aliases. Note, though, that it should not be applied to property
/// string values.
///
/// See: http://unicode.org/reports/tr44/#UAX44-LM2
pub fn symbolic_name_normalize(string: &mut String) {
    let bytes = unsafe {
        // SAFETY: `symbolic_name_normalize_bytes` guarantees that
        // `bytes[..len]` is valid UTF-8.
        string.as_mut_vec()
    };
    let len = symbolic_name_normalize_bytes(bytes).len();
    bytes.truncate(len);
}

/// Normalize the given symbolic name in place according to UAX44-LM3.
///
/// A "symbolic name" typically corresponds to property names and property
/// value aliases. Note, though, that it should not be applied to property
/// string values.
///
/// The slice returned is guaranteed to be valid UTF-8 for all possible values
/// of `slice`.
///
/// See: http://unicode.org/reports/tr44/#UAX44-LM3
fn symbolic_name_normalize_bytes(slice: &mut [u8]) -> &mut [u8] {
    // I couldn't find a place in the standard that specified that property
    // names/aliases had a particular structure (unlike character names), but
    // we assume that it's ASCII only and drop anything that isn't ASCII.
    let mut start = 0;
    if slice.len() >= 2 {
        // Ignore any "is" prefix.
        let starts_with_is =
            slice[0..2] == b"is"[..]
            || slice[0..2] == b"IS"[..]
            || slice[0..2] == b"iS"[..]
            || slice[0..2] == b"Is"[..];
        if starts_with_is {
            start = 2;
        }
    }
    let mut next_write = 0;
    for i in start..slice.len() {
        // SAFETY ARGUMENT: To guarantee that the resulting slice is valid
        // UTF-8, we ensure that the slice contains only ASCII bytes. In
        // particular, we drop every non-ASCII byte from the normalized string.
        let b = slice[i];
        if b == b' ' || b == b'_' || b == b'-' {
            continue;
        } else if b'A' <= b && b <= b'Z' {
            slice[next_write] = b + (b'a' - b'A');
            next_write += 1;
        } else if b <= 0x7F {
            slice[next_write] = b;
            next_write += 1;
        }
    }
    &mut slice[..next_write]
}

#[cfg(test)]
mod tests {
    use super::{
        character_name_normalize, character_name_normalize_bytes,
        symbolic_name_normalize, symbolic_name_normalize_bytes,
    };

    fn char_norm(s: &str) -> String {
        let mut s = s.to_string();
        character_name_normalize(&mut s);
        s
    }

    fn sym_norm(s: &str) -> String {
        let mut s = s.to_string();
        symbolic_name_normalize(&mut s);
        s
    }

    #[test]
    fn char_normalize() {
        assert_eq!(char_norm("HANGUL JUNGSEONG O-E"), "hanguljungseongo-e");
        assert_eq!(char_norm("zero-width space"), "zerowidthspace");
        assert_eq!(char_norm("zerowidthspace"), "zerowidthspace");
        assert_eq!(char_norm("ZERO WIDTH SPACE"), "zerowidthspace");
        assert_eq!(char_norm("TIBETAN MARK TSA -PHRU"), "tibetanmarktsa-phru");
    }

    #[test]
    fn sym_normalize() {
        assert_eq!(sym_norm("Line_Break"), "linebreak");
        assert_eq!(sym_norm("Line-break"), "linebreak");
        assert_eq!(sym_norm("linebreak"), "linebreak");
        assert_eq!(sym_norm("BA"), "ba");
        assert_eq!(sym_norm("ba"), "ba");
        assert_eq!(sym_norm("Greek"), "greek");
        assert_eq!(sym_norm("isGreek"), "greek");
        assert_eq!(sym_norm("IS_Greek"), "greek");
    }

    #[test]
    fn valid_utf8_character() {
        let mut x = b"abc\xFFxyz".to_vec();
        let y = character_name_normalize_bytes(&mut x);
        assert_eq!(y, b"abcxyz");
    }

    #[test]
    fn valid_utf8_symbolic() {
        let mut x = b"abc\xFFxyz".to_vec();
        let y = symbolic_name_normalize_bytes(&mut x);
        assert_eq!(y, b"abcxyz");
    }
}
