#![recursion_limit = "128"]
#![no_std]
#[macro_use]
extern crate generic_array;
use core::cell::Cell;
use core::ops::{Add, Drop};
use generic_array::GenericArray;
use generic_array::functional::*;
use generic_array::sequence::*;
use generic_array::typenum::{U1, U3, U4, U97};

#[test]
fn test() {
    let mut list97 = [0; 97];
    for i in 0..97 {
        list97[i] = i as i32;
    }
    let l: GenericArray<i32, U97> = GenericArray::clone_from_slice(&list97);
    assert_eq!(l[0], 0);
    assert_eq!(l[1], 1);
    assert_eq!(l[32], 32);
    assert_eq!(l[56], 56);
}

#[test]
fn test_drop() {
    #[derive(Clone)]
    struct TestDrop<'a>(&'a Cell<u32>);

    impl<'a> Drop for TestDrop<'a> {
        fn drop(&mut self) {
            self.0.set(self.0.get() + 1);
        }
    }

    let drop_counter = Cell::new(0);
    {
        let _: GenericArray<TestDrop, U3> = arr![TestDrop; TestDrop(&drop_counter),
                           TestDrop(&drop_counter),
                           TestDrop(&drop_counter)];
    }
    assert_eq!(drop_counter.get(), 3);
}

#[test]
fn test_arr() {
    let test: GenericArray<u32, U3> = arr![u32; 1, 2, 3];
    assert_eq!(test[1], 2);
}

#[test]
fn test_copy() {
    let test = arr![u32; 1, 2, 3];
    let test2 = test;
    // if GenericArray is not copy, this should fail as a use of a moved value
    assert_eq!(test[1], 2);
    assert_eq!(test2[0], 1);
}

#[derive(Debug, PartialEq, Eq)]
struct NoClone<T>(T);

#[test]
fn test_from_slice() {
    let arr = [1, 2, 3, 4];
    let gen_arr = GenericArray::<_, U3>::from_slice(&arr[..3]);
    assert_eq!(&arr[..3], gen_arr.as_slice());
    let arr = [NoClone(1u32), NoClone(2), NoClone(3), NoClone(4)];
    let gen_arr = GenericArray::<_, U3>::from_slice(&arr[..3]);
    assert_eq!(&arr[..3], gen_arr.as_slice());
}

#[test]
fn test_from_mut_slice() {
    let mut arr = [1, 2, 3, 4];
    {
        let gen_arr = GenericArray::<_, U3>::from_mut_slice(&mut arr[..3]);
        gen_arr[2] = 10;
    }
    assert_eq!(arr, [1, 2, 10, 4]);
    let mut arr = [NoClone(1u32), NoClone(2), NoClone(3), NoClone(4)];
    {
        let gen_arr = GenericArray::<_, U3>::from_mut_slice(&mut arr[..3]);
        gen_arr[2] = NoClone(10);
    }
    assert_eq!(arr, [NoClone(1), NoClone(2), NoClone(10), NoClone(4)]);
}

#[test]
fn test_default() {
    let arr = GenericArray::<u8, U1>::default();
    assert_eq!(arr[0], 0);
}

#[test]
fn test_from() {
    let data = [(1, 2, 3), (4, 5, 6), (7, 8, 9)];
    let garray: GenericArray<(usize, usize, usize), U3> = data.into();
    assert_eq!(&data, garray.as_slice());
}

#[test]
fn test_unit_macro() {
    let arr = arr![f32; 3.14];
    assert_eq!(arr[0], 3.14);
}

#[test]
fn test_empty_macro() {
    let _arr = arr![f32;];
}

#[test]
fn test_cmp() {
    arr![u8; 0x00].cmp(&arr![u8; 0x00]);
}

/// This test should cause a helpful compile error if uncommented.
// #[test]
// fn test_empty_macro2(){
//     let arr = arr![];
// }
#[cfg(feature = "serde")]
mod impl_serde {
    extern crate serde_json;

    use generic_array::GenericArray;
    use generic_array::typenum::U6;

    #[test]
    fn test_serde_implementation() {
        let array: GenericArray<f64, U6> = arr![f64; 0.0, 5.0, 3.0, 7.07192, 76.0, -9.0];
        let string = serde_json::to_string(&array).unwrap();
        assert_eq!(string, "[0.0,5.0,3.0,7.07192,76.0,-9.0]");

        let test_array: GenericArray<f64, U6> = serde_json::from_str(&string).unwrap();
        assert_eq!(test_array, array);
    }
}

