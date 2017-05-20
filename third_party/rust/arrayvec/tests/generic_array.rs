#![cfg(feature = "use_generic_array")]

extern crate arrayvec;
#[macro_use]
extern crate generic_array;

use arrayvec::ArrayVec;

use generic_array::GenericArray;

use generic_array::typenum::U41;

#[test]
fn test_simple() {
    let mut vec: ArrayVec<GenericArray<i32, U41>> = ArrayVec::new();

    assert_eq!(vec.len(), 0);
    assert_eq!(vec.capacity(), 41);
    vec.extend(0..20);
    assert_eq!(vec.len(), 20);
    assert_eq!(&vec[..5], &[0, 1, 2, 3, 4]);
}

