// SPDX-FileCopyrightText: 2018 - 2022 Kamila Borowska <kamila@borowska.pw>
// SPDX-FileCopyrightText: 2019 Riey <creeper844@gmail.com>
// SPDX-FileCopyrightText: 2020 Amanieu d'Antras <amanieu@gmail.com>
// SPDX-FileCopyrightText: 2021 Bruno Corrêa Zimmermann <brunoczim@gmail.com>
// SPDX-FileCopyrightText: 2021 micycle
// SPDX-FileCopyrightText: 2022 Cass Fridkin <cass@cloudflare.com>
//
// SPDX-License-Identifier: MIT OR Apache-2.0

#[macro_use]
extern crate enum_map;

use enum_map::{Enum, EnumArray, EnumMap, IntoIter};

use std::cell::{Cell, RefCell};
use std::collections::HashSet;
use std::convert::Infallible;
use std::marker::PhantomData;
use std::num::ParseIntError;
use std::panic::{catch_unwind, UnwindSafe};

trait From<T>: Sized {
    fn from(_: T) -> Self {
        unreachable!();
    }
}

impl<T, U> From<T> for U {}

#[derive(Copy, Clone, Debug, Enum, PartialEq)]
enum Example {
    A,
    B,
    C,
}

#[test]
fn test_bool() {
    let mut map = enum_map! { false => 24, true => 42 };
    assert_eq!(map[false], 24);
    assert_eq!(map[true], 42);
    map[false] += 1;
    assert_eq!(map[false], 25);
    for (key, item) in &mut map {
        if !key {
            *item += 1;
        }
    }
    assert_eq!(map[false], 26);
    assert_eq!(map[true], 42);
}

#[test]
fn test_clone() {
    let map = enum_map! { false => 3, true => 5 };
    assert_eq!(map.clone(), map);
}

#[test]
fn test_debug() {
    let map = enum_map! { false => 3, true => 5 };
    assert_eq!(format!("{:?}", map), "{false: 3, true: 5}");
}

#[test]
fn test_hash() {
    let map = enum_map! { false => 3, true => 5 };
    let mut set = HashSet::new();
    set.insert(map);
    assert!(set.contains(&map));
}

#[test]
fn test_clear() {
    let mut map = enum_map! { false => 1, true => 2 };
    map.clear();
    assert_eq!(map[true], 0);
    assert_eq!(map[false], 0);
}

#[test]
fn struct_of_enum() {
    #[derive(Copy, Clone, Debug, Enum, PartialEq)]
    struct Product {
        example: Example,
        is_done: bool,
    }

    let mut map = enum_map! {
        Product { example: Example::A, is_done: false } => "foo",
        Product { example: Example::B, is_done: false } => "bar",
        Product { example: Example::C, is_done: false } => "baz",
        Product { example: Example::A, is_done: true } => "done foo",
        Product { example: Example::B, is_done: true } => "bar done",
        Product { example: Example::C, is_done: true } => "doooozne",
    };

    assert_eq!(
        map[Product {
            example: Example::B,
            is_done: false
        }],
        "bar"
    );
    assert_eq!(
        map[Product {
            example: Example::C,
            is_done: false
        }],
        "baz"
    );
    assert_eq!(
        map[Product {
            example: Example::B,
            is_done: true
        }],
        "bar done"
    );

    map[Product {
        example: Example::B,
        is_done: true,
    }] = "not really done";
    assert_eq!(
        map[Product {
            example: Example::B,
            is_done: false
        }],
        "bar"
    );
    assert_eq!(
        map[Product {
            example: Example::C,
            is_done: false
        }],
        "baz"
    );
    assert_eq!(
        map[Product {
            example: Example::B,
            is_done: true
        }],
        "not really done"
    );
}

#[test]
fn tuple_struct_of_enum() {
    #[derive(Copy, Clone, Debug, Enum, PartialEq)]
    struct Product(Example, bool);

    let mut map = enum_map! {
        Product(Example::A, false) => "foo",
        Product(Example::B, false) => "bar",
        Product(Example::C, false) => "baz",
        Product(Example::A, true) => "done foo",
        Product(Example::B, true) => "bar done",
        Product(Example::C, true) => "doooozne",
    };

    assert_eq!(map[Product(Example::B, false)], "bar");
    assert_eq!(map[Product(Example::C, false)], "baz");
    assert_eq!(map[Product(Example::B, true)], "bar done");

    map[Product(Example::B, true)] = "not really done";
    assert_eq!(map[Product(Example::B, false)], "bar");
    assert_eq!(map[Product(Example::C, false)], "baz");
    assert_eq!(map[Product(Example::B, true)], "not really done");
}

