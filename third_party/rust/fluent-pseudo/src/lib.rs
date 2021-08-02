use regex::Captures;
use regex::Regex;
use std::borrow::Cow;

static TRANSFORM_SMALL_MAP: &[char] = &[
    'a', 'ƀ', 'ƈ', 'ḓ', 'e', 'ƒ', 'ɠ', 'ħ', 'i', 'ĵ', 'ķ', 'ŀ', 'ḿ', 'ƞ', 'o', 'ƥ', 'ɋ', 'ř', 'ş',
    'ŧ', 'u', 'ṽ', 'ẇ', 'ẋ', 'ẏ', 'ẑ',
];
static TRANSFORM_CAPS_MAP: &[char] = &[
    'A', 'Ɓ', 'Ƈ', 'Ḓ', 'E', 'Ƒ', 'Ɠ', 'Ħ', 'I', 'Ĵ', 'Ķ', 'Ŀ', 'Ḿ', 'Ƞ', 'O', 'Ƥ', 'Ɋ', 'Ř', 'Ş',
    'Ŧ', 'U', 'Ṽ', 'Ẇ', 'Ẋ', 'Ẏ', 'Ẑ',
];

static FLIPPED_SMALL_MAP: &[char] = &[
    'ɐ', 'q', 'ɔ', 'p', 'ǝ', 'ɟ', 'ƃ', 'ɥ', 'ı', 'ɾ', 'ʞ', 'ʅ', 'ɯ', 'u', 'o', 'd', 'b', 'ɹ', 's',
    'ʇ', 'n', 'ʌ', 'ʍ', 'x', 'ʎ', 'z',
];
static FLIPPED_CAPS_MAP: &[char] = &[
    '∀', 'Ԑ', 'Ↄ', 'ᗡ', 'Ǝ', 'Ⅎ', '⅁', 'H', 'I', 'ſ', 'Ӽ', '⅂', 'W', 'N', 'O', 'Ԁ', 'Ò', 'ᴚ', 'S',
    '⊥', '∩', 'Ʌ', 'M', 'X', '⅄', 'Z',
];

static mut RE_EXCLUDED: Option<Regex> = None;
static mut RE_AZ: Option<Regex> = None;

pub fn transform_dom(s: &str, flipped: bool, elongate: bool, with_markers: bool) -> Cow<str> {
    // Exclude access-keys and other single-char messages
    if s.len() == 1 {
        return s.into();
    }

    // XML entities (&#x202a;) and XML tags.
    let re_excluded =
        unsafe { RE_EXCLUDED.get_or_insert_with(|| Regex::new(r"&[#\w]+;|<\s*.+?\s*>").unwrap()) };

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
    result.to_mut().replace_range(result_range, &transform_sub);

    if with_markers {
        return Cow::from("[") + result + "]"
    }

    result
}

pub fn transform(s: &str, flipped: bool, elongate: bool) -> Cow<str> {
    let re_az = unsafe { RE_AZ.get_or_insert_with(|| Regex::new(r"[a-zA-Z]").unwrap()) };

    let (small_map, caps_map) = if flipped {
        (FLIPPED_SMALL_MAP, FLIPPED_CAPS_MAP)
    } else {
        (TRANSFORM_SMALL_MAP, TRANSFORM_CAPS_MAP)
    };

    re_az.replace_all(s, |caps: &Captures| {
        let ch = caps[0].chars().next().unwrap();
        let cc = ch as u8;
        if (97..=122).contains(&cc) {
            let pos = cc - 97;
            let new_char = small_map[pos as usize];
            // duplicate "a", "e", "o" and "u" to emulate ~30% longer text
            if elongate && (cc == 97 || cc == 101 || cc == 111 || cc == 117) {
                let mut s = new_char.to_string();
                s.push(new_char);
                s
            } else {
                new_char.to_string()
            }
        } else if (65..=90).contains(&cc) {
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
        assert_eq!(x, "Ħeeŀŀoo Ẇoořŀḓ");

        let x = transform("Hello World", false, false);
        assert_eq!(x, "Ħeŀŀo Ẇořŀḓ");

        let x = transform("Hello World", true, false);
        assert_eq!(x, "Hǝʅʅo Moɹʅp");

        let x = transform("f", false, true);
        assert_eq!(x, "ƒ");
    }

    #[test]
    fn dom_test() {
        let x = transform_dom("Hello <a>World</a>", false, true, false);
        assert_eq!(x, "Ħeeŀŀoo <a>Ẇoořŀḓ</a>");

        let x = transform_dom("Hello <a>World</a> in <b>my</b> House.", false, true, false);
        assert_eq!(x, "Ħeeŀŀoo <a>Ẇoořŀḓ</a> iƞ <b>ḿẏ</b> Ħoouuşee.");

        // Use markers.
        let x = transform_dom("Hello World within markers", false, false, true);
        assert_eq!(x, "[Ħeŀŀo Ẇořŀḓ ẇiŧħiƞ ḿařķeřş]");

        // Don't touch single character values.
        let x = transform_dom("f", false, true, false);
        assert_eq!(x, "f");
    }
}
