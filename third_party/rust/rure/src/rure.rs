use std::collections::HashMap;
use std::ffi::{CStr, CString};
use std::ops::Deref;
use std::ptr;
use std::slice;
use std::str;

use libc::{c_char, size_t};
use regex::bytes;

use crate::error::{Error, ErrorKind};

const RURE_FLAG_CASEI: u32 = 1 << 0;
const RURE_FLAG_MULTI: u32 = 1 << 1;
const RURE_FLAG_DOTNL: u32 = 1 << 2;
const RURE_FLAG_SWAP_GREED: u32 = 1 << 3;
const RURE_FLAG_SPACE: u32 = 1 << 4;
const RURE_FLAG_UNICODE: u32 = 1 << 5;
const RURE_DEFAULT_FLAGS: u32 = RURE_FLAG_UNICODE;

pub struct Regex {
    re: bytes::Regex,
    capture_names: HashMap<String, i32>,
}

pub struct Options {
    size_limit: usize,
    dfa_size_limit: usize,
}

// The `RegexSet` is not exposed with option support or matching at an
// arbitrary position with a crate just yet. To circumvent this, we use
// the `Exec` structure directly.
pub struct RegexSet {
    re: bytes::RegexSet,
}

#[repr(C)]
pub struct rure_match {
    pub start: size_t,
    pub end: size_t,
}

pub struct Captures(bytes::Locations);

pub struct Iter {
    re: *const Regex,
    last_end: usize,
    last_match: Option<usize>,
}

pub struct IterCaptureNames {
    capture_names: bytes::CaptureNames<'static>,
    name_ptrs: Vec<*mut c_char>,
}

impl Deref for Regex {
    type Target = bytes::Regex;
    fn deref(&self) -> &bytes::Regex {
        &self.re
    }
}

impl Deref for RegexSet {
    type Target = bytes::RegexSet;
    fn deref(&self) -> &bytes::RegexSet {
        &self.re
    }
}

impl Default for Options {
    fn default() -> Options {
        Options { size_limit: 10 * (1 << 20), dfa_size_limit: 2 * (1 << 20) }
    }
}

ffi_fn! {
    fn rure_compile_must(pattern: *const c_char) -> *const Regex {
        let len = unsafe { CStr::from_ptr(pattern).to_bytes().len() };
        let pat = pattern as *const u8;
        let mut err = Error::new(ErrorKind::None);
        let re = rure_compile(
            pat, len, RURE_DEFAULT_FLAGS, ptr::null(), &mut err);
        if err.is_err() {
            let _ = writeln!(&mut io::stderr(), "{}", err);
            let _ = writeln!(
                &mut io::stderr(), "aborting from rure_compile_must");
            unsafe { abort() }
        }
        re
    }
}

ffi_fn! {
    fn rure_compile(
        pattern: *const u8,
        length: size_t,
        flags: u32,
        options: *const Options,
        error: *mut Error,
    ) -> *const Regex {
        let pat = unsafe { slice::from_raw_parts(pattern, length) };
        let pat = match str::from_utf8(pat) {
            Ok(pat) => pat,
            Err(err) => {
                unsafe {
                    if !error.is_null() {
                        *error = Error::new(ErrorKind::Str(err));
                    }
                    return ptr::null();
                }
            }
        };
        let mut builder = bytes::RegexBuilder::new(pat);
        if !options.is_null() {
            let options = unsafe { &*options };
            builder.size_limit(options.size_limit);
            builder.dfa_size_limit(options.dfa_size_limit);
        }
        builder.case_insensitive(flags & RURE_FLAG_CASEI > 0);
        builder.multi_line(flags & RURE_FLAG_MULTI > 0);
        builder.dot_matches_new_line(flags & RURE_FLAG_DOTNL > 0);
        builder.swap_greed(flags & RURE_FLAG_SWAP_GREED > 0);
        builder.ignore_whitespace(flags & RURE_FLAG_SPACE > 0);
        builder.unicode(flags & RURE_FLAG_UNICODE > 0);
        match builder.build() {
            Ok(re) => {
                let mut capture_names = HashMap::new();
                for (i, name) in re.capture_names().enumerate() {
                    if let Some(name) = name {
                        capture_names.insert(name.to_owned(), i as i32);
                    }
                }
                let re = Regex {
                    re: re,
                    capture_names: capture_names,
                };
                Box::into_raw(Box::new(re))
            }
            Err(err) => {
                unsafe {
                    if !error.is_null() {
                        *error = Error::new(ErrorKind::Regex(err));
                    }
                    ptr::null()
                }
            }
        }
    }
}

