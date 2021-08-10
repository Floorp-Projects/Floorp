// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at https://mozilla.org/MPL/2.0/.

use std::collections::HashMap;
use std::os::raw::c_char;

use ffi_support::FfiStr;

use crate::ffi_string_ext::FallibleToString;

pub type RawStringArray = *const *const c_char;
pub type RawIntArray = *const i32;
pub type RawInt64Array = *const i64;

/// Creates a vector of strings from a raw C-like string array.
///
/// Returns an error if any of the strings contain invalid UTF-8 characters.
///
/// # Safety
///
/// * We check the array pointer for validity (non-null).
/// * FfiStr checks each individual char pointer for validity (non-null).
/// * We discard invalid char pointers (null pointer).
/// * Invalid UTF-8 in any string will return an error from this function.
pub fn from_raw_string_array(arr: RawStringArray, len: i32) -> glean_core::Result<Vec<String>> {
    unsafe {
        if arr.is_null() || len <= 0 {
            return Ok(vec![]);
        }

        let arr_ptrs = std::slice::from_raw_parts(arr, len as usize);
        arr_ptrs
            .iter()
            .map(|&p| {
                // Drop invalid strings
                FfiStr::from_raw(p).to_string_fallible()
            })
            .collect()
    }
}

/// Creates a HashMap<i32, String> from a pair of C int and string arrays.
///
/// Returns an error if any of the strings contain invalid UTF-8 characters.
///
/// # Safety
///
/// * We check the array pointer for validity (non-null).
/// * FfiStr checks each individual char pointer for validity (non-null).
/// * We discard invalid char pointers (null pointer).
/// * Invalid UTF-8 in any string will return an error from this function.
pub fn from_raw_int_array_and_string_array(
    keys: RawIntArray,
    values: RawStringArray,
    len: i32,
) -> glean_core::Result<Option<HashMap<i32, String>>> {
    unsafe {
        if keys.is_null() || values.is_null() || len <= 0 {
            return Ok(None);
        }

        let keys_ptrs = std::slice::from_raw_parts(keys, len as usize);
        let values_ptrs = std::slice::from_raw_parts(values, len as usize);

        let res: glean_core::Result<_> = keys_ptrs
            .iter()
            .zip(values_ptrs.iter())
            .map(|(&k, &v)| FfiStr::from_raw(v).to_string_fallible().map(|s| (k, s)))
            .collect();
        res.map(Some)
    }
}

/// Creates a HashMap<String, String> from a pair of C string arrays.
///
/// Returns an error if any of the strings contain invalid UTF-8 characters.
///
/// # Safety
///
/// * We check the array pointer for validity (non-null).
/// * FfiStr checks each individual char pointer for validity (non-null).
/// * We discard invalid char pointers (null pointer).
/// * Invalid UTF-8 in any string will return an error from this function.
pub fn from_raw_string_array_and_string_array(
    keys: RawStringArray,
    values: RawStringArray,
    len: i32,
) -> glean_core::Result<Option<HashMap<String, String>>> {
    unsafe {
        if keys.is_null() || values.is_null() || len <= 0 {
            return Ok(None);
        }

        let keys_ptrs = std::slice::from_raw_parts(keys, len as usize);
        let values_ptrs = std::slice::from_raw_parts(values, len as usize);

        let res: glean_core::Result<_> = keys_ptrs
            .iter()
            .zip(values_ptrs.iter())
            .map(|(&k, &v)| {
                let k = FfiStr::from_raw(k).to_string_fallible()?;

                let v = FfiStr::from_raw(v).to_string_fallible()?;

                Ok((k, v))
            })
            .collect();
        res.map(Some)
    }
}

/// Creates a Vec<u32> from a raw C uint64 array.
///
/// This will return an empty `Vec` if the input is empty.
///
/// # Safety
///
/// * We check the array pointer for validity (non-null).
pub fn from_raw_int64_array(values: RawInt64Array, len: i32) -> Vec<i64> {
    unsafe {
        if values.is_null() || len <= 0 {
            return vec![];
        }

        let value_slice = std::slice::from_raw_parts(values, len as usize);
        value_slice.to_vec()
    }
}

