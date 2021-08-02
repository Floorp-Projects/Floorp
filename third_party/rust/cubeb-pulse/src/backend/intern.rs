// Copyright © 2017-2018 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use std::ffi::{CStr, CString};
use std::os::raw::c_char;

#[derive(Debug)]
pub struct Intern {
    vec: Vec<CString>,
}

impl Intern {
    pub fn new() -> Intern {
        Intern { vec: Vec::new() }
    }

    pub fn add(&mut self, string: &CStr) -> *const c_char {
        fn eq(s1: &CStr, s2: &CStr) -> bool {
            s1 == s2
        }
        for s in &self.vec {
            if eq(s, string) {
                return s.as_ptr();
            }
        }

        self.vec.push(string.to_owned());
        self.vec.last().unwrap().as_ptr()
    }
}

#[cfg(test)]
mod tests {
    use super::Intern;
    use std::ffi::CStr;

    #[test]
    fn intern() {
        fn cstr(str: &[u8]) -> &CStr {
            CStr::from_bytes_with_nul(str).unwrap()
        }

        let mut intern = Intern::new();

        let foo_ptr = intern.add(cstr(b"foo\0"));
        let bar_ptr = intern.add(cstr(b"bar\0"));
        assert!(!foo_ptr.is_null());
        assert!(!bar_ptr.is_null());
        assert!(foo_ptr != bar_ptr);

        assert!(foo_ptr == intern.add(cstr(b"foo\0")));
        assert!(foo_ptr != intern.add(cstr(b"fo\0")));
        assert!(foo_ptr != intern.add(cstr(b"fool\0")));
        assert!(foo_ptr != intern.add(cstr(b"not foo\0")));
    }
}