#[test]
fn discriminants() {
    #[derive(Debug, Enum, PartialEq)]
    enum Discriminants {
        A = 2000,
        B = 3000,
        C = 1000,
    }
    let mut map = EnumMap::default();
    map[Discriminants::A] = 3;
    map[Discriminants::B] = 2;
    map[Discriminants::C] = 1;
    let mut pairs = map.iter();
    assert_eq!(pairs.next(), Some((Discriminants::A, &3)));
    assert_eq!(pairs.next(), Some((Discriminants::B, &2)));
    assert_eq!(pairs.next(), Some((Discriminants::C, &1)));
    assert_eq!(pairs.next(), None);
}

#[test]
fn extend() {
    let mut map = enum_map! { _ => 0 };
    map.extend(vec![(Example::A, 3)]);
    map.extend(vec![(&Example::B, &4)]);
    assert_eq!(
        map,
        enum_map! { Example::A => 3, Example::B => 4, Example::C => 0 }
    );
}

#[test]
fn collect() {
    let iter = vec![(Example::A, 5), (Example::B, 7)]
        .into_iter()
        .map(|(k, v)| (k, v + 1));
    assert_eq!(
        iter.collect::<EnumMap<_, _>>(),
        enum_map! { Example::A => 6, Example::B => 8, Example::C => 0 }
    );
}

#[test]
fn huge_enum() {
    #[derive(Enum)]
    enum Example {
        A,
        B,
        C,
        D,
        E,
        F,
        G,
        H,
        I,
        J,
        K,
        L,
        M,
        N,
        O,
        P,
        Q,
        R,
        S,
        T,
        U,
        V,
        W,
        X,
        Y,
        Z,
        Aa,
        Bb,
        Cc,
        Dd,
        Ee,
        Ff,
        Gg,
        Hh,
        Ii,
        Jj,
        Kk,
        Ll,
        Mm,
        Nn,
        Oo,
        Pp,
        Qq,
        Rr,
        Ss,
        Tt,
        Uu,
        Vv,
        Ww,
        Xx,
        Yy,
        Zz,
    }

    let map = enum_map! { _ => 2 };
    assert_eq!(map[Example::Xx], 2);
}

#[test]
fn iterator_len() {
    assert_eq!(
        enum_map! { Example::A | Example::B | Example::C => 0 }
            .iter()
            .len(),
        3
    );
}

#[test]
fn iter_mut_len() {
    assert_eq!(
        enum_map! { Example::A | Example::B | Example::C => 0 }
            .iter_mut()
            .len(),
        3
    );
}

#[test]
fn into_iter_len() {
    assert_eq!(enum_map! { Example::A | _ => 0 }.into_iter().len(), 3);
}

#[test]
fn iterator_next_back() {
    assert_eq!(
        enum_map! { Example::A => 1, Example::B => 2, Example::C => 3 }
            .iter()
            .next_back(),
        Some((Example::C, &3))
    );
}

#[test]
fn iter_mut_next_back() {
    assert_eq!(
        enum_map! { Example::A => 1, Example::B => 2, Example::C => 3 }
            .iter_mut()
            .next_back(),
        Some((Example::C, &mut 3))
    );
}

#[test]
fn into_iter() {
    let mut iter = enum_map! { true => 5, false => 7 }.into_iter();
    assert_eq!(iter.next(), Some((false, 7)));
    assert_eq!(iter.next(), Some((true, 5)));
    assert_eq!(iter.next(), None);
    assert_eq!(iter.next(), None);
}

#[test]
fn into_iter_u8() {
    assert_eq!(
        enum_map! { i => i }.into_iter().collect::<Vec<_>>(),
        (0..256).map(|x| (x as u8, x as u8)).collect::<Vec<_>>()
    );
}

struct DropReporter<'a> {
    into: &'a RefCell<Vec<usize>>,
    value: usize,
}

impl<'a> Drop for DropReporter<'a> {
    fn drop(&mut self) {
        self.into.borrow_mut().push(self.value);
    }
}

#[test]
fn into_iter_drop() {
    let dropped = RefCell::new(Vec::default());
    let mut a: IntoIter<Example, _> = enum_map! {
        k => DropReporter {
            into: &dropped,
            value: k as usize,
        },
    }
    .into_iter();
    assert_eq!(a.next().unwrap().0, Example::A);
    assert_eq!(*dropped.borrow(), &[0]);
    drop(a);
    assert_eq!(*dropped.borrow(), &[0, 1, 2]);
}

#[test]
fn into_iter_double_ended_iterator() {
    let mut iter = enum_map! { 0 => 5, 255 => 7, _ => 0 }.into_iter();
    assert_eq!(iter.next(), Some((0, 5)));
    assert_eq!(iter.next_back(), Some((255, 7)));
    assert_eq!(iter.next(), Some((1, 0)));
    assert_eq!(iter.next_back(), Some((254, 0)));
    assert!(iter.rev().eq((2..254).rev().map(|i| (i, 0))));
}

