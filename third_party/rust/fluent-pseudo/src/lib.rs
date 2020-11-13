use regex::Captures;
use regex::Regex;
use std::borrow::Cow;

static TRANSFORM_SMALL_MAP: &[char] = &[
    'ȧ', 'ƀ', 'ƈ', 'ḓ', 'ḗ', 'ƒ', 'ɠ', 'ħ', 'ī', 'ĵ', 'ķ', 'ŀ', 'ḿ', 'ƞ', 'ǿ', 'ƥ', 'ɋ', 'ř', 'ş',
    'ŧ', 'ŭ', 'ṽ', 'ẇ', 'ẋ', 'ẏ', 'ẑ',
];
static TRANSFORM_CAPS_MAP: &[char] = &[
    'Ȧ', 'Ɓ', 'Ƈ', 'Ḓ', 'Ḗ', 'Ƒ', 'Ɠ', 'Ħ', 'Ī', 'Ĵ', 'Ķ', 'Ŀ', 'Ḿ', 'Ƞ', 'Ǿ', 'Ƥ', 'Ɋ', 'Ř', 'Ş',
    'Ŧ', 'Ŭ', 'Ṽ', 'Ẇ', 'Ẋ', 'Ẏ', 'Ẑ',
];

static FLIPPED_SMALL_MAP: &[char] = &[
    'ɐ', 'q', 'ɔ', 'p', 'ǝ', 'ɟ', 'ƃ', 'ɥ', 'ı', 'ɾ', 'ʞ', 'ʅ', 'ɯ', 'u', 'o', 'd', 'b', 'ɹ', 's',
    'ʇ', 'n', 'ʌ', 'ʍ', 'x', 'ʎ', 'z',
];
static FLIPPED_CAPS_MAP: &[char] = &[
    '∀', 'Ԑ', 'Ↄ', 'ᗡ', 'Ǝ', 'Ⅎ', '⅁', 'H', 'I', 'ſ', 'Ӽ', '⅂', 'W', 'N', 'O', 'Ԁ', 'Ò', 'ᴚ', 'S',
    '⊥', '∩', 'Ʌ', 'M', 'X', '⅄', 'Z',
];

pub fn transform_dom(s: &str, flipped: bool, elongate: bool) -> Cow<str> {
    // Exclude access-keys and other single-char messages
    if s.len() == 1 {
        return s.into();
    }

    // XML entities (&#x202a;) and XML tags.
    let re_excluded = Regex::new(r"&[#\w]+;|<\s*.+?\s*>").unwrap();

    let mut result = Cow::from(s);

    let mut pos = 0;
    let mut diff = 0;

    for cap in re_excluded.captures_iter(s) {
        let capture = cap.get(0).unwrap();

        let sub_len = capture.start() - pos;
        let range = pos..capture.start();
        let result_range = pos + diff..capture.start() + diff;
        let sub = &s[range.clone()];
        let transform_sub = transform(&sub, false, true);
        diff += transform_sub.len() - sub_len;
        result
            .to_mut()
            .replace_range(result_range.clone(), &transform_sub);
        pos = capture.end();
    }
    let range = pos..s.len();
    let result_range = pos + diff..result.len();
    let transform_sub = transform(&s[range], flipped, elongate);
    result
        .to_mut()
        .replace_range(result_range, &transform_sub);
    result
}

pub fn transform(s: &str, flipped: bool, elongate: bool) -> Cow<str> {
    // XXX: avoid recreating it on each call.
    let re_az = Regex::new(r"[a-zA-Z]").unwrap();

    let (small_map, caps_map) = if flipped {
        (FLIPPED_SMALL_MAP, FLIPPED_CAPS_MAP)
    } else {
        (TRANSFORM_SMALL_MAP, TRANSFORM_CAPS_MAP)
    };

    re_az.replace_all(s, |caps: &Captures| {
        let ch = caps[0].chars().nth(0).unwrap();
        let cc = ch as u8;
        if cc >= 97 && cc <= 122 {
            let pos = cc - 97;
            let new_char = small_map[pos as usize];
            if elongate && (cc == 97 || cc == 101 || cc == 111 || cc == 117) {
                let mut s = new_char.to_string();
                s.push(new_char);
                s
            } else {
                new_char.to_string()
            }
        } else if cc >= 65 && cc <= 90 {
            let pos = cc - 65;
            let new_char = caps_map[pos as usize];
            new_char.to_string()
        } else {
            ch.to_string()
        }
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn it_works() {
        let x = transform("Hello World", false, true);
        assert_eq!(x, "Ħḗḗŀŀǿǿ Ẇǿǿřŀḓ");

        let x = transform("Hello World", false, false);
        assert_eq!(x, "Ħḗŀŀǿ Ẇǿřŀḓ");

        let x = transform("Hello World", true, false);
        assert_eq!(x, "Hǝʅʅo Moɹʅp");

        let x = transform("f", false, true);
        assert_eq!(x, "ƒ");
    }

    #[test]
    fn dom_test() {
        let x = transform_dom("Hello <a>World</a>", false, true);
        assert_eq!(x, "Ħḗḗŀŀǿǿ <a>Ẇǿǿřŀḓ</a>");

        let x = transform_dom("Hello <a>World</a> in <b>my</b> House.", false, true);
        assert_eq!(x, "Ħḗḗŀŀǿǿ <a>Ẇǿǿřŀḓ</a> īƞ <b>ḿẏ</b> Ħǿǿŭŭşḗḗ.");

        // Don't touch single character values.
        let x = transform_dom("f", false, true);
        assert_eq!(x, "f");
    }
}