ffi_fn! {
    fn rure_free(re: *const Regex) {
        unsafe { drop(Box::from_raw(re as *mut Regex)); }
    }
}

ffi_fn! {
    fn rure_is_match(
        re: *const Regex,
        haystack: *const u8,
        len: size_t,
        start: size_t,
    ) -> bool {
        let re = unsafe { &*re };
        let haystack = unsafe { slice::from_raw_parts(haystack, len) };
        re.is_match_at(haystack, start)
    }
}

ffi_fn! {
    fn rure_find(
        re: *const Regex,
        haystack: *const u8,
        len: size_t,
        start: size_t,
        match_info: *mut rure_match,
    ) -> bool {
        let re = unsafe { &*re };
        let haystack = unsafe { slice::from_raw_parts(haystack, len) };
        re.find_at(haystack, start).map(|m| unsafe {
            if !match_info.is_null() {
                (*match_info).start = m.start();
                (*match_info).end = m.end();
            }
        }).is_some()
    }
}

ffi_fn! {
    fn rure_find_captures(
        re: *const Regex,
        haystack: *const u8,
        len: size_t,
        start: size_t,
        captures: *mut Captures,
    ) -> bool {
        let re = unsafe { &*re };
        let haystack = unsafe { slice::from_raw_parts(haystack, len) };
        let slots = unsafe { &mut (*captures).0 };
        re.read_captures_at(slots, haystack, start).is_some()
    }
}

ffi_fn! {
    fn rure_shortest_match(
        re: *const Regex,
        haystack: *const u8,
        len: size_t,
        start: size_t,
        end: *mut usize,
    ) -> bool {
        let re = unsafe { &*re };
        let haystack = unsafe { slice::from_raw_parts(haystack, len) };
        match re.shortest_match_at(haystack, start) {
            None => false,
            Some(i) => {
                if !end.is_null() {
                    unsafe {
                        *end = i;
                    }
                }
                true
            }
        }
    }
}

ffi_fn! {
    fn rure_capture_name_index(
        re: *const Regex,
        name: *const c_char,
    ) -> i32 {
        let re = unsafe { &*re };
        let name = unsafe { CStr::from_ptr(name) };
        let name = match name.to_str() {
            Err(_) => return -1,
            Ok(name) => name,
        };
        re.capture_names.get(name).map(|&i|i).unwrap_or(-1)
    }
}

ffi_fn! {
    fn rure_iter_capture_names_new(
        re: *const Regex,
    ) -> *mut IterCaptureNames {
        let re = unsafe { &*re };
        Box::into_raw(Box::new(IterCaptureNames {
            capture_names: re.re.capture_names(),
            name_ptrs: Vec::new(),
        }))
    }
}

ffi_fn! {
    fn rure_iter_capture_names_free(it: *mut IterCaptureNames) {
        unsafe {
            let it = &mut *it;
            while let Some(ptr) = it.name_ptrs.pop() {
                drop(CString::from_raw(ptr));
            }
            drop(Box::from_raw(it));
        }
    }
}

ffi_fn! {
    fn rure_iter_capture_names_next(
        it: *mut IterCaptureNames,
        capture_name: *mut *mut c_char,
    ) -> bool {
        if capture_name.is_null() {
            return false;
        }

        let it = unsafe { &mut *it };
        let cn = match it.capture_names.next() {
            // Top-level iterator ran out of capture groups
            None => return false,
            Some(val) => {
                let name = match val {
                    // inner Option didn't have a name
                    None => "",
                    Some(name) => name
                };
                name
            }
        };

        unsafe {
            let cs = match CString::new(cn.as_bytes()) {
                Result::Ok(val) => val,
                Result::Err(_) => return false
            };
            let ptr = cs.into_raw();
            it.name_ptrs.push(ptr);
            *capture_name = ptr;
        }
        true

    }
}

ffi_fn! {
    fn rure_iter_new(
        re: *const Regex,
    ) -> *mut Iter {
        Box::into_raw(Box::new(Iter {
            re: re,
            last_end: 0,
            last_match: None,
        }))
    }
}

ffi_fn! {
    fn rure_iter_free(it: *mut Iter) {
        unsafe { drop(Box::from_raw(it)); }
    }
}

