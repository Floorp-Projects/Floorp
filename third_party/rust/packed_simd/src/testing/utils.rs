//! Testing utilities

#![allow(dead_code)]

use crate::{cmp::PartialOrd, fmt::Debug, LexicographicallyOrdered};

/// Tests PartialOrd for `a` and `b` where `a < b` is true.
pub fn test_lt<T>(
    a: LexicographicallyOrdered<T>, b: LexicographicallyOrdered<T>,
) where
    LexicographicallyOrdered<T>: Debug + PartialOrd,
{
    assert!(a < b, "{:?}, {:?}", a, b);
    assert!(b > a, "{:?}, {:?}", a, b);

    assert!(!(a == b), "{:?}, {:?}", a, b);
    assert!(a != b, "{:?}, {:?}", a, b);

    assert!(a <= b, "{:?}, {:?}", a, b);
    assert!(b >= a, "{:?}, {:?}", a, b);

    // Irreflexivity
    assert!(!(a < a), "{:?}, {:?}", a, b);
    assert!(!(b < b), "{:?}, {:?}", a, b);
    assert!(!(a > a), "{:?}, {:?}", a, b);
    assert!(!(b > b), "{:?}, {:?}", a, b);

    assert!(a <= a, "{:?}, {:?}", a, b);
    assert!(b <= b, "{:?}, {:?}", a, b);
}

/// Tests PartialOrd for `a` and `b` where `a <= b` is true.
pub fn test_le<T>(
    a: LexicographicallyOrdered<T>, b: LexicographicallyOrdered<T>,
) where
    LexicographicallyOrdered<T>: Debug + PartialOrd,
{
    assert!(a <= b, "{:?}, {:?}", a, b);
    assert!(b >= a, "{:?}, {:?}", a, b);

    assert!(a == b || a < b, "{:?}, {:?}", a, b);
    assert!(a == b || b > a, "{:?}, {:?}", a, b);

    if a == b {
        assert!(!(a < b), "{:?}, {:?}", a, b);
        assert!(!(b > a), "{:?}, {:?}", a, b);

        assert!(!(a != b), "{:?}, {:?}", a, b);
    } else {
        assert!(a != b, "{:?}, {:?}", a, b);
        test_lt(a, b);
    }
}

/// Test PartialOrd::partial_cmp for `a` and `b` returning `Ordering`
pub fn test_cmp<T>(
    a: LexicographicallyOrdered<T>, b: LexicographicallyOrdered<T>,
    o: Option<crate::cmp::Ordering>,
) where
    LexicographicallyOrdered<T>: PartialOrd + Debug,
    T: Debug + crate::sealed::Simd + Copy + Clone,
    <T as crate::sealed::Simd>::Element: Default + Copy + Clone + PartialOrd,
{
    assert!(T::LANES <= 64, "array length in these two arrays needs updating");
    let mut arr_a: [T::Element; 64] = [Default::default(); 64];
    let mut arr_b: [T::Element; 64] = [Default::default(); 64];

    unsafe {
        crate::ptr::write_unaligned(
            arr_a.as_mut_ptr() as *mut LexicographicallyOrdered<T>,
            a,
        )
    }
    unsafe {
        crate::ptr::write_unaligned(
            arr_b.as_mut_ptr() as *mut LexicographicallyOrdered<T>,
            b,
        )
    }
    let expected = arr_a[0..T::LANES].partial_cmp(&arr_b[0..T::LANES]);
    let result = a.partial_cmp(&b);
    assert_eq!(expected, result, "{:?}, {:?}", a, b);
    assert_eq!(o, result, "{:?}, {:?}", a, b);
    match o {
        Some(crate::cmp::Ordering::Less) => {
            test_lt(a, b);
            test_le(a, b);
        }
        Some(crate::cmp::Ordering::Greater) => {
            test_lt(b, a);
            test_le(b, a);
        }
        Some(crate::cmp::Ordering::Equal) => {
            assert!(a == b, "{:?}, {:?}", a, b);
            assert!(!(a != b), "{:?}, {:?}", a, b);
            assert!(!(a < b), "{:?}, {:?}", a, b);
            assert!(!(b < a), "{:?}, {:?}", a, b);
            assert!(!(a > b), "{:?}, {:?}", a, b);
            assert!(!(b > a), "{:?}, {:?}", a, b);

            test_le(a, b);
            test_le(b, a);
        }
        None => {
            assert!(!(a == b), "{:?}, {:?}", a, b);
            assert!(!(a != b), "{:?}, {:?}", a, b);
            assert!(!(a < b), "{:?}, {:?}", a, b);
            assert!(!(a > b), "{:?}, {:?}", a, b);
            assert!(!(b < a), "{:?}, {:?}", a, b);
            assert!(!(b > a), "{:?}, {:?}", a, b);
            assert!(!(a <= b), "{:?}, {:?}", a, b);
            assert!(!(b <= a), "{:?}, {:?}", a, b);
            assert!(!(a >= b), "{:?}, {:?}", a, b);
            assert!(!(b >= a), "{:?}, {:?}", a, b);
        }
    }
}

// Returns a tuple containing two distinct pointer values of the same type as
// the element type of the Simd vector `$id`.
#[allow(unused)]
macro_rules! ptr_vals {
    ($id:ty) => {
        // expands to an expression
        #[allow(unused_unsafe)]
        unsafe {
            // all bits cleared
            let clear: <$id as sealed::Simd>::Element = crate::mem::zeroed();
            // all bits set
            let set: <$id as sealed::Simd>::Element =
                crate::mem::transmute(-1_isize);
            (clear, set)
        }
    };
}
