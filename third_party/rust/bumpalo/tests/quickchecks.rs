extern crate bumpalo;
extern crate quickcheck;

use bumpalo::Bump;
use quickcheck::{quickcheck, Arbitrary, Gen};
use std::mem;

#[derive(Clone, Debug, PartialEq)]
struct BigValue {
    data: [u64; 32],
}

impl BigValue {
    fn new(x: u64) -> BigValue {
        BigValue {
            data: [
                x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x, x,
                x, x, x, x,
            ],
        }
    }
}

impl Arbitrary for BigValue {
    fn arbitrary<G: Gen>(g: &mut G) -> BigValue {
        BigValue::new(u64::arbitrary(g))
    }
}

#[derive(Clone, Debug)]
enum Elems<T, U> {
    OneT(T),
    TwoT(T, T),
    FourT(T, T, T, T),
    OneU(U),
    TwoU(U, U),
    FourU(U, U, U, U),
}

impl<T, U> Arbitrary for Elems<T, U>
where
    T: Arbitrary + Clone,
    U: Arbitrary + Clone,
{
    fn arbitrary<G: Gen>(g: &mut G) -> Elems<T, U> {
        let x: u8 = u8::arbitrary(g);
        match x % 6 {
            0 => Elems::OneT(T::arbitrary(g)),
            1 => Elems::TwoT(T::arbitrary(g), T::arbitrary(g)),
            2 => Elems::FourT(
                T::arbitrary(g),
                T::arbitrary(g),
                T::arbitrary(g),
                T::arbitrary(g),
            ),
            3 => Elems::OneU(U::arbitrary(g)),
            4 => Elems::TwoU(U::arbitrary(g), U::arbitrary(g)),
            5 => Elems::FourU(
                U::arbitrary(g),
                U::arbitrary(g),
                U::arbitrary(g),
                U::arbitrary(g),
            ),
            _ => unreachable!(),
        }
    }

    fn shrink(&self) -> Box<dyn Iterator<Item = Self>> {
        match self {
            Elems::OneT(_) => Box::new(vec![].into_iter()),
            Elems::TwoT(a, b) => {
                Box::new(vec![Elems::OneT(a.clone()), Elems::OneT(b.clone())].into_iter())
            }
            Elems::FourT(a, b, c, d) => Box::new(
                vec![
                    Elems::TwoT(a.clone(), b.clone()),
                    Elems::TwoT(a.clone(), c.clone()),
                    Elems::TwoT(a.clone(), d.clone()),
                    Elems::TwoT(b.clone(), c.clone()),
                    Elems::TwoT(b.clone(), d.clone()),
                ]
                .into_iter(),
            ),
            Elems::OneU(_) => Box::new(vec![].into_iter()),
            Elems::TwoU(a, b) => {
                Box::new(vec![Elems::OneU(a.clone()), Elems::OneU(b.clone())].into_iter())
            }
            Elems::FourU(a, b, c, d) => Box::new(
                vec![
                    Elems::TwoU(a.clone(), b.clone()),
                    Elems::TwoU(a.clone(), c.clone()),
                    Elems::TwoU(a.clone(), d.clone()),
                    Elems::TwoU(b.clone(), c.clone()),
                    Elems::TwoU(b.clone(), d.clone()),
                ]
                .into_iter(),
            ),
        }
    }
}

fn overlap((a1, a2): (usize, usize), (b1, b2): (usize, usize)) -> bool {
    assert!(a1 < a2);
    assert!(b1 < b2);
    a1 < b2 && b1 < a2
}

fn range<T>(t: &T) -> (usize, usize) {
    let start = t as *const _ as usize;
    let end = start + mem::size_of::<T>();
    (start, end)
}

quickcheck! {
    fn can_allocate_big_values(values: Vec<BigValue>) -> () {
        let bump = Bump::new();
        let mut alloced = vec![];

        for vals in values.iter().cloned() {
            alloced.push(bump.alloc(vals));
        }

        for (vals, alloc) in values.iter().zip(alloced.into_iter()) {
            assert_eq!(vals, alloc);
        }
    }

    fn big_allocations_never_overlap(values: Vec<BigValue>) -> () {
        let bump = Bump::new();
        let mut alloced = vec![];

        for v in values {
            let a = bump.alloc(v);
            let start = a as *const _ as usize;
            let end = unsafe { (a as *const BigValue).offset(1) as usize };
            let range = (start, end);

            for r in &alloced {
                assert!(!overlap(*r, range));
            }

            alloced.push(range);
        }
    }

    fn can_allocate_heterogeneous_things_and_they_dont_overlap(things: Vec<Elems<u8, u64>>) -> () {
        let bump = Bump::new();
        let mut ranges = vec![];

        for t in things {
            let r = match t {
                Elems::OneT(a) => {
                    range(bump.alloc(a))
                },
                Elems::TwoT(a, b) => {
                    range(bump.alloc([a, b]))
                },
                Elems::FourT(a, b, c, d) => {
                    range(bump.alloc([a, b, c, d]))
                },
                Elems::OneU(a) => {
                    range(bump.alloc(a))
                },
                Elems::TwoU(a, b) => {
                    range(bump.alloc([a, b]))
                },
                Elems::FourU(a, b, c, d) => {
                    range(bump.alloc([a, b, c, d]))
                },
            };

            for s in &ranges {
                assert!(!overlap(r, *s));
            }

            ranges.push(r);
        }
    }
}
