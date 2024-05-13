mod tables;

pub use tables::CLDR_VERSION;

use crate::subtags;

unsafe fn lang_from_parts(
    input: (Option<u64>, Option<u32>, Option<u32>),
    lang: Option<subtags::Language>,
    script: Option<subtags::Script>,
    region: Option<subtags::Region>,
) -> Option<(
    subtags::Language,
    Option<subtags::Script>,
    Option<subtags::Region>,
)> {
    let lang = lang
        .or_else(|| input.0.map(|s| subtags::Language::from_raw_unchecked(s)))
        .unwrap();
    let script = script.or_else(|| input.1.map(|s| subtags::Script::from_raw_unchecked(s)));
    let region = region.or_else(|| input.2.map(|r| subtags::Region::from_raw_unchecked(r)));
    Some((lang, script, region))
}

pub fn maximize(
    lang: subtags::Language,
    script: Option<subtags::Script>,
    region: Option<subtags::Region>,
) -> Option<(
    subtags::Language,
    Option<subtags::Script>,
    Option<subtags::Region>,
)> {
    if !lang.is_empty() && script.is_some() && region.is_some() {
        return None;
    }

    if let Some(l) = Into::<Option<u64>>::into(lang) {
        if let Some(r) = region {
            let result = tables::LANG_REGION
                .binary_search_by_key(&(&l, &r.into()), |(key_l, key_r, _)| (key_l, key_r))
                .ok();
            if let Some(r) = result {
                // safe because all table entries are well formed.
                return unsafe { lang_from_parts(tables::LANG_REGION[r].2, None, None, None) };
            }
        }

        if let Some(s) = script {
            let result = tables::LANG_SCRIPT
                .binary_search_by_key(&(&l, &s.into()), |(key_l, key_s, _)| (key_l, key_s))
                .ok();
            if let Some(r) = result {
                // safe because all table entries are well formed.
                return unsafe { lang_from_parts(tables::LANG_SCRIPT[r].2, None, None, None) };
            }
        }

        let result = tables::LANG_ONLY
            .binary_search_by_key(&(&l), |(key_l, _)| key_l)
            .ok();
        if let Some(r) = result {
            // safe because all table entries are well formed.
            return unsafe { lang_from_parts(tables::LANG_ONLY[r].1, None, script, region) };
        }
    } else if let Some(s) = script {
        if let Some(r) = region {
            let result = tables::SCRIPT_REGION
                .binary_search_by_key(&(&s.into(), &r.into()), |(key_s, key_r, _)| (key_s, key_r))
                .ok();
            if let Some(r) = result {
                // safe because all table entries are well formed.
                return unsafe { lang_from_parts(tables::SCRIPT_REGION[r].2, None, None, None) };
            }
        }

        let result = tables::SCRIPT_ONLY
            .binary_search_by_key(&(&s.into()), |(key_s, _)| key_s)
            .ok();
        if let Some(r) = result {
            // safe because all table entries are well formed.
            return unsafe { lang_from_parts(tables::SCRIPT_ONLY[r].1, None, None, region) };
        }
    } else if let Some(r) = region {
        let result = tables::REGION_ONLY
            .binary_search_by_key(&(&r.into()), |(key_r, _)| key_r)
            .ok();
        if let Some(r) = result {
            // safe because all table entries are well formed.
            return unsafe { lang_from_parts(tables::REGION_ONLY[r].1, None, None, None) };
        }
    }

    None
}

pub fn minimize(
    lang: subtags::Language,
    script: Option<subtags::Script>,
    region: Option<subtags::Region>,
) -> Option<(
    subtags::Language,
    Option<subtags::Script>,
    Option<subtags::Region>,
)> {
    // maximize returns None when all 3 components are
    // already filled so don't call it in that case.
    let max_langid = if !lang.is_empty() && script.is_some() && region.is_some() {
        (lang, script, region)
    } else {
        maximize(lang, script, region)?
    };

    if let Some(trial) = maximize(max_langid.0, None, None) {
        if trial == max_langid {
            return Some((max_langid.0, None, None));
        }
    }

    if max_langid.2.is_some() {
        if let Some(trial) = maximize(max_langid.0, None, max_langid.2) {
            if trial == max_langid {
                return Some((max_langid.0, None, max_langid.2));
            }
        }
    }

    if max_langid.1.is_some() {
        if let Some(trial) = maximize(max_langid.0, max_langid.1, None) {
            if trial == max_langid {
                return Some((max_langid.0, max_langid.1, None));
            }
        }
    }
    None
}