#[test]
fn values_rev_collect() {
    assert_eq!(
        vec![3, 2, 1],
        enum_map! { Example::A => 1, Example::B => 2, Example::C => 3 }
            .values()
            .rev()
            .cloned()
            .collect::<Vec<_>>()
    );
}

#[test]
fn values_len() {
    assert_eq!(enum_map! { false => 0, true => 1 }.values().len(), 2);
}

#[test]
fn into_values_rev_collect() {
    assert_eq!(
        vec![3, 2, 1],
        enum_map! { Example::A => 1, Example::B => 2, Example::C => 3 }
            .into_values()
            .rev()
            .collect::<Vec<_>>()
    );
}

#[test]
fn into_values_len() {
    assert_eq!(enum_map! { false => 0, true => 1 }.into_values().len(), 2);
}

#[test]
fn values_mut_next_back() {
    let mut map = enum_map! { false => 0, true => 1 };
    assert_eq!(map.values_mut().next_back(), Some(&mut 1));
}
#[test]
fn test_u8() {
    let mut map = enum_map! { b'a' => 4, _ => 0 };
    map[b'c'] = 3;
    assert_eq!(map[b'a'], 4);
    assert_eq!(map[b'b'], 0);
    assert_eq!(map[b'c'], 3);
    assert_eq!(map.iter().next(), Some((0, &0)));
}

#[derive(Enum)]
enum Void {}

#[test]
fn empty_map() {
    let void: EnumMap<Void, Void> = enum_map! {};
    assert_eq!(void.len(), 0);
}

#[test]
#[should_panic]
fn empty_value() {
    let _void: EnumMap<bool, Void> = enum_map! { _ => unreachable!() };
}

#[test]
fn empty_infallible_map() {
    let void: EnumMap<Infallible, Infallible> = enum_map! {};
    assert_eq!(void.len(), 0);
}

#[derive(Clone, Copy)]
enum X {
    A(PhantomData<*const ()>),
}

impl Enum for X {
    const LENGTH: usize = 1;

    fn from_usize(arg: usize) -> X {
        assert_eq!(arg, 0);
        X::A(PhantomData)
    }

    fn into_usize(self) -> usize {
        0
    }
}

impl<V> EnumArray<V> for X {
    type Array = [V; Self::LENGTH];
}

fn assert_sync_send<T: Sync + Send>(_: T) {}

#[test]
fn assert_enum_map_does_not_copy_sync_send_dependency_of_keys() {
    let mut map = enum_map! { X::A(PhantomData) => true };
    assert_sync_send(map);
    assert_sync_send(&map);
    assert_sync_send(&mut map);
    assert_sync_send(map.iter());
    assert_sync_send(map.iter_mut());
    assert_sync_send(map.into_iter());
    assert!(map[X::A(PhantomData)]);
}

#[test]
fn test_sum() {
    assert_eq!(
        enum_map! { i => u8::into(i) }
            .iter()
            .map(|(_, v)| v)
            .sum::<u32>(),
        32_640
    );
}

#[test]
fn test_sum_mut() {
    assert_eq!(
        enum_map! { i => u8::into(i) }
            .iter_mut()
            .map(|(_, &mut v)| -> u32 { v })
            .sum::<u32>(),
        32_640
    );
}

#[test]
fn test_iter_clone() {
    struct S(u8);
    let map = enum_map! {
        Example::A => S(3),
        Example::B => S(4),
        Example::C => S(1),
    };
    let iter = map.iter();
    assert_eq!(iter.clone().map(|(_, S(v))| v).sum::<u8>(), 8);
    assert_eq!(iter.map(|(_, S(v))| v).sum::<u8>(), 8);
    let values = map.values();
    assert_eq!(values.clone().map(|S(v)| v).sum::<u8>(), 8);
    assert_eq!(values.map(|S(v)| v).sum::<u8>(), 8);
}

#[test]
fn question_mark() -> Result<(), ParseIntError> {
    let map = enum_map! { false => "2".parse()?, true => "5".parse()? };
    assert_eq!(map, enum_map! { false => 2, true => 5 });
    Ok(())
}

#[test]
fn question_mark_failure() {
    struct IncOnDrop<'a>(&'a Cell<i32>);

    impl Drop for IncOnDrop<'_> {
        fn drop(&mut self) {
            self.0.set(self.0.get() + 1);
        }
    }

    fn failible() -> Result<IncOnDrop<'static>, &'static str> {
        Err("ERROR!")
    }

    fn try_block(inc: &Cell<i32>) -> Result<(), &'static str> {
        enum_map! {
            32 => failible()?,
            _ => {
                IncOnDrop(inc)
            }
        };
        Ok(())
    }
    let value = Cell::new(0);
    assert_eq!(try_block(&value), Err("ERROR!"));
    assert_eq!(value.get(), 32);
}

