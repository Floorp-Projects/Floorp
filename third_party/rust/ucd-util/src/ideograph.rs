/// A set of ranges that corresponds to the set of all ideograph codepoints.
///
/// These ranges are defined in Unicode 4.8 Table 4-13.
pub const RANGE_IDEOGRAPH: &'static [(u32, u32)] = &[
    (0x3400, 0x4DB5),
    (0x4E00, 0x9FD5),
    (0x4E00, 0x9FD5),
    (0x20000, 0x2A6D6),
    (0x2A700, 0x2B734),
    (0x2B740, 0x2B81D),
    (0x2B820, 0x2CEA1),
    (0x17000, 0x187EC),
    (0xF900, 0xFA6D),
    (0xFA70, 0xFAD9),
    (0x2F800, 0x2FA1D),
];

/// Return the character name of the given ideograph codepoint.
///
/// This operation is only defined on ideographic codepoints. This includes
/// precisely the following inclusive ranges:
///
/// * `3400..4DB5`
/// * `4E00..9FD5`
/// * `20000..2A6D6`
/// * `2A700..2B734`
/// * `2B740..2B81D`
/// * `2B820..2CEA1`
/// * `17000..187EC`
/// * `F900..FA6D`
/// * `FA70..FAD9`
/// * `2F800..2FA1D`
///
/// If the given codepoint is not in any of the above ranges, then `None` is
/// returned.
///
/// This implements the algorithm described in Unicode 4.8.
pub fn ideograph_name(cp: u32) -> Option<String> {
    // This match should be in sync with the `RANGE_IDEOGRAPH` constant.
    match cp {
        0x3400...0x4DB5
        | 0x4E00...0x9FD5
        | 0x20000...0x2A6D6
        | 0x2A700...0x2B734
        | 0x2B740...0x2B81D
        | 0x2B820...0x2CEA1 => {
            Some(format!("CJK UNIFIED IDEOGRAPH-{:04X}", cp))
        }
        0x17000...0x187EC => {
            Some(format!("TANGUT IDEOGRAPH-{:04X}", cp))
        }
        0xF900...0xFA6D | 0xFA70...0xFAD9 | 0x2F800...0x2FA1D => {
            Some(format!("CJK COMPATIBILITY IDEOGRAPH-{:04X}", cp))
        }
        _ => None,
    }
}

#[cfg(test)]
mod tests {
    use super::ideograph_name;

    #[test]
    fn name() {
        assert_eq!(
            ideograph_name(0x4E00).unwrap(),
            "CJK UNIFIED IDEOGRAPH-4E00");
        assert_eq!(
            ideograph_name(0x9FD5).unwrap(),
            "CJK UNIFIED IDEOGRAPH-9FD5");
        assert_eq!(
            ideograph_name(0x17000).unwrap(),
            "TANGUT IDEOGRAPH-17000");
        assert_eq!(
            ideograph_name(0xF900).unwrap(),
            "CJK COMPATIBILITY IDEOGRAPH-F900");
    }

    #[test]
    fn invalid() {
        assert!(ideograph_name(0).is_none());
    }
}
