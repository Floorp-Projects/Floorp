mod tables;

pub use tables::CLDR_VERSION;

use tinystr::{TinyStr4, TinyStr8};

unsafe fn lang_from_parts(
    input: (Option<u64>, Option<u32>, Option<u32>),
    lang: Option<TinyStr8>,
    script: Option<TinyStr4>,
    region: Option<TinyStr4>,
) -> Option<(Option<TinyStr8>, Option<TinyStr4>, Option<TinyStr4>)> {
    let lang = lang.or_else(|| input.0.map(|l| TinyStr8::new_unchecked(l)));
    let script = script.or_else(|| input.1.map(|s| TinyStr4::new_unchecked(s)));
    let region = region.or_else(|| input.2.map(|r| TinyStr4::new_unchecked(r)));
    Some((lang, script, region))
}

pub fn maximize(
    lang: Option<TinyStr8>,
    script: Option<TinyStr4>,
    region: Option<TinyStr4>,
) -> Option<(Option<TinyStr8>, Option<TinyStr4>, Option<TinyStr4>)> {
    if lang.is_some() && script.is_some() && region.is_some() {
        return None;
    }

    if let Some(l) = lang {
        if let Some(r) = region {
            let result = tables::LANG_REGION
                .binary_search_by_key(&(&l.into(), &r.into()), |(key_l, key_r, _)| (key_l, key_r))
                .ok();
            if let Some(r) = result {
                // safe because all table entries are well formed.
                return unsafe { lang_from_parts(tables::LANG_REGION[r].2, None, None, None) };
            }
        }

        if let Some(s) = script {
            let result = tables::LANG_SCRIPT
                .binary_search_by_key(&(&l.into(), &s.into()), |(key_l, key_s, _)| (key_l, key_s))
                .ok();
            if let Some(r) = result {
                // safe because all table entries are well formed.
                return unsafe { lang_from_parts(tables::LANG_SCRIPT[r].2, None, None, None) };
            }
        }

        let result = tables::LANG_ONLY
            .binary_search_by_key(&(&l.into()), |(key_l, _)| key_l)
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
    lang: Option<TinyStr8>,
    script: Option<TinyStr4>,
    region: Option<TinyStr4>,
) -> Option<(Option<TinyStr8>, Option<TinyStr4>, Option<TinyStr4>)> {
    // maximize returns None when all 3 components are
    // already filled so don't call it in that case.
    let max_langid = if lang.is_some() && script.is_some() && region.is_some() {
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