ffi_fn! {
    fn rure_iter_next(
        it: *mut Iter,
        haystack: *const u8,
        len: size_t,
        match_info: *mut rure_match,
    ) -> bool {
        let it = unsafe { &mut *it };
        let re = unsafe { &*it.re };
        let text = unsafe { slice::from_raw_parts(haystack, len) };
        if it.last_end > text.len() {
            return false;
        }
        let (s, e) = match re.find_at(text, it.last_end) {
            None => return false,
            Some(m) => (m.start(), m.end()),
        };
        if s == e {
            // This is an empty match. To ensure we make progress, start
            // the next search at the smallest possible starting position
            // of the next match following this one.
            it.last_end += 1;
            // Don't accept empty matches immediately following a match.
            // Just move on to the next match.
            if Some(e) == it.last_match {
                return rure_iter_next(it, haystack, len, match_info);
            }
        } else {
            it.last_end = e;
        }
        it.last_match = Some(e);
        if !match_info.is_null() {
            unsafe {
                (*match_info).start = s;
                (*match_info).end = e;
            }
        }
        true
    }
}

ffi_fn! {
    fn rure_iter_next_captures(
        it: *mut Iter,
        haystack: *const u8,
        len: size_t,
        captures: *mut Captures,
    ) -> bool {
        let it = unsafe { &mut *it };
        let re = unsafe { &*it.re };
        let slots = unsafe { &mut (*captures).0 };
        let text = unsafe { slice::from_raw_parts(haystack, len) };
        if it.last_end > text.len() {
            return false;
        }
        let (s, e) = match re.read_captures_at(slots, text, it.last_end) {
            None => return false,
            Some(m) => (m.start(), m.end()),
        };
        if s == e {
            // This is an empty match. To ensure we make progress, start
            // the next search at the smallest possible starting position
            // of the next match following this one.
            it.last_end += 1;
            // Don't accept empty matches immediately following a match.
            // Just move on to the next match.
            if Some(e) == it.last_match {
                return rure_iter_next_captures(it, haystack, len, captures);
            }
        } else {
            it.last_end = e;
        }
        it.last_match = Some(e);
        true
    }
}

ffi_fn! {
    fn rure_captures_new(re: *const Regex) -> *mut Captures {
        let re = unsafe { &*re };
        let captures = Captures(re.locations());
        Box::into_raw(Box::new(captures))
    }
}

ffi_fn! {
    fn rure_captures_free(captures: *const Captures) {
        unsafe { drop(Box::from_raw(captures as *mut Captures)); }
    }
}

ffi_fn! {
    fn rure_captures_at(
        captures: *const Captures,
        i: size_t,
        match_info: *mut rure_match,
    ) -> bool {
        let locs = unsafe { &(*captures).0 };
        match locs.pos(i) {
            Some((start, end)) => {
                if !match_info.is_null() {
                    unsafe {
                        (*match_info).start = start;
                        (*match_info).end = end;
                    }
                }
                true
            }
            _ => false
        }
    }
}

ffi_fn! {
    fn rure_captures_len(captures: *const Captures) -> size_t {
        unsafe { (*captures).0.len() }
    }
}

ffi_fn! {
    fn rure_options_new() -> *mut Options {
        Box::into_raw(Box::new(Options::default()))
    }
}

ffi_fn! {
    fn rure_options_free(options: *mut Options) {
        unsafe { drop(Box::from_raw(options)); }
    }
}

ffi_fn! {
    fn rure_options_size_limit(options: *mut Options, limit: size_t) {
        let options = unsafe { &mut *options };
        options.size_limit = limit;
    }
}

ffi_fn! {
    fn rure_options_dfa_size_limit(options: *mut Options, limit: size_t) {
        let options = unsafe { &mut *options };
        options.dfa_size_limit = limit;
    }
}

