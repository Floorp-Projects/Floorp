//! The purpose of these tests is to cover corner cases of iterators
//! and adaptors.
//!
//! In particular we test the tedious size_hint and exact size correctness.

#[macro_use] extern crate itertools;

extern crate quickcheck;

use std::default::Default;

use quickcheck as qc;
use std::ops::Range;
use std::cmp::Ordering;
use itertools::Itertools;
use itertools::{
    multizip,
    EitherOrBoth,
};
use itertools::free::{
    cloned,
    enumerate,
    multipeek,
    put_back,
    put_back_n,
    rciter,
    zip,
    zip_eq,
};

use quickcheck::TestResult;

/// Our base iterator that we can impl Arbitrary for
///
/// NOTE: Iter is tricky and is not fused, to help catch bugs.
/// At the end it will return None once, then return Some(0),
/// then return None again.
#[derive(Clone, Debug)]
struct Iter<T>(Range<T>, i32); // with fuse/done flag

impl<T> Iter<T>
{
    fn new(it: Range<T>) -> Self
    {
        Iter(it, 0)
    }
}

impl<T> Iterator for Iter<T> where Range<T>: Iterator,
    <Range<T> as Iterator>::Item: Default,
{
    type Item = <Range<T> as Iterator>::Item;

    fn next(&mut self) -> Option<Self::Item>
    {
        let elt = self.0.next();
        if elt.is_none() {
            self.1 += 1;
            // check fuse flag
            if self.1 == 2 {
                return Some(Default::default())
            }
        }
        elt
    }

    fn size_hint(&self) -> (usize, Option<usize>)
    {
        self.0.size_hint()
    }
}

impl<T> DoubleEndedIterator for Iter<T> where Range<T>: DoubleEndedIterator,
    <Range<T> as Iterator>::Item: Default,
{
    fn next_back(&mut self) -> Option<Self::Item> { self.0.next_back() }
}

impl<T> ExactSizeIterator for Iter<T> where Range<T>: ExactSizeIterator,
    <Range<T> as Iterator>::Item: Default,
{ }

impl<T> qc::Arbitrary for Iter<T> where T: qc::Arbitrary
{
    fn arbitrary<G: qc::Gen>(g: &mut G) -> Self
    {
        Iter::new(T::arbitrary(g)..T::arbitrary(g))
    }

    fn shrink(&self) -> Box<Iterator<Item=Iter<T>>>
    {
        let r = self.0.clone();
        Box::new(
            r.start.shrink().flat_map(move |x| {
                r.end.shrink().map(move |y| (x.clone(), y))
            })
            .map(|(a, b)| Iter::new(a..b))
        )
    }
}

fn correct_size_hint<I: Iterator>(mut it: I) -> bool {
    // record size hint at each iteration
    let initial_hint = it.size_hint();
    let mut hints = Vec::with_capacity(initial_hint.0 + 1);
    hints.push(initial_hint);
    while let Some(_) = it.next() {
        hints.push(it.size_hint())
    }

    let mut true_count = hints.len(); // start off +1 too much

    // check all the size hints
    for &(low, hi) in &hints {
        true_count -= 1;
        if low > true_count ||
            (hi.is_some() && hi.unwrap() < true_count)
        {
            println!("True size: {:?}, size hint: {:?}", true_count, (low, hi));
            //println!("All hints: {:?}", hints);
            return false
        }
    }
    true
}

fn exact_size<I: ExactSizeIterator>(mut it: I) -> bool {
    // check every iteration
    let (mut low, mut hi) = it.size_hint();
    if Some(low) != hi { return false; }
    while let Some(_) = it.next() {
        let (xlow, xhi) = it.size_hint();
        if low != xlow + 1 { return false; }
        low = xlow;
        hi = xhi;
        if Some(low) != hi { return false; }
    }
    let (low, hi) = it.size_hint();
    low == 0 && hi == Some(0)
}

// Exact size for this case, without ExactSizeIterator
fn exact_size_for_this<I: Iterator>(mut it: I) -> bool {
    // check every iteration
    let (mut low, mut hi) = it.size_hint();
    if Some(low) != hi { return false; }
    while let Some(_) = it.next() {
        let (xlow, xhi) = it.size_hint();
        if low != xlow + 1 { return false; }
        low = xlow;
        hi = xhi;
        if Some(low) != hi { return false; }
    }
    let (low, hi) = it.size_hint();
    low == 0 && hi == Some(0)
}

