//! Safe wrappers for memory-accessing functions like `std::ptr::copy()`.
use std::ptr;

macro_rules! idx_check (
    ($slice:expr, $idx:expr) => {
        assert!($idx < $slice.len(),
            concat!("`", stringify!($idx), "` ({}) out of bounds. Length: {}"),
            $idx, $slice.len());
    }
);

macro_rules! len_check (
    ($slice:expr, $start:ident, $len:ident) => {
        assert!(
            $start.checked_add($len)
                .expect(concat!("Overflow evaluating ", stringify!($start + $len)))
                <= $slice.len(),
            "Length {} starting at {} is out of bounds (slice len {}).", $len, $start, $slice.len()
        )
    }
);

/// Copy `len` elements from `src_idx` to `dest_idx`. Ranges may overlap.
///
/// Safe wrapper for `memmove()`/`std::ptr::copy()`.
///
/// ###Panics
/// * If either `src_idx` or `dest_idx` are out of bounds, or if either of these plus `len` is out of
/// bounds.
/// * If `src_idx + len` or `dest_idx + len` overflows.
pub fn copy_over<T: Copy>(slice: &mut [T], src_idx: usize, dest_idx: usize, len: usize) {
    if slice.len() == 0 { return; }

    idx_check!(slice, src_idx);
    idx_check!(slice, dest_idx);
    len_check!(slice, src_idx, len);
    len_check!(slice, dest_idx, len);

    let src_ptr: *const T = &slice[src_idx];
    let dest_ptr: *mut T = &mut slice[dest_idx];

    unsafe {
        ptr::copy(src_ptr, dest_ptr, len);
    }
}

/// Safe wrapper for `std::ptr::write_bytes()`/`memset()`.
pub fn write_bytes(slice: &mut [u8], byte: u8) {
    unsafe {
        ptr::write_bytes(slice.as_mut_ptr(), byte, slice.len());
    }
}

/// Prepend `elems` to `vec`, resizing if necessary.
///
/// ###Panics
/// If `vec.len() + elems.len()` overflows.
pub fn prepend<T: Copy>(elems: &[T], vec: &mut Vec<T>) {
    // Our overflow check occurs here, no need to do it ourselves.
    vec.reserve(elems.len());

    let old_len = vec.len();
    let new_len = old_len + elems.len();

    unsafe {
        vec.set_len(new_len);
    }

    // Move the old elements down to the end.
    if old_len > 0 {
        copy_over(vec, 0, elems.len(), old_len);
    }

    vec[..elems.len()].copy_from_slice(elems);
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    #[should_panic]
    fn bounds_check() {
        let mut arr = [0i32, 1, 2, 3, 4, 5];

        copy_over(&mut arr, 2, 1, 7);
    }

    #[test]
    fn copy_empty() {
        let mut arr: [i32; 0] = [];

        copy_over(&mut arr, 0, 0, 0);
    }

    #[test]
    fn prepend_empty() {
        let mut vec: Vec<i32> = vec![];
        prepend(&[1, 2, 3], &mut vec);
    }

    #[test]
    fn prepend_i32() {
        let mut vec = vec![3, 4, 5];
        prepend(&[1, 2], &mut vec);
        assert_eq!(vec, &[1, 2, 3, 4, 5]);
    }
}