ffi_fn! {
    fn rure_compile_set(
        patterns: *const *const u8,
        patterns_lengths: *const size_t,
        patterns_count: size_t,
        flags: u32,
        options: *const Options,
        error: *mut Error
    ) -> *const RegexSet {
        let (raw_pats, raw_patsl) = unsafe {
            (
                slice::from_raw_parts(patterns, patterns_count),
                slice::from_raw_parts(patterns_lengths, patterns_count)
            )
        };

        let mut pats = Vec::with_capacity(patterns_count);
        for (&raw_pat, &raw_patl) in raw_pats.iter().zip(raw_patsl) {
            let pat = unsafe { slice::from_raw_parts(raw_pat, raw_patl) };
            pats.push(match str::from_utf8(pat) {
                Ok(pat) => pat,
                Err(err) => {
                    unsafe {
                        if !error.is_null() {
                            *error = Error::new(ErrorKind::Str(err));
                        }
                        return ptr::null();
                    }
                }
            });
        }

        let mut builder = bytes::RegexSetBuilder::new(pats);
        if !options.is_null() {
            let options = unsafe { &*options };
            builder.size_limit(options.size_limit);
            builder.dfa_size_limit(options.dfa_size_limit);
        }
        builder.case_insensitive(flags & RURE_FLAG_CASEI > 0);
        builder.multi_line(flags & RURE_FLAG_MULTI > 0);
        builder.dot_matches_new_line(flags & RURE_FLAG_DOTNL > 0);
        builder.swap_greed(flags & RURE_FLAG_SWAP_GREED > 0);
        builder.ignore_whitespace(flags & RURE_FLAG_SPACE > 0);
        builder.unicode(flags & RURE_FLAG_UNICODE > 0);
        match builder.build() {
            Ok(re) => {
                Box::into_raw(Box::new(RegexSet { re: re }))
            }
            Err(err) => {
                unsafe {
                    if !error.is_null() {
                        *error = Error::new(ErrorKind::Regex(err))
                    }
                    ptr::null()
                }
            }
        }
    }
}

ffi_fn! {
    fn rure_set_free(re: *const RegexSet) {
        unsafe { drop(Box::from_raw(re as *mut RegexSet)); }
    }
}

ffi_fn! {
    fn rure_set_is_match(
        re: *const RegexSet,
        haystack: *const u8,
        len: size_t,
        start: size_t
    ) -> bool {
        let re = unsafe { &*re };
        let haystack = unsafe { slice::from_raw_parts(haystack, len) };
        re.is_match_at(haystack, start)
    }
}

ffi_fn! {
    fn rure_set_matches(
        re: *const RegexSet,
        haystack: *const u8,
        len: size_t,
        start: size_t,
        matches: *mut bool
    ) -> bool {
        let re = unsafe { &*re };
        let mut matches = unsafe {
            slice::from_raw_parts_mut(matches, re.len())
        };
        let haystack = unsafe { slice::from_raw_parts(haystack, len) };

        // read_matches_at isn't guaranteed to set non-matches to false
        for item in matches.iter_mut() {
            *item = false;
        }
        re.read_matches_at(&mut matches, haystack, start)
    }
}

ffi_fn! {
    fn rure_set_len(re: *const RegexSet) -> size_t {
        unsafe { (*re).len() }
    }
}

ffi_fn! {
    fn rure_escape_must(pattern: *const c_char) -> *const c_char {
        let len = unsafe { CStr::from_ptr(pattern).to_bytes().len() };
        let pat = pattern as *const u8;
        let mut err = Error::new(ErrorKind::None);
        let esc = rure_escape(pat, len, &mut err);
        if err.is_err() {
            let _ = writeln!(&mut io::stderr(), "{}", err);
            let _ = writeln!(
                &mut io::stderr(), "aborting from rure_escape_must");
            unsafe { abort() }
        }
        esc
    }
}

/// A helper function that implements fallible escaping in a way that returns
/// an error if escaping failed.
///
/// This should ideally be exposed, but it needs API design work. In
/// particular, this should not return a C string, but a `const uint8_t *`
/// instead, since it may contain a NUL byte.
fn rure_escape(
    pattern: *const u8,
    length: size_t,
    error: *mut Error,
) -> *const c_char {
    let pat: &[u8] = unsafe { slice::from_raw_parts(pattern, length) };
    let str_pat = match str::from_utf8(pat) {
        Ok(val) => val,
        Err(err) => unsafe {
            if !error.is_null() {
                *error = Error::new(ErrorKind::Str(err));
            }
            return ptr::null();
        },
    };
    let esc_pat = regex::escape(str_pat);
    let c_esc_pat = match CString::new(esc_pat) {
        Ok(val) => val,
        Err(err) => unsafe {
            if !error.is_null() {
                *error = Error::new(ErrorKind::Nul(err));
            }
            return ptr::null();
        },
    };
    c_esc_pat.into_raw() as *const c_char
}

ffi_fn! {
    fn rure_cstring_free(s: *mut c_char) {
        unsafe { drop(CString::from_raw(s)); }
    }
}