/*
 * NOTE: Range<i8> is broken!
 * (all signed ranges are)
#[quickcheck]
fn size_range_i8(a: Iter<i8>) -> bool {
    exact_size(a)
}

#[quickcheck]
fn size_range_i16(a: Iter<i16>) -> bool {
    exact_size(a)
}

#[quickcheck]
fn size_range_u8(a: Iter<u8>) -> bool {
    exact_size(a)
}
 */

macro_rules! quickcheck {
    // accept several property function definitions
    // The property functions can use pattern matching and `mut` as usual
    // in the function arguments, but the functions can not be generic.
    {$($(#$attr:tt)* fn $fn_name:ident($($arg:tt)*) -> $ret:ty { $($code:tt)* })*} => (
        quickcheck!{@as_items
        $(
            #[test]
            $(#$attr)*
            fn $fn_name() {
                fn prop($($arg)*) -> $ret {
                    $($code)*
                }
                ::quickcheck::quickcheck(quickcheck!(@fn prop [] $($arg)*));
            }
        )*
        }
    );
    // parse argument list (with patterns allowed) into prop as fn(_, _) -> _
    (@fn $f:ident [$($t:tt)*]) => {
        quickcheck!(@as_expr $f as fn($($t),*) -> _)
    };
    (@fn $f:ident [$($p:tt)*] : $($tail:tt)*) => {
        quickcheck!(@fn $f [$($p)* _] $($tail)*)
    };
    (@fn $f:ident [$($p:tt)*] $t:tt $($tail:tt)*) => {
        quickcheck!(@fn $f [$($p)*] $($tail)*)
    };
    (@as_items $($i:item)*) => ($($i)*);
    (@as_expr $i:expr) => ($i);
}

quickcheck! {

    fn size_product(a: Iter<u16>, b: Iter<u16>) -> bool {
        correct_size_hint(a.cartesian_product(b))
    }
    fn size_product3(a: Iter<u16>, b: Iter<u16>, c: Iter<u16>) -> bool {
        correct_size_hint(iproduct!(a, b, c))
    }

    fn size_step(a: Iter<i16>, s: usize) -> bool {
        let mut s = s;
        if s == 0 {
            s += 1; // never zero
        }
        let filt = a.clone().dedup();
        correct_size_hint(filt.step(s)) &&
            exact_size(a.step(s))
    }
    fn equal_step(a: Iter<i16>, s: usize) -> bool {
        let mut s = s;
        if s == 0 {
            s += 1; // never zero
        }
        let mut i = 0;
        itertools::equal(a.clone().step(s), a.filter(|_| {
            let keep = i % s == 0;
            i += 1;
            keep
        }))
    }
    fn equal_step_vec(a: Vec<i16>, s: usize) -> bool {
        let mut s = s;
        if s == 0 {
            s += 1; // never zero
        }
        let mut i = 0;
        itertools::equal(a.iter().step(s), a.iter().filter(|_| {
            let keep = i % s == 0;
            i += 1;
            keep
        }))
    }

    fn size_multipeek(a: Iter<u16>, s: u8) -> bool {
        let mut it = multipeek(a);
        // peek a few times
        for _ in 0..s {
            it.peek();
        }
        exact_size(it)
    }

    fn equal_merge(a: Vec<i16>, b: Vec<i16>) -> bool {
        let mut sa = a.clone();
        let mut sb = b.clone();
        sa.sort();
        sb.sort();
        let mut merged = sa.clone();
        merged.extend(sb.iter().cloned());
        merged.sort();
        itertools::equal(&merged, sa.iter().merge(&sb))
    }
    fn size_merge(a: Iter<u16>, b: Iter<u16>) -> bool {
        correct_size_hint(a.merge(b))
    }
    fn size_zip(a: Iter<i16>, b: Iter<i16>, c: Iter<i16>) -> bool {
        let filt = a.clone().dedup();
        correct_size_hint(multizip((filt, b.clone(), c.clone()))) &&
            exact_size(multizip((a, b, c)))
    }
    fn size_zip_rc(a: Iter<i16>, b: Iter<i16>) -> bool {
        let rc = rciter(a.clone());
        correct_size_hint(multizip((&rc, &rc, b)))
    }

    fn equal_kmerge(a: Vec<i16>, b: Vec<i16>, c: Vec<i16>) -> bool {
        use itertools::free::kmerge;
        let mut sa = a.clone();
        let mut sb = b.clone();
        let mut sc = c.clone();
        sa.sort();
        sb.sort();
        sc.sort();
        let mut merged = sa.clone();
        merged.extend(sb.iter().cloned());
        merged.extend(sc.iter().cloned());
        merged.sort();
        itertools::equal(merged.into_iter(), kmerge(vec![sa, sb, sc]))
    }

    // Any number of input iterators
    fn equal_kmerge_2(mut inputs: Vec<Vec<i16>>) -> bool {
        use itertools::free::kmerge;
        // sort the inputs
        for input in &mut inputs {
            input.sort();
        }
        let mut merged = inputs.concat();
        merged.sort();
        itertools::equal(merged.into_iter(), kmerge(inputs))
    }

    // Any number of input iterators
    fn equal_kmerge_by_ge(mut inputs: Vec<Vec<i16>>) -> bool {
        // sort the inputs
        for input in &mut inputs {
            input.sort();
            input.reverse();
        }
        let mut merged = inputs.concat();
        merged.sort();
        merged.reverse();
        itertools::equal(merged.into_iter(),
                         inputs.into_iter().kmerge_by(|x, y| x >= y))
    }

    // Any number of input iterators
    fn equal_kmerge_by_lt(mut inputs: Vec<Vec<i16>>) -> bool {
        // sort the inputs
        for input in &mut inputs {
            input.sort();
        }
        let mut merged = inputs.concat();
        merged.sort();
        itertools::equal(merged.into_iter(),
                         inputs.into_iter().kmerge_by(|x, y| x < y))
    }

    // Any number of input iterators
    fn equal_kmerge_by_le(mut inputs: Vec<Vec<i16>>) -> bool {
        // sort the inputs
        for input in &mut inputs {
            input.sort();
        }
        let mut merged = inputs.concat();
        merged.sort();
        itertools::equal(merged.into_iter(),
                         inputs.into_iter().kmerge_by(|x, y| x <= y))
    }
    fn size_kmerge(a: Iter<i16>, b: Iter<i16>, c: Iter<i16>) -> bool {
        use itertools::free::kmerge;
        correct_size_hint(kmerge(vec![a, b, c]))
    }
    fn equal_zip_eq(a: Vec<i32>, b: Vec<i32>) -> bool {
        let len = std::cmp::min(a.len(), b.len());
        let a = &a[..len];
        let b = &b[..len];
        itertools::equal(zip_eq(a, b), zip(a, b))
    }
    fn size_zip_longest(a: Iter<i16>, b: Iter<i16>) -> bool {
        let filt = a.clone().dedup();
        let filt2 = b.clone().dedup();
        correct_size_hint(filt.zip_longest(b.clone())) &&
        correct_size_hint(a.clone().zip_longest(filt2)) &&
            exact_size(a.zip_longest(b))
    }
    fn size_2_zip_longest(a: Iter<i16>, b: Iter<i16>) -> bool {
        let it = a.clone().zip_longest(b.clone());
        let jt = a.clone().zip_longest(b.clone());
        itertools::equal(a.clone(),
                         it.filter_map(|elt| match elt {
                             EitherOrBoth::Both(x, _) => Some(x),
                             EitherOrBoth::Left(x) => Some(x),
                             _ => None,
                         }
                         ))
            &&
        itertools::equal(b.clone(),
                         jt.filter_map(|elt| match elt {
                             EitherOrBoth::Both(_, y) => Some(y),
                             EitherOrBoth::Right(y) => Some(y),
                             _ => None,
                         }
                         ))
    }
    fn size_interleave(a: Iter<i16>, b: Iter<i16>) -> bool {
        correct_size_hint(a.interleave(b))
    }
    fn exact_interleave(a: Iter<i16>, b: Iter<i16>) -> bool {
        exact_size_for_this(a.interleave(b))
    }
    fn size_interleave_shortest(a: Iter<i16>, b: Iter<i16>) -> bool {
        correct_size_hint(a.interleave_shortest(b))
    }
    fn exact_interleave_shortest(a: Vec<()>, b: Vec<()>) -> bool {
        exact_size_for_this(a.iter().interleave_shortest(&b))
    }
    fn size_intersperse(a: Iter<i16>, x: i16) -> bool {
        correct_size_hint(a.intersperse(x))
    }
    fn equal_intersperse(a: Vec<i32>, x: i32) -> bool {
        let mut inter = false;
        let mut i = 0;
        for elt in a.iter().cloned().intersperse(x) {
            if inter {
                if elt != x { return false }
            } else {
                if elt != a[i] { return false }
                i += 1;
            }
            inter = !inter;
        }
        true
    }

    fn equal_flatten(a: Vec<Option<i32>>) -> bool {
        itertools::equal(a.iter().flatten(),
                         a.iter().filter_map(|x| x.as_ref()))
    }

    fn equal_flatten_vec(a: Vec<Vec<u8>>) -> bool {
        itertools::equal(a.iter().flatten(),
                         a.iter().flat_map(|x| x))
    }
    fn equal_flatten_vec_rev(a: Vec<Vec<u8>>) -> bool {
        itertools::equal(a.iter().flatten().rev(),
                         a.iter().flat_map(|x| x).rev())
    }

    fn equal_combinations_2(a: Vec<u8>) -> bool {
        let mut v = Vec::new();
        for (i, &x) in enumerate(&a) {
            for &y in &a[i + 1..] {
                v.push((x, y));
            }
        }
        itertools::equal(cloned(&a).tuple_combinations::<(_, _)>(), cloned(&v))
    }
}

quickcheck! {
    fn equal_dedup(a: Vec<i32>) -> bool {
        let mut b = a.clone();
        b.dedup();
        itertools::equal(&b, a.iter().dedup())
    }
}

quickcheck! {
    fn size_dedup(a: Vec<i32>) -> bool {
        correct_size_hint(a.iter().dedup())
    }
}

quickcheck! {
    fn exact_repeatn((n, x): (usize, i32)) -> bool {
        let it = itertools::repeat_n(x, n);
        exact_size(it)
    }
}

quickcheck! {
    fn size_put_back(a: Vec<u8>, x: Option<u8>) -> bool {
        let mut it = put_back(a.into_iter());
        match x {
            Some(t) => it.put_back(t),
            None => {}
        }
        correct_size_hint(it)
    }
}

quickcheck! {
    fn size_put_backn(a: Vec<u8>, b: Vec<u8>) -> bool {
        let mut it = put_back_n(a.into_iter());
        for elt in b {
            it.put_back(elt)
        }
        correct_size_hint(it)
    }
}

quickcheck! {
    fn size_tee(a: Vec<u8>) -> bool {
        let (mut t1, mut t2) = a.iter().tee();
        t1.next();
        t1.next();
        t2.next();
        exact_size(t1) && exact_size(t2)
    }
}

quickcheck! {
    fn size_tee_2(a: Vec<u8>) -> bool {
        let (mut t1, mut t2) = a.iter().dedup().tee();
        t1.next();
        t1.next();
        t2.next();
        correct_size_hint(t1) && correct_size_hint(t2)
    }
}

quickcheck! {
    fn size_take_while_ref(a: Vec<u8>, stop: u8) -> bool {
        correct_size_hint(a.iter().take_while_ref(|x| **x != stop))
    }
}

quickcheck! {
    fn equal_partition(a: Vec<i32>) -> bool {
        let mut a = a;
        let mut ap = a.clone();
        let split_index = itertools::partition(&mut ap, |x| *x >= 0);
        let parted = (0..split_index).all(|i| ap[i] >= 0) &&
            (split_index..a.len()).all(|i| ap[i] < 0);

        a.sort();
        ap.sort();
        parted && (a == ap)
    }
}

quickcheck! {
    fn size_combinations(it: Iter<i16>) -> bool {
        correct_size_hint(it.tuple_combinations::<(_, _)>())
    }
}

quickcheck! {
    fn equal_combinations(it: Iter<i16>) -> bool {
        let values = it.clone().collect_vec();
        let mut cmb = it.tuple_combinations();
        for i in 0..values.len() {
            for j in i+1..values.len() {
                let pair = (values[i], values[j]);
                if pair != cmb.next().unwrap() {
                    return false;
                }
            }
        }
        cmb.next() == None
    }
}

quickcheck! {
    fn size_pad_tail(it: Iter<i8>, pad: u8) -> bool {
        correct_size_hint(it.clone().pad_using(pad as usize, |_| 0)) &&
            correct_size_hint(it.dropping(1).rev().pad_using(pad as usize, |_| 0))
    }
}

quickcheck! {
    fn size_pad_tail2(it: Iter<i8>, pad: u8) -> bool {
        exact_size(it.pad_using(pad as usize, |_| 0))
    }
}

quickcheck! {
    fn size_unique(it: Iter<i8>) -> bool {
        correct_size_hint(it.unique())
    }
}

quickcheck! {
    fn fuzz_group_by_lazy_1(it: Iter<u8>) -> bool {
        let jt = it.clone();
        let groups = it.group_by(|k| *k);
        let res = itertools::equal(jt, groups.into_iter().flat_map(|(_, x)| x));
        res
    }
}

quickcheck! {
    fn fuzz_group_by_lazy_2(data: Vec<u8>) -> bool {
        let groups = data.iter().group_by(|k| *k / 10);
        let res = itertools::equal(data.iter(), groups.into_iter().flat_map(|(_, x)| x));
        res
    }
}

quickcheck! {
    fn fuzz_group_by_lazy_3(data: Vec<u8>) -> bool {
        let grouper = data.iter().group_by(|k| *k / 10);
        let groups = grouper.into_iter().collect_vec();
        let res = itertools::equal(data.iter(), groups.into_iter().flat_map(|(_, x)| x));
        res
    }
}

quickcheck! {
    fn fuzz_group_by_lazy_duo(data: Vec<u8>, order: Vec<(bool, bool)>) -> bool {
        let grouper = data.iter().group_by(|k| *k / 3);
        let mut groups1 = grouper.into_iter();
        let mut groups2 = grouper.into_iter();
        let mut elts = Vec::<&u8>::new();
        let mut old_groups = Vec::new();

        let tup1 = |(_, b)| b;
        for &(ord, consume_now) in &order {
            let iter = &mut [&mut groups1, &mut groups2][ord as usize];
            match iter.next() {
                Some((_, gr)) => if consume_now {
                    for og in old_groups.drain(..) {
                        elts.extend(og);
                    }
                    elts.extend(gr);
                } else {
                    old_groups.push(gr);
                },
                None => break,
            }
        }
        for og in old_groups.drain(..) {
            elts.extend(og);
        }
        for gr in groups1.map(&tup1) { elts.extend(gr); }
        for gr in groups2.map(&tup1) { elts.extend(gr); }
        itertools::assert_equal(&data, elts);
        true
    }
}

quickcheck! {
    fn equal_chunks_lazy(a: Vec<u8>, size: u8) -> bool {
        let mut size = size;
        if size == 0 {
            size += 1;
        }
        let chunks = a.iter().chunks(size as usize);
        let it = a.chunks(size as usize);
        for (a, b) in chunks.into_iter().zip(it) {
            if !itertools::equal(a, b) {
                return false;
            }
        }
        true
    }
}

quickcheck! {
    fn equal_tuple_windows_1(a: Vec<u8>) -> bool {
        let x = a.windows(1).map(|s| (&s[0], ));
        let y = a.iter().tuple_windows::<(_,)>();
        itertools::equal(x, y)
    }

    fn equal_tuple_windows_2(a: Vec<u8>) -> bool {
        let x = a.windows(2).map(|s| (&s[0], &s[1]));
        let y = a.iter().tuple_windows::<(_, _)>();
        itertools::equal(x, y)
    }

    fn equal_tuple_windows_3(a: Vec<u8>) -> bool {
        let x = a.windows(3).map(|s| (&s[0], &s[1], &s[2]));
        let y = a.iter().tuple_windows::<(_, _, _)>();
        itertools::equal(x, y)
    }

    fn equal_tuple_windows_4(a: Vec<u8>) -> bool {
        let x = a.windows(4).map(|s| (&s[0], &s[1], &s[2], &s[3]));
        let y = a.iter().tuple_windows::<(_, _, _, _)>();
        itertools::equal(x, y)
    }

    fn equal_tuples_1(a: Vec<u8>) -> bool {
        let x = a.chunks(1).map(|s| (&s[0], ));
        let y = a.iter().tuples::<(_,)>();
        itertools::equal(x, y)
    }

    fn equal_tuples_2(a: Vec<u8>) -> bool {
        let x = a.chunks(2).filter(|s| s.len() == 2).map(|s| (&s[0], &s[1]));
        let y = a.iter().tuples::<(_, _)>();
        itertools::equal(x, y)
    }

    fn equal_tuples_3(a: Vec<u8>) -> bool {
        let x = a.chunks(3).filter(|s| s.len() == 3).map(|s| (&s[0], &s[1], &s[2]));
        let y = a.iter().tuples::<(_, _, _)>();
        itertools::equal(x, y)
    }

    fn equal_tuples_4(a: Vec<u8>) -> bool {
        let x = a.chunks(4).filter(|s| s.len() == 4).map(|s| (&s[0], &s[1], &s[2], &s[3]));
        let y = a.iter().tuples::<(_, _, _, _)>();
        itertools::equal(x, y)
    }

    fn exact_tuple_buffer(a: Vec<u8>) -> bool {
        let mut iter = a.iter().tuples::<(_, _, _, _)>();
        (&mut iter).last();
        let buffer = iter.into_buffer();
        assert_eq!(buffer.len(), a.len() % 4);
        exact_size(buffer)
    }
}

// with_position
quickcheck! {
    fn with_position_exact_size_1(a: Vec<u8>) -> bool {
        exact_size_for_this(a.iter().with_position())
    }
    fn with_position_exact_size_2(a: Iter<u8>) -> bool {
        exact_size_for_this(a.with_position())
    }
}

/// A peculiar type: Equality compares both tuple items, but ordering only the
/// first item.  This is so we can check the stability property easily.
#[derive(Clone, Debug, PartialEq, Eq)]
struct Val(u32, u32);

impl PartialOrd<Val> for Val {
    fn partial_cmp(&self, other: &Val) -> Option<Ordering> {
        self.0.partial_cmp(&other.0)
    }
}

impl Ord for Val {
    fn cmp(&self, other: &Val) -> Ordering {
        self.0.cmp(&other.0)
    }
}

impl qc::Arbitrary for Val {
    fn arbitrary<G: qc::Gen>(g: &mut G) -> Self {
        let (x, y) = <(u32, u32)>::arbitrary(g);
        Val(x, y)
    }
    fn shrink(&self) -> Box<Iterator<Item = Self>> {
        Box::new((self.0, self.1).shrink().map(|(x, y)| Val(x, y)))
    }
}

quickcheck! {
    fn minmax(a: Vec<Val>) -> bool {
        use itertools::MinMaxResult;


        let minmax = a.iter().minmax();
        let expected = match a.len() {
            0 => MinMaxResult::NoElements,
            1 => MinMaxResult::OneElement(&a[0]),
            _ => MinMaxResult::MinMax(a.iter().min().unwrap(),
                                      a.iter().max().unwrap()),
        };
        minmax == expected
    }
}

quickcheck! {
    fn minmax_f64(a: Vec<f64>) -> TestResult {
        use itertools::MinMaxResult;

        if a.iter().any(|x| x.is_nan()) {
            return TestResult::discard();
        }

        let min = cloned(&a).fold1(f64::min);
        let max = cloned(&a).fold1(f64::max);

        let minmax = cloned(&a).minmax();
        let expected = match a.len() {
            0 => MinMaxResult::NoElements,
            1 => MinMaxResult::OneElement(min.unwrap()),
            _ => MinMaxResult::MinMax(min.unwrap(), max.unwrap()),
        };
        TestResult::from_bool(minmax == expected)
    }
}