#[test]
#[should_panic = "Intentional panic"]
fn map_panic() {
    let map: EnumMap<u8, String> = enum_map! { i => i.to_string() };
    map.map(|k, v| {
        if k == 2 {
            panic!("Intentional panic");
        }
        v + " modified"
    });
}

macro_rules! make_enum_map_macro_safety_test {
    ($a:tt $b:tt) => {
        // This is misuse of an API, however we need to test that to ensure safety
        // as we use unsafe code.
        enum E {
            A,
            B,
            C,
        }

        impl Enum for E {
            const LENGTH: usize = $a;

            fn from_usize(value: usize) -> E {
                match value {
                    0 => E::A,
                    1 => E::B,
                    2 => E::C,
                    _ => unimplemented!(),
                }
            }

            fn into_usize(self) -> usize {
                self as usize
            }
        }

        impl<V> EnumArray<V> for E {
            type Array = [V; $b];
        }

        let map: EnumMap<E, String> = enum_map! { _ => "Hello, world!".into() };
        map.into_iter();
    };
}

#[test]
fn enum_map_macro_safety_under() {
    make_enum_map_macro_safety_test!(2 3);
}

#[test]
fn enum_map_macro_safety_over() {
    make_enum_map_macro_safety_test!(3 2);
}

#[test]
fn drop_panic_into_iter() {
    struct DropHandler<'a>(&'a Cell<usize>);
    impl Drop for DropHandler<'_> {
        fn drop(&mut self) {
            self.0.set(self.0.get() + 1);
        }
    }
    impl UnwindSafe for DropHandler<'_> {}
    struct Storage<'a> {
        should_panic: bool,
        _drop_handler: DropHandler<'a>,
    }
    impl Drop for Storage<'_> {
        fn drop(&mut self) {
            if self.should_panic {
                panic!();
            }
        }
    }
    let cell = Cell::new(0);
    let map: EnumMap<Example, _> = enum_map! {
        v => Storage { should_panic: v == Example::B, _drop_handler: DropHandler(&cell) },
    };
    assert!(catch_unwind(|| {
        map.into_iter();
    })
    .is_err());
    assert_eq!(cell.get(), 3);
}

#[test]
fn test_const_enum_map_from_array() {
    const CONST_ENUM_MAP_FROM_ARRAY: EnumMap<bool, u32> = EnumMap::from_array([4, 8]);
    assert_eq!(
        CONST_ENUM_MAP_FROM_ARRAY,
        enum_map! { false => 4, true => 8 },
    );
}

#[test]
fn usize_override() {
    #[allow(non_camel_case_types, dead_code)]
    type usize = ();
    #[derive(Enum)]
    enum X {
        A,
        B,
    }
}

// Regression test for https://codeberg.org/xfix/enum-map/issues/112
#[test]
fn test_issue_112() {
    #[derive(Enum, PartialEq, Debug)]
    enum Inner {
        Inner1,
        Inner2,
    }

    #[derive(Enum, PartialEq, Debug)]
    enum Outer {
        A,
        B(Inner),
        C,
        D(Inner, Inner),
        E,
    }

    assert_eq!(Outer::A.into_usize(), 0);
    assert_eq!(Outer::A, Outer::from_usize(0));
    assert_eq!(Outer::B(Inner::Inner1).into_usize(), 1);
    assert_eq!(Outer::B(Inner::Inner1), Outer::from_usize(1));
    assert_eq!(Outer::B(Inner::Inner2).into_usize(), 2);
    assert_eq!(Outer::B(Inner::Inner2), Outer::from_usize(2));
    assert_eq!(Outer::C.into_usize(), 3);
    assert_eq!(Outer::C, Outer::from_usize(3));
    assert_eq!(Outer::D(Inner::Inner1, Inner::Inner1).into_usize(), 4);
    assert_eq!(Outer::D(Inner::Inner1, Inner::Inner1), Outer::from_usize(4));
    assert_eq!(Outer::D(Inner::Inner2, Inner::Inner1).into_usize(), 5);
    assert_eq!(Outer::D(Inner::Inner2, Inner::Inner1), Outer::from_usize(5));
    assert_eq!(Outer::D(Inner::Inner1, Inner::Inner2).into_usize(), 6);
    assert_eq!(Outer::D(Inner::Inner1, Inner::Inner2), Outer::from_usize(6));
    assert_eq!(Outer::D(Inner::Inner2, Inner::Inner2).into_usize(), 7);
    assert_eq!(Outer::D(Inner::Inner2, Inner::Inner2), Outer::from_usize(7));
    assert_eq!(Outer::E.into_usize(), 8);
    assert_eq!(Outer::E, Outer::from_usize(8));
}