#[cfg(test)]
mod test {
    use super::*;
    use std::ffi::CString;

    mod raw_string_array {
        use super::*;

        #[test]
        fn parsing_valid_array() {
            let expected = vec!["first", "second"];
            let array: Vec<CString> = expected
                .iter()
                .map(|&s| CString::new(&*s).unwrap())
                .collect();
            let ptr_array: Vec<*const _> = array.iter().map(|s| s.as_ptr()).collect();

            let list = from_raw_string_array(ptr_array.as_ptr(), expected.len() as i32).unwrap();
            assert_eq!(expected, list);
        }

        #[test]
        fn parsing_empty_array() {
            let expected: Vec<String> = vec![];

            // Testing a null pointer (length longer to ensure the null pointer is checked)
            let list = from_raw_string_array(std::ptr::null(), 2).unwrap();
            assert_eq!(expected, list);

            // Need a (filled) vector to obtain a valid pointer.
            let array = vec![CString::new("glean").unwrap()];
            let ptr_array: Vec<*const _> = array.iter().map(|s| s.as_ptr()).collect();

            // Check the length with a valid pointer.
            let list = from_raw_string_array(ptr_array.as_ptr(), 0).unwrap();
            assert_eq!(expected, list);
        }

        #[test]
        fn parsing_invalid_utf8_fails() {
            // CAREFUL! We're manually constructing nul-terminated

            // Need a (filled) vector to obtain a valid pointer.
            let array = vec![
                // -1 is definitely an invalid UTF-8 codepoint
                // Let's not break anything and append the nul terminator
                vec![0x67, 0x6c, -1, 0x65, 0x61, 0x6e, 0x00],
            ];
            let ptr_array: Vec<*const _> = array.iter().map(|s| s.as_ptr()).collect();

            let list = from_raw_string_array(ptr_array.as_ptr(), array.len() as i32);
            assert!(list.is_err());
        }
    }

    mod raw_int_string_array {
        use super::*;

        #[test]
        fn parsing_valid_array() {
            let mut expected_map = HashMap::new();
            expected_map.insert(7, "seven".to_string());
            expected_map.insert(8, "eight".to_string());

            let int_array = vec![7, 8];
            let str_array = vec![
                CString::new("seven").unwrap(),
                CString::new("eight").unwrap(),
            ];
            let ptr_array: Vec<*const _> = str_array.iter().map(|s| s.as_ptr()).collect();

            let map = from_raw_int_array_and_string_array(
                int_array.as_ptr(),
                ptr_array.as_ptr(),
                expected_map.len() as i32,
            )
            .unwrap();
            assert_eq!(Some(expected_map), map);
        }

        #[test]
        fn parsing_empty_array() {
            // Testing a null pointer (length longer to ensure the null pointer is checked)
            let result =
                from_raw_int_array_and_string_array(std::ptr::null(), std::ptr::null(), 2).unwrap();
            assert_eq!(None, result);

            // Need a (filled) vector to obtain a valid pointer.
            let int_array = vec![1];
            let result =
                from_raw_int_array_and_string_array(int_array.as_ptr(), std::ptr::null(), 2)
                    .unwrap();
            assert_eq!(None, result);

            let array = vec![CString::new("glean").unwrap()];
            let ptr_array: Vec<*const _> = array.iter().map(|s| s.as_ptr()).collect();
            let result =
                from_raw_int_array_and_string_array(std::ptr::null(), ptr_array.as_ptr(), 2)
                    .unwrap();
            assert_eq!(None, result);

            // Check the length with valid pointers.
            let result =
                from_raw_int_array_and_string_array(int_array.as_ptr(), ptr_array.as_ptr(), 0)
                    .unwrap();
            assert_eq!(None, result);
        }