#[test]
fn test_map() {
    let b: GenericArray<i32, U4> = GenericArray::generate(|i| i as i32 * 4).map(|x| x - 3);

    assert_eq!(b, arr![i32; -3, 1, 5, 9]);
}

#[test]
fn test_zip() {
    let a: GenericArray<_, U4> = GenericArray::generate(|i| i + 1);
    let b: GenericArray<_, U4> = GenericArray::generate(|i| i as i32 * 4);

    // Uses reference and non-reference arguments
    let c = (&a).zip(b, |r, l| *r as i32 + l);

    assert_eq!(c, arr![i32; 1, 6, 11, 16]);
}

#[test]
#[should_panic]
fn test_from_iter_short() {
    use core::iter::repeat;

    let a: GenericArray<_, U4> = repeat(11).take(3).collect();

    assert_eq!(a, arr![i32; 11, 11, 11, 0]);
}

#[test]
fn test_from_iter() {
    use core::iter::{once, repeat};

    let a: GenericArray<_, U4> = repeat(11).take(3).chain(once(0)).collect();

    assert_eq!(a, arr![i32; 11, 11, 11, 0]);
}

#[test]
fn test_sizes() {
    #![allow(dead_code)]
    use core::mem::{size_of, size_of_val};

    #[derive(Debug, Copy, Clone)]
    #[repr(C)]
    #[repr(packed)]
    struct Test {
        t: u16,
        s: u32,
        r: u16,
        f: u16,
        o: u32,
    }

    assert_eq!(size_of::<Test>(), 14);

    assert_eq!(size_of_val(&arr![u8; 1, 2, 3]), size_of::<u8>() * 3);
    assert_eq!(size_of_val(&arr![u32; 1]), size_of::<u32>() * 1);
    assert_eq!(size_of_val(&arr![u64; 1, 2, 3, 4]), size_of::<u64>() * 4);

    assert_eq!(size_of::<GenericArray<Test, U97>>(), size_of::<Test>() * 97);
}

#[test]
fn test_append() {
    let a = arr![i32; 1, 2, 3];

    let b = a.append(4);

    assert_eq!(b, arr![i32; 1, 2, 3, 4]);
}

#[test]
fn test_prepend() {
    let a = arr![i32; 1, 2, 3];

    let b = a.prepend(4);

    assert_eq!(b, arr![i32; 4, 1, 2, 3]);
}

#[test]
fn test_pop() {
    let a = arr![i32; 1, 2, 3, 4];

    let (init, last) = a.pop_back();

    assert_eq!(init, arr![i32; 1, 2, 3]);
    assert_eq!(last, 4);

    let (head, tail) = a.pop_front();

    assert_eq!(head, 1);
    assert_eq!(tail, arr![i32; 2, 3, 4]);
}

#[test]
fn test_split() {
    let a = arr![i32; 1, 2, 3, 4];

    let (b, c) = a.split();

    assert_eq!(b, arr![i32; 1]);
    assert_eq!(c, arr![i32; 2, 3, 4]);

    let (e, f) = a.split();

    assert_eq!(e, arr![i32; 1, 2]);
    assert_eq!(f, arr![i32; 3, 4]);
}

#[test]
fn test_concat() {
    let a = arr![i32; 1, 2];
    let b = arr![i32; 3, 4];

    let c = a.concat(b);

    assert_eq!(c, arr![i32; 1, 2, 3, 4]);

    let (d, e) = c.split();

    assert_eq!(d, arr![i32; 1]);
    assert_eq!(e, arr![i32; 2, 3, 4]);
}

#[test]
fn test_fold() {
    let a = arr![i32; 1, 2, 3, 4];

    assert_eq!(10, a.fold(0, |a, x| a + x));
}

fn sum_generic<S>(s: S) -> i32
where
    S: FunctionalSequence<i32>,
    S::Item: Add<i32, Output = i32>, // `+`
    i32: Add<S::Item, Output = i32>, // reflexive
{
    s.fold(0, |a, x| a + x)
}

#[test]
fn test_sum() {
    let a = sum_generic(arr![i32; 1, 2, 3, 4]);

    assert_eq!(a, 10);
}