        #[test]
        fn parsing_invalid_utf8_fails() {
            // CAREFUL! We're manually constructing nul-terminated

            // Need a (filled) vector to obtain a valid pointer.
            let int_array = vec![1];
            let array = vec![
                // -1 is definitely an invalid UTF-8 codepoint
                // Let's not break anything and append the nul terminator
                vec![0x67, 0x6c, -1, 0x65, 0x61, 0x6e, 0x00],
            ];
            let ptr_array: Vec<*const _> = array.iter().map(|s| s.as_ptr()).collect();

            let map = from_raw_int_array_and_string_array(
                int_array.as_ptr(),
                ptr_array.as_ptr(),
                array.len() as i32,
            );
            assert!(map.is_err());
        }
    }

    mod raw_string_string_array {
        use super::*;

        #[test]
        fn parsing_valid_array() {
            let mut expected_map = HashMap::new();
            expected_map.insert("one".to_string(), "seven".to_string());
            expected_map.insert("two".to_string(), "eight".to_string());

            let key_array = vec![CString::new("one").unwrap(), CString::new("two").unwrap()];
            let ptr_key_array: Vec<*const _> = key_array.iter().map(|s| s.as_ptr()).collect();

            let str_array = vec![
                CString::new("seven").unwrap(),
                CString::new("eight").unwrap(),
            ];
            let ptr_array: Vec<*const _> = str_array.iter().map(|s| s.as_ptr()).collect();

            let map = from_raw_string_array_and_string_array(
                ptr_key_array.as_ptr(),
                ptr_array.as_ptr(),
                expected_map.len() as i32,
            )
            .unwrap();
            assert_eq!(Some(expected_map), map);
        }

        #[test]
        fn parsing_empty_array() {
            // Testing a null pointer (length longer to ensure the null pointer is checked)
            let result =
                from_raw_string_array_and_string_array(std::ptr::null(), std::ptr::null(), 2)
                    .unwrap();
            assert_eq!(None, result);

            // Need a (filled) vector to obtain a valid pointer.
            let key_array = vec![CString::new("one").unwrap()];
            let ptr_key_array: Vec<*const _> = key_array.iter().map(|s| s.as_ptr()).collect();

            let str_array = vec![CString::new("seven").unwrap()];
            let ptr_array: Vec<*const _> = str_array.iter().map(|s| s.as_ptr()).collect();
            let result =
                from_raw_string_array_and_string_array(ptr_key_array.as_ptr(), std::ptr::null(), 2)
                    .unwrap();
            assert_eq!(None, result);

            let result =
                from_raw_int_array_and_string_array(std::ptr::null(), ptr_array.as_ptr(), 2)
                    .unwrap();
            assert_eq!(None, result);

            // Check the length with valid pointers.
            let result = from_raw_string_array_and_string_array(
                ptr_key_array.as_ptr(),
                ptr_array.as_ptr(),
                0,
            )
            .unwrap();
            assert_eq!(None, result);
        }

        #[test]
        fn parsing_invalid_utf8_fails() {
            // CAREFUL! We're manually constructing nul-terminated

            // Need a (filled) vector to obtain a valid pointer.
            let key_array = vec![CString::new("one").unwrap()];
            let ptr_key_array: Vec<*const _> = key_array.iter().map(|s| s.as_ptr()).collect();
            let array = vec![
                // -1 is definitely an invalid UTF-8 codepoint
                // Let's not break anything and append the nul terminator
                vec![0x67, 0x6c, -1, 0x65, 0x61, 0x6e, 0x00],
            ];
            let ptr_array: Vec<*const _> = array.iter().map(|s| s.as_ptr()).collect();

            let map = from_raw_string_array_and_string_array(
                ptr_key_array.as_ptr(),
                ptr_array.as_ptr(),
                array.len() as i32,
            );
            assert!(map.is_err());
        }
    }
}
