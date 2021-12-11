use paste;
use permutohedron;
use quickcheck as qc;
use rand::{distributions::{Distribution, Standard}, Rng, SeedableRng, rngs::StdRng};
use rand::{seq::SliceRandom, thread_rng};
use std::{cmp::min, fmt::Debug, marker::PhantomData};
use itertools as it;
use crate::it::Itertools;
use crate::it::ExactlyOneError;
use crate::it::multizip;
use crate::it::multipeek;
use crate::it::peek_nth;
use crate::it::free::rciter;
use crate::it::free::put_back_n;
use crate::it::FoldWhile;
use crate::it::cloned;
use crate::it::iproduct;
use crate::it::izip;

#[test]
fn product3() {
    let prod = iproduct!(0..3, 0..2, 0..2);
    assert_eq!(prod.size_hint(), (12, Some(12)));
    let v = prod.collect_vec();
    for i in 0..3 {
        for j in 0..2 {
            for k in 0..2 {
                assert!((i, j, k) == v[(i * 2 * 2 + j * 2 + k) as usize]);
            }
        }
    }
    for (_, _, _, _) in iproduct!(0..3, 0..2, 0..2, 0..3) {
        /* test compiles */
    }
}

#[test]
fn interleave_shortest() {
    let v0: Vec<i32> = vec![0, 2, 4];
    let v1: Vec<i32> = vec![1, 3, 5, 7];
    let it = v0.into_iter().interleave_shortest(v1.into_iter());
    assert_eq!(it.size_hint(), (6, Some(6)));
    assert_eq!(it.collect_vec(), vec![0, 1, 2, 3, 4, 5]);

    let v0: Vec<i32> = vec![0, 2, 4, 6, 8];
    let v1: Vec<i32> = vec![1, 3, 5];
    let it = v0.into_iter().interleave_shortest(v1.into_iter());
    assert_eq!(it.size_hint(), (7, Some(7)));
    assert_eq!(it.collect_vec(), vec![0, 1, 2, 3, 4, 5, 6]);

    let i0 = ::std::iter::repeat(0);
    let v1: Vec<_> = vec![1, 3, 5];
    let it = i0.interleave_shortest(v1.into_iter());
    assert_eq!(it.size_hint(), (7, Some(7)));

    let v0: Vec<_> = vec![0, 2, 4];
    let i1 = ::std::iter::repeat(1);
    let it = v0.into_iter().interleave_shortest(i1);
    assert_eq!(it.size_hint(), (6, Some(6)));
}

#[test]
fn duplicates_by() {
    let xs = ["aaa", "bbbbb", "aa", "ccc", "bbbb", "aaaaa", "cccc"];
    let ys = ["aa", "bbbb", "cccc"];
    it::assert_equal(ys.iter(), xs.iter().duplicates_by(|x| x[..2].to_string()));
    it::assert_equal(ys.iter(), xs.iter().rev().duplicates_by(|x| x[..2].to_string()).rev());
    let ys_rev = ["ccc", "aa", "bbbbb"];
    it::assert_equal(ys_rev.iter(), xs.iter().duplicates_by(|x| x[..2].to_string()).rev());
}

#[test]
fn duplicates() {
    let xs = [0, 1, 2, 3, 2, 1, 3];
    let ys = [2, 1, 3];
    it::assert_equal(ys.iter(), xs.iter().duplicates());
    it::assert_equal(ys.iter(), xs.iter().rev().duplicates().rev());
    let ys_rev = [3, 2, 1];
    it::assert_equal(ys_rev.iter(), xs.iter().duplicates().rev());

    let xs = [0, 1, 0, 1];
    let ys = [0, 1];
    it::assert_equal(ys.iter(), xs.iter().duplicates());
    it::assert_equal(ys.iter(), xs.iter().rev().duplicates().rev());
    let ys_rev = [1, 0];
    it::assert_equal(ys_rev.iter(), xs.iter().duplicates().rev());
}

#[test]
fn unique_by() {
    let xs = ["aaa", "bbbbb", "aa", "ccc", "bbbb", "aaaaa", "cccc"];
    let ys = ["aaa", "bbbbb", "ccc"];
    it::assert_equal(ys.iter(), xs.iter().unique_by(|x| x[..2].to_string()));
    it::assert_equal(ys.iter(), xs.iter().rev().unique_by(|x| x[..2].to_string()).rev());
    let ys_rev = ["cccc", "aaaaa", "bbbb"];
    it::assert_equal(ys_rev.iter(), xs.iter().unique_by(|x| x[..2].to_string()).rev());
}

#[test]
fn unique() {
    let xs = [0, 1, 2, 3, 2, 1, 3];
    let ys = [0, 1, 2, 3];
    it::assert_equal(ys.iter(), xs.iter().unique());
    it::assert_equal(ys.iter(), xs.iter().rev().unique().rev());
    let ys_rev = [3, 1, 2, 0];
    it::assert_equal(ys_rev.iter(), xs.iter().unique().rev());

    let xs = [0, 1];
    let ys = [0, 1];
    it::assert_equal(ys.iter(), xs.iter().unique());
    it::assert_equal(ys.iter(), xs.iter().rev().unique().rev());
    let ys_rev = [1, 0];
    it::assert_equal(ys_rev.iter(), xs.iter().unique().rev());
}

#[test]
fn intersperse() {
    let xs = ["a", "", "b", "c"];
    let v: Vec<&str> = xs.iter().map(|x| x.clone()).intersperse(", ").collect();
    let text: String = v.concat();
    assert_eq!(text, "a, , b, c".to_string());

    let ys = [0, 1, 2, 3];
    let mut it = ys[..0].iter().map(|x| *x).intersperse(1);
    assert!(it.next() == None);
}

#[test]
fn dedup() {
    let xs = [0, 1, 1, 1, 2, 1, 3, 3];
    let ys = [0, 1, 2, 1, 3];
    it::assert_equal(ys.iter(), xs.iter().dedup());
    let xs = [0, 0, 0, 0, 0];
    let ys = [0];
    it::assert_equal(ys.iter(), xs.iter().dedup());

    let xs = [0, 1, 1, 1, 2, 1, 3, 3];
    let ys = [0, 1, 2, 1, 3];
    let mut xs_d = Vec::new();
    xs.iter().dedup().fold((), |(), &elt| xs_d.push(elt));
    assert_eq!(&xs_d, &ys);
}

#[test]
fn coalesce() {
    let data = vec![-1., -2., -3., 3., 1., 0., -1.];
    let it = data.iter().cloned().coalesce(|x, y|
        if (x >= 0.) == (y >= 0.) {
            Ok(x + y)
        } else {
            Err((x, y))
        }
    );
    itertools::assert_equal(it.clone(), vec![-6., 4., -1.]);
    assert_eq!(
        it.fold(vec![], |mut v, n| {
            v.push(n);
            v
        }),
        vec![-6., 4., -1.]
    );
}

#[test]
fn dedup_by() {
    let xs = [(0, 0), (0, 1), (1, 1), (2, 1), (0, 2), (3, 1), (0, 3), (1, 3)];
    let ys = [(0, 0), (0, 1), (0, 2), (3, 1), (0, 3)];
    it::assert_equal(ys.iter(), xs.iter().dedup_by(|x, y| x.1==y.1));
    let xs = [(0, 1), (0, 2), (0, 3), (0, 4), (0, 5)];
    let ys = [(0, 1)];
    it::assert_equal(ys.iter(), xs.iter().dedup_by(|x, y| x.0==y.0));

    let xs = [(0, 0), (0, 1), (1, 1), (2, 1), (0, 2), (3, 1), (0, 3), (1, 3)];
    let ys = [(0, 0), (0, 1), (0, 2), (3, 1), (0, 3)];
    let mut xs_d = Vec::new();
    xs.iter().dedup_by(|x, y| x.1==y.1).fold((), |(), &elt| xs_d.push(elt));
    assert_eq!(&xs_d, &ys);
}

#[test]
fn dedup_with_count() {
    let xs: [i32; 8] = [0, 1, 1, 1, 2, 1, 3, 3];
    let ys: [(usize, &i32); 5] = [(1, &0), (3, &1), (1, &2), (1, &1), (2, &3)];

    it::assert_equal(ys.iter().cloned(), xs.iter().dedup_with_count());

    let xs: [i32; 5] = [0, 0, 0, 0, 0];
    let ys: [(usize, &i32); 1] = [(5, &0)];

    it::assert_equal(ys.iter().cloned(), xs.iter().dedup_with_count());
}


#[test]
fn dedup_by_with_count() {
    let xs = [(0, 0), (0, 1), (1, 1), (2, 1), (0, 2), (3, 1), (0, 3), (1, 3)];
    let ys = [(1, &(0, 0)), (3, &(0, 1)), (1, &(0, 2)), (1, &(3, 1)), (2, &(0, 3))];

    it::assert_equal(ys.iter().cloned(), xs.iter().dedup_by_with_count(|x, y| x.1==y.1));

    let xs = [(0, 1), (0, 2), (0, 3), (0, 4), (0, 5)];
    let ys = [( 5, &(0, 1))];

    it::assert_equal(ys.iter().cloned(), xs.iter().dedup_by_with_count(|x, y| x.0==y.0));
}

#[test]
fn all_equal() {
    assert!("".chars().all_equal());
    assert!("A".chars().all_equal());
    assert!(!"AABBCCC".chars().all_equal());
    assert!("AAAAAAA".chars().all_equal());
    for (_key, mut sub) in &"AABBCCC".chars().group_by(|&x| x) {
        assert!(sub.all_equal());
    }
}

#[test]
fn all_unique() {
    assert!("ABCDEFGH".chars().all_unique());
    assert!(!"ABCDEFGA".chars().all_unique());
    assert!(::std::iter::empty::<usize>().all_unique());
}

#[test]
fn test_put_back_n() {
    let xs = [0, 1, 1, 1, 2, 1, 3, 3];
    let mut pb = put_back_n(xs.iter().cloned());
    pb.next();
    pb.next();
    pb.put_back(1);
    pb.put_back(0);
    it::assert_equal(pb, xs.iter().cloned());
}

#[test]
fn tee() {
    let xs  = [0, 1, 2, 3];
    let (mut t1, mut t2) = xs.iter().cloned().tee();
    assert_eq!(t1.next(), Some(0));
    assert_eq!(t2.next(), Some(0));
    assert_eq!(t1.next(), Some(1));
    assert_eq!(t1.next(), Some(2));
    assert_eq!(t1.next(), Some(3));
    assert_eq!(t1.next(), None);
    assert_eq!(t2.next(), Some(1));
    assert_eq!(t2.next(), Some(2));
    assert_eq!(t1.next(), None);
    assert_eq!(t2.next(), Some(3));
    assert_eq!(t2.next(), None);
    assert_eq!(t1.next(), None);
    assert_eq!(t2.next(), None);

    let (t1, t2) = xs.iter().cloned().tee();
    it::assert_equal(t1, xs.iter().cloned());
    it::assert_equal(t2, xs.iter().cloned());

    let (t1, t2) = xs.iter().cloned().tee();
    it::assert_equal(t1.zip(t2), xs.iter().cloned().zip(xs.iter().cloned()));
}


#[test]
fn test_rciter() {
    let xs = [0, 1, 1, 1, 2, 1, 3, 5, 6];

    let mut r1 = rciter(xs.iter().cloned());
    let mut r2 = r1.clone();
    assert_eq!(r1.next(), Some(0));
    assert_eq!(r2.next(), Some(1));
    let mut z = r1.zip(r2);
    assert_eq!(z.next(), Some((1, 1)));
    assert_eq!(z.next(), Some((2, 1)));
    assert_eq!(z.next(), Some((3, 5)));
    assert_eq!(z.next(), None);

    // test intoiterator
    let r1 = rciter(0..5);
    let mut z = izip!(&r1, r1);
    assert_eq!(z.next(), Some((0, 1)));
}

#[allow(deprecated)]
#[test]
fn trait_pointers() {
    struct ByRef<'r, I: ?Sized>(&'r mut I) ;

    impl<'r, X, I: ?Sized> Iterator for ByRef<'r, I> where
        I: 'r + Iterator<Item=X>
    {
        type Item = X;
        fn next(&mut self) -> Option<Self::Item>
        {
            self.0.next()
        }
    }

    let mut it = Box::new(0..10) as Box<dyn Iterator<Item=i32>>;
    assert_eq!(it.next(), Some(0));

    {
        /* make sure foreach works on non-Sized */
        let jt: &mut dyn Iterator<Item = i32> = &mut *it;
        assert_eq!(jt.next(), Some(1));

        {
            let mut r = ByRef(jt);
            assert_eq!(r.next(), Some(2));
        }

        assert_eq!(jt.find_position(|x| *x == 4), Some((1, 4)));
        jt.foreach(|_| ());
    }
}

#[test]
fn merge_by() {
    let odd : Vec<(u32, &str)> = vec![(1, "hello"), (3, "world"), (5, "!")];
    let even = vec![(2, "foo"), (4, "bar"), (6, "baz")];
    let expected = vec![(1, "hello"), (2, "foo"), (3, "world"), (4, "bar"), (5, "!"), (6, "baz")];
    let results = odd.iter().merge_by(even.iter(), |a, b| a.0 <= b.0);
    it::assert_equal(results, expected.iter());
}

#[test]
fn merge_by_btree() {
    use std::collections::BTreeMap;
    let mut bt1 = BTreeMap::new();
    bt1.insert("hello", 1);
    bt1.insert("world", 3);
    let mut bt2 = BTreeMap::new();
    bt2.insert("foo", 2);
    bt2.insert("bar", 4);
    let results = bt1.into_iter().merge_by(bt2.into_iter(), |a, b| a.0 <= b.0 );
    let expected = vec![("bar", 4), ("foo", 2), ("hello", 1), ("world", 3)];
    it::assert_equal(results, expected.into_iter());
}

#[allow(deprecated)]
#[test]
fn kmerge() {
    let its = (0..4).map(|s| (s..10).step(4));

    it::assert_equal(its.kmerge(), 0..10);
}

#[allow(deprecated)]
#[test]
fn kmerge_2() {
    let its = vec![3, 2, 1, 0].into_iter().map(|s| (s..10).step(4));

    it::assert_equal(its.kmerge(), 0..10);
}

#[test]
fn kmerge_empty() {
    let its = (0..4).map(|_| 0..0);
    assert_eq!(its.kmerge().next(), None);
}

#[test]
fn kmerge_size_hint() {
    let its = (0..5).map(|_| (0..10));
    assert_eq!(its.kmerge().size_hint(), (50, Some(50)));
}

#[test]
fn kmerge_empty_size_hint() {
    let its = (0..5).map(|_| (0..0));
    assert_eq!(its.kmerge().size_hint(), (0, Some(0)));
}

#[test]
fn join() {
    let many = [1, 2, 3];
    let one  = [1];
    let none: Vec<i32> = vec![];

    assert_eq!(many.iter().join(", "), "1, 2, 3");
    assert_eq!( one.iter().join(", "), "1");
    assert_eq!(none.iter().join(", "), "");
}

#[test]
fn sorted_unstable_by() {
    let sc = [3, 4, 1, 2].iter().cloned().sorted_by(|&a, &b| {
        a.cmp(&b)
    });
    it::assert_equal(sc, vec![1, 2, 3, 4]);

    let v = (0..5).sorted_unstable_by(|&a, &b| a.cmp(&b).reverse());
    it::assert_equal(v, vec![4, 3, 2, 1, 0]);
}

#[test]
fn sorted_unstable_by_key() {
    let sc = [3, 4, 1, 2].iter().cloned().sorted_unstable_by_key(|&x| x);
    it::assert_equal(sc, vec![1, 2, 3, 4]);

    let v = (0..5).sorted_unstable_by_key(|&x| -x);
    it::assert_equal(v, vec![4, 3, 2, 1, 0]);
}

#[test]
fn sorted_by() {
    let sc = [3, 4, 1, 2].iter().cloned().sorted_by(|&a, &b| {
        a.cmp(&b)
    });
    it::assert_equal(sc, vec![1, 2, 3, 4]);

    let v = (0..5).sorted_by(|&a, &b| a.cmp(&b).reverse());
    it::assert_equal(v, vec![4, 3, 2, 1, 0]);
}

qc::quickcheck! {
    fn k_smallest_range(n: u64, m: u16, k: u16) -> () {
        // u16 is used to constrain k and m to 0..2¹⁶,
        //  otherwise the test could use too much memory.
        let (k, m) = (k as u64, m as u64);

        // Generate a random permutation of n..n+m
        let i = {
            let mut v: Vec<u64> = (n..n.saturating_add(m)).collect();
            v.shuffle(&mut thread_rng());
            v.into_iter()
        };

        // Check that taking the k smallest elements yields n..n+min(k, m)
        it::assert_equal(
            i.k_smallest(k as usize),
            n..n.saturating_add(min(k, m))
        );
    }
}

#[derive(Clone, Debug)]
struct RandIter<T: 'static + Clone + Send, R: 'static + Clone + Rng + SeedableRng + Send = StdRng> {
    idx: usize,
    len: usize,
    rng: R,
    _t: PhantomData<T>
}

impl<T: Clone + Send, R: Clone + Rng + SeedableRng + Send> Iterator for RandIter<T, R>
where Standard: Distribution<T> {
    type Item = T;
    fn next(&mut self) -> Option<T> {
        if self.idx == self.len {
            None
        } else {
            self.idx += 1;
            Some(self.rng.gen())
        }
    }
}

impl<T: Clone + Send, R: Clone + Rng + SeedableRng + Send> qc::Arbitrary for RandIter<T, R> {
    fn arbitrary<G: qc::Gen>(g: &mut G) -> Self {
        RandIter {
            idx: 0,
            len: g.size(),
            rng: R::seed_from_u64(g.next_u64()),
            _t : PhantomData{},
        }
    }
}

// Check that taking the k smallest is the same as
//  sorting then taking the k first elements
fn k_smallest_sort<I>(i: I, k: u16) -> ()
where
    I: Iterator + Clone,
    I::Item: Ord + Debug,
{
    let j = i.clone();
    let k = k as usize;
    it::assert_equal(
        i.k_smallest(k),
        j.sorted().take(k)
    )
}

macro_rules! generic_test {
    ($f:ident, $($t:ty),+) => {
        $(paste::item! {
            qc::quickcheck! {
                fn [< $f _ $t >](i: RandIter<$t>, k: u16) -> () {
                    $f(i, k)
                }
            }
        })+
    };
}

generic_test!(k_smallest_sort, u8, u16, u32, u64, i8, i16, i32, i64);

#[test]
fn sorted_by_key() {
    let sc = [3, 4, 1, 2].iter().cloned().sorted_by_key(|&x| x);
    it::assert_equal(sc, vec![1, 2, 3, 4]);

    let v = (0..5).sorted_by_key(|&x| -x);
    it::assert_equal(v, vec![4, 3, 2, 1, 0]);
}

#[test]
fn test_multipeek() {
    let nums = vec![1u8,2,3,4,5];

    let mp = multipeek(nums.iter().map(|&x| x));
    assert_eq!(nums, mp.collect::<Vec<_>>());

    let mut mp = multipeek(nums.iter().map(|&x| x));
    assert_eq!(mp.peek(), Some(&1));
    assert_eq!(mp.next(), Some(1));
    assert_eq!(mp.peek(), Some(&2));
    assert_eq!(mp.peek(), Some(&3));
    assert_eq!(mp.next(), Some(2));
    assert_eq!(mp.peek(), Some(&3));
    assert_eq!(mp.peek(), Some(&4));
    assert_eq!(mp.peek(), Some(&5));
    assert_eq!(mp.peek(), None);
    assert_eq!(mp.next(), Some(3));
    assert_eq!(mp.next(), Some(4));
    assert_eq!(mp.peek(), Some(&5));
    assert_eq!(mp.peek(), None);
    assert_eq!(mp.next(), Some(5));
    assert_eq!(mp.next(), None);
    assert_eq!(mp.peek(), None);
}

#[test]
fn test_multipeek_reset() {
    let data = [1, 2, 3, 4];

    let mut mp = multipeek(cloned(&data));
    assert_eq!(mp.peek(), Some(&1));
    assert_eq!(mp.next(), Some(1));
    assert_eq!(mp.peek(), Some(&2));
    assert_eq!(mp.peek(), Some(&3));
    mp.reset_peek();
    assert_eq!(mp.peek(), Some(&2));
    assert_eq!(mp.next(), Some(2));
}

#[test]
fn test_multipeek_peeking_next() {
    use crate::it::PeekingNext;
    let nums = vec![1u8,2,3,4,5,6,7];

    let mut mp = multipeek(nums.iter().map(|&x| x));
    assert_eq!(mp.peeking_next(|&x| x != 0), Some(1));
    assert_eq!(mp.next(), Some(2));
    assert_eq!(mp.peek(), Some(&3));
    assert_eq!(mp.peek(), Some(&4));
    assert_eq!(mp.peeking_next(|&x| x == 3), Some(3));
    assert_eq!(mp.peek(), Some(&4));
    assert_eq!(mp.peeking_next(|&x| x != 4), None);
    assert_eq!(mp.peeking_next(|&x| x == 4), Some(4));
    assert_eq!(mp.peek(), Some(&5));
    assert_eq!(mp.peek(), Some(&6));
    assert_eq!(mp.peeking_next(|&x| x != 5), None);
    assert_eq!(mp.peek(), Some(&7));
    assert_eq!(mp.peeking_next(|&x| x == 5), Some(5));
    assert_eq!(mp.peeking_next(|&x| x == 6), Some(6));
    assert_eq!(mp.peek(), Some(&7));
    assert_eq!(mp.peek(), None);
    assert_eq!(mp.next(), Some(7));
    assert_eq!(mp.peek(), None);
}

#[test]
fn test_peek_nth() {
    let nums = vec![1u8,2,3,4,5];

    let iter = peek_nth(nums.iter().map(|&x| x));
    assert_eq!(nums, iter.collect::<Vec<_>>());

    let mut iter = peek_nth(nums.iter().map(|&x| x));

    assert_eq!(iter.peek_nth(0), Some(&1));
    assert_eq!(iter.peek_nth(0), Some(&1));
    assert_eq!(iter.next(), Some(1));

    assert_eq!(iter.peek_nth(0), Some(&2));
    assert_eq!(iter.peek_nth(1), Some(&3));
    assert_eq!(iter.next(), Some(2));

    assert_eq!(iter.peek_nth(0), Some(&3));
    assert_eq!(iter.peek_nth(1), Some(&4));
    assert_eq!(iter.peek_nth(2), Some(&5));
    assert_eq!(iter.peek_nth(3), None);

    assert_eq!(iter.next(), Some(3));
    assert_eq!(iter.next(), Some(4));

    assert_eq!(iter.peek_nth(0), Some(&5));
    assert_eq!(iter.peek_nth(1), None);
    assert_eq!(iter.next(), Some(5));
    assert_eq!(iter.next(), None);

    assert_eq!(iter.peek_nth(0), None);
    assert_eq!(iter.peek_nth(1), None);
}

#[test]
fn test_peek_nth_peeking_next() {
    use it::PeekingNext;
    let nums = vec![1u8,2,3,4,5,6,7];
    let mut iter = peek_nth(nums.iter().map(|&x| x));

    assert_eq!(iter.peeking_next(|&x| x != 0), Some(1));
    assert_eq!(iter.next(), Some(2));

    assert_eq!(iter.peek_nth(0), Some(&3));
    assert_eq!(iter.peek_nth(1), Some(&4));
    assert_eq!(iter.peeking_next(|&x| x == 3), Some(3));
    assert_eq!(iter.peek(), Some(&4));

    assert_eq!(iter.peeking_next(|&x| x != 4), None);
    assert_eq!(iter.peeking_next(|&x| x == 4), Some(4));
    assert_eq!(iter.peek_nth(0), Some(&5));
    assert_eq!(iter.peek_nth(1), Some(&6));

    assert_eq!(iter.peeking_next(|&x| x != 5), None);
    assert_eq!(iter.peek(), Some(&5));

    assert_eq!(iter.peeking_next(|&x| x == 5), Some(5));
    assert_eq!(iter.peeking_next(|&x| x == 6), Some(6));
    assert_eq!(iter.peek_nth(0), Some(&7));
    assert_eq!(iter.peek_nth(1), None);
    assert_eq!(iter.next(), Some(7));
    assert_eq!(iter.peek(), None);
}

#[test]
fn pad_using() {
    it::assert_equal((0..0).pad_using(1, |_| 1), 1..2);

    let v: Vec<usize> = vec![0, 1, 2];
    let r = v.into_iter().pad_using(5, |n| n);
    it::assert_equal(r, vec![0, 1, 2, 3, 4]);

    let v: Vec<usize> = vec![0, 1, 2];
    let r = v.into_iter().pad_using(1, |_| panic!());
    it::assert_equal(r, vec![0, 1, 2]);
}

#[test]
fn group_by() {
    for (ch1, sub) in &"AABBCCC".chars().group_by(|&x| x) {
        for ch2 in sub {
            assert_eq!(ch1, ch2);
        }
    }

    for (ch1, sub) in &"AAABBBCCCCDDDD".chars().group_by(|&x| x) {
        for ch2 in sub {
            assert_eq!(ch1, ch2);
            if ch1 == 'C' {
                break;
            }
        }
    }

    let toupper = |ch: &char| ch.to_uppercase().nth(0).unwrap();

    // try all possible orderings
    for indices in permutohedron::Heap::new(&mut [0, 1, 2, 3]) {
        let groups = "AaaBbbccCcDDDD".chars().group_by(&toupper);
        let mut subs = groups.into_iter().collect_vec();

        for &idx in &indices[..] {
            let (key, text) = match idx {
                 0 => ('A', "Aaa".chars()),
                 1 => ('B', "Bbb".chars()),
                 2 => ('C', "ccCc".chars()),
                 3 => ('D', "DDDD".chars()),
                 _ => unreachable!(),
            };
            assert_eq!(key, subs[idx].0);
            it::assert_equal(&mut subs[idx].1, text);
        }
    }

    let groups = "AAABBBCCCCDDDD".chars().group_by(|&x| x);
    let mut subs = groups.into_iter().map(|(_, g)| g).collect_vec();

    let sd = subs.pop().unwrap();
    let sc = subs.pop().unwrap();
    let sb = subs.pop().unwrap();
    let sa = subs.pop().unwrap();
    for (a, b, c, d) in multizip((sa, sb, sc, sd)) {
        assert_eq!(a, 'A');
        assert_eq!(b, 'B');
        assert_eq!(c, 'C');
        assert_eq!(d, 'D');
    }

    // check that the key closure is called exactly n times
    {
        let mut ntimes = 0;
        let text = "AABCCC";
        for (_, sub) in &text.chars().group_by(|&x| { ntimes += 1; x}) {
            for _ in sub {
            }
        }
        assert_eq!(ntimes, text.len());
    }

    {
        let mut ntimes = 0;
        let text = "AABCCC";
        for _ in &text.chars().group_by(|&x| { ntimes += 1; x}) {
        }
        assert_eq!(ntimes, text.len());
    }

    {
        let text = "ABCCCDEEFGHIJJKK";
        let gr = text.chars().group_by(|&x| x);
        it::assert_equal(gr.into_iter().flat_map(|(_, sub)| sub), text.chars());
    }
}

#[test]
fn group_by_lazy_2() {
    let data = vec![0, 1];
    let groups = data.iter().group_by(|k| *k);
    let gs = groups.into_iter().collect_vec();
    it::assert_equal(data.iter(), gs.into_iter().flat_map(|(_k, g)| g));

    let data = vec![0, 1, 1, 0, 0];
    let groups = data.iter().group_by(|k| *k);
    let mut gs = groups.into_iter().collect_vec();
    gs[1..].reverse();
    it::assert_equal(&[0, 0, 0, 1, 1], gs.into_iter().flat_map(|(_, g)| g));

    let grouper = data.iter().group_by(|k| *k);
    let mut groups = Vec::new();
    for (k, group) in &grouper {
        if *k == 1 {
            groups.push(group);
        }
    }
    it::assert_equal(&mut groups[0], &[1, 1]);

    let data = vec![0, 0, 0, 1, 1, 0, 0, 2, 2, 3, 3];
    let grouper = data.iter().group_by(|k| *k);
    let mut groups = Vec::new();
    for (i, (_, group)) in grouper.into_iter().enumerate() {
        if i < 2 {
            groups.push(group);
        } else if i < 4 {
            for _ in group {
            }
        } else {
            groups.push(group);
        }
    }
    it::assert_equal(&mut groups[0], &[0, 0, 0]);
    it::assert_equal(&mut groups[1], &[1, 1]);
    it::assert_equal(&mut groups[2], &[3, 3]);

    // use groups as chunks
    let data = vec![0, 0, 0, 1, 1, 0, 0, 2, 2, 3, 3];
    let mut i = 0;
    let grouper = data.iter().group_by(move |_| { let k = i / 3; i += 1; k });
    for (i, group) in &grouper {
        match i {
            0 => it::assert_equal(group, &[0, 0, 0]),
            1 => it::assert_equal(group, &[1, 1, 0]),
            2 => it::assert_equal(group, &[0, 2, 2]),
            3 => it::assert_equal(group, &[3, 3]),
            _ => unreachable!(),
        }
    }
}

#[test]
fn group_by_lazy_3() {
    // test consuming each group on the lap after it was produced
    let data = vec![0, 0, 0, 1, 1, 0, 0, 1, 1, 2, 2];
    let grouper = data.iter().group_by(|elt| *elt);
    let mut last = None;
    for (key, group) in &grouper {
        if let Some(gr) = last.take() {
            for elt in gr {
                assert!(elt != key && i32::abs(elt - key) == 1);
            }
        }
        last = Some(group);
    }
}

#[test]
fn chunks() {
    let data = vec![0, 0, 0, 1, 1, 0, 0, 2, 2, 3, 3];
    let grouper = data.iter().chunks(3);
    for (i, chunk) in grouper.into_iter().enumerate() {
        match i {
            0 => it::assert_equal(chunk, &[0, 0, 0]),
            1 => it::assert_equal(chunk, &[1, 1, 0]),
            2 => it::assert_equal(chunk, &[0, 2, 2]),
            3 => it::assert_equal(chunk, &[3, 3]),
            _ => unreachable!(),
        }
    }
}

#[test]
fn concat_empty() {
    let data: Vec<Vec<()>> = Vec::new();
    assert_eq!(data.into_iter().concat(), Vec::new())
}

#[test]
fn concat_non_empty() {
    let data = vec![vec![1,2,3], vec![4,5,6], vec![7,8,9]];
    assert_eq!(data.into_iter().concat(), vec![1,2,3,4,5,6,7,8,9])
}

#[test]
fn combinations() {
    assert!((1..3).combinations(5).next().is_none());

    let it = (1..3).combinations(2);
    it::assert_equal(it, vec![
        vec![1, 2],
        ]);

    let it = (1..5).combinations(2);
    it::assert_equal(it, vec![
        vec![1, 2],
        vec![1, 3],
        vec![1, 4],
        vec![2, 3],
        vec![2, 4],
        vec![3, 4],
        ]);

    it::assert_equal((0..0).tuple_combinations::<(_, _)>(), <Vec<_>>::new());
    it::assert_equal((0..1).tuple_combinations::<(_, _)>(), <Vec<_>>::new());
    it::assert_equal((0..2).tuple_combinations::<(_, _)>(), vec![(0, 1)]);

    it::assert_equal((0..0).combinations(2), <Vec<Vec<_>>>::new());
    it::assert_equal((0..1).combinations(1), vec![vec![0]]);
    it::assert_equal((0..2).combinations(1), vec![vec![0], vec![1]]);
    it::assert_equal((0..2).combinations(2), vec![vec![0, 1]]);
}

#[test]
fn combinations_of_too_short() {
    for i in 1..10 {
        assert!((0..0).combinations(i).next().is_none());
        assert!((0..i - 1).combinations(i).next().is_none());
    }
}


#[test]
fn combinations_zero() {
    it::assert_equal((1..3).combinations(0), vec![vec![]]);
    it::assert_equal((0..0).combinations(0), vec![vec![]]);
}

#[test]
fn permutations_zero() {
    it::assert_equal((1..3).permutations(0), vec![vec![]]);
    it::assert_equal((0..0).permutations(0), vec![vec![]]);
}

#[test]
fn combinations_with_replacement() {
    // Pool smaller than n
    it::assert_equal((0..1).combinations_with_replacement(2), vec![vec![0, 0]]);
    // Pool larger than n
    it::assert_equal(
        (0..3).combinations_with_replacement(2),
        vec![
            vec![0, 0],
            vec![0, 1],
            vec![0, 2],
            vec![1, 1],
            vec![1, 2],
            vec![2, 2],
        ],
    );
    // Zero size
    it::assert_equal(
        (0..3).combinations_with_replacement(0),
        vec![vec![]],
    );
    // Zero size on empty pool
    it::assert_equal(
        (0..0).combinations_with_replacement(0),
        vec![vec![]],
    );
    // Empty pool
    it::assert_equal(
        (0..0).combinations_with_replacement(2),
        <Vec<Vec<_>>>::new(),
    );
}

#[test]
fn powerset() {
    it::assert_equal((0..0).powerset(), vec![vec![]]);
    it::assert_equal((0..1).powerset(), vec![vec![], vec![0]]);
    it::assert_equal((0..2).powerset(), vec![vec![], vec![0], vec![1], vec![0, 1]]);
    it::assert_equal((0..3).powerset(), vec![
        vec![],
        vec![0], vec![1], vec![2],
        vec![0, 1], vec![0, 2], vec![1, 2],
        vec![0, 1, 2]
    ]);

    assert_eq!((0..4).powerset().count(), 1 << 4);
    assert_eq!((0..8).powerset().count(), 1 << 8);
    assert_eq!((0..16).powerset().count(), 1 << 16);
}

#[test]
fn diff_mismatch() {
    let a = vec![1, 2, 3, 4];
    let b = vec![1.0, 5.0, 3.0, 4.0];
    let b_map = b.into_iter().map(|f| f as i32);
    let diff = it::diff_with(a.iter(), b_map, |a, b| *a == b);

    assert!(match diff {
        Some(it::Diff::FirstMismatch(1, _, from_diff)) =>
            from_diff.collect::<Vec<_>>() == vec![5, 3, 4],
        _ => false,
    });
}

#[test]
fn diff_longer() {
    let a = vec![1, 2, 3, 4];
    let b = vec![1.0, 2.0, 3.0, 4.0, 5.0, 6.0];
    let b_map = b.into_iter().map(|f| f as i32);
    let diff = it::diff_with(a.iter(), b_map, |a, b| *a == b);

    assert!(match diff {
        Some(it::Diff::Longer(_, remaining)) =>
            remaining.collect::<Vec<_>>() == vec![5, 6],
        _ => false,
    });
}

#[test]
fn diff_shorter() {
    let a = vec![1, 2, 3, 4];
    let b = vec![1.0, 2.0];
    let b_map = b.into_iter().map(|f| f as i32);
    let diff = it::diff_with(a.iter(), b_map, |a, b| *a == b);

    assert!(match diff {
        Some(it::Diff::Shorter(len, _)) => len == 2,
        _ => false,
    });
}

#[test]
fn minmax() {
    use std::cmp::Ordering;
    use crate::it::MinMaxResult;

    // A peculiar type: Equality compares both tuple items, but ordering only the
    // first item.  This is so we can check the stability property easily.
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

    assert_eq!(None::<Option<u32>>.iter().minmax(), MinMaxResult::NoElements);

    assert_eq!(Some(1u32).iter().minmax(), MinMaxResult::OneElement(&1));

    let data = vec![Val(0, 1), Val(2, 0), Val(0, 2), Val(1, 0), Val(2, 1)];

    let minmax = data.iter().minmax();
    assert_eq!(minmax, MinMaxResult::MinMax(&Val(0, 1), &Val(2, 1)));

    let (min, max) = data.iter().minmax_by_key(|v| v.1).into_option().unwrap();
    assert_eq!(min, &Val(2, 0));
    assert_eq!(max, &Val(0, 2));

    let (min, max) = data.iter().minmax_by(|x, y| x.1.cmp(&y.1)).into_option().unwrap();
    assert_eq!(min, &Val(2, 0));
    assert_eq!(max, &Val(0, 2));
}

#[test]
fn format() {
    let data = [0, 1, 2, 3];
    let ans1 = "0, 1, 2, 3";
    let ans2 = "0--1--2--3";

    let t1 = format!("{}", data.iter().format(", "));
    assert_eq!(t1, ans1);
    let t2 = format!("{:?}", data.iter().format("--"));
    assert_eq!(t2, ans2);

    let dataf = [1.1, 2.71828, -22.];
    let t3 = format!("{:.2e}", dataf.iter().format(", "));
    assert_eq!(t3, "1.10e0, 2.72e0, -2.20e1");
}

#[test]
fn while_some() {
    let ns = (1..10).map(|x| if x % 5 != 0 { Some(x) } else { None })
                    .while_some();
    it::assert_equal(ns, vec![1, 2, 3, 4]);
}

#[allow(deprecated)]
#[test]
fn fold_while() {
    let mut iterations = 0;
    let vec = vec![1, 2, 3, 4, 5, 6, 7, 8, 9, 10];
    let sum = vec.into_iter().fold_while(0, |acc, item| {
        iterations += 1;
        let new_sum = acc.clone() + item;
        if new_sum <= 20 {
            FoldWhile::Continue(new_sum)
        } else {
            FoldWhile::Done(acc)
        }
    }).into_inner();
    assert_eq!(iterations, 6);
    assert_eq!(sum, 15);
}

#[test]
fn tree_fold1() {
    let x = [
        "",
        "0",
        "0 1 x",
        "0 1 x 2 x",
        "0 1 x 2 3 x x",
        "0 1 x 2 3 x x 4 x",
        "0 1 x 2 3 x x 4 5 x x",
        "0 1 x 2 3 x x 4 5 x 6 x x",
        "0 1 x 2 3 x x 4 5 x 6 7 x x x",
        "0 1 x 2 3 x x 4 5 x 6 7 x x x 8 x",
        "0 1 x 2 3 x x 4 5 x 6 7 x x x 8 9 x x",
        "0 1 x 2 3 x x 4 5 x 6 7 x x x 8 9 x 10 x x",
        "0 1 x 2 3 x x 4 5 x 6 7 x x x 8 9 x 10 11 x x x",
        "0 1 x 2 3 x x 4 5 x 6 7 x x x 8 9 x 10 11 x x 12 x x",
        "0 1 x 2 3 x x 4 5 x 6 7 x x x 8 9 x 10 11 x x 12 13 x x x",
        "0 1 x 2 3 x x 4 5 x 6 7 x x x 8 9 x 10 11 x x 12 13 x 14 x x x",
        "0 1 x 2 3 x x 4 5 x 6 7 x x x 8 9 x 10 11 x x 12 13 x 14 15 x x x x",
    ];
    for (i, &s) in x.iter().enumerate() {
        let expected = if s == "" { None } else { Some(s.to_string()) };
        let num_strings = (0..i).map(|x| x.to_string());
        let actual = num_strings.tree_fold1(|a, b| format!("{} {} x", a, b));
        assert_eq!(actual, expected);
    }
}

#[test]
fn exactly_one_question_mark_syntax_works() {
    exactly_one_question_mark_return().unwrap_err();
}

fn exactly_one_question_mark_return() -> Result<(), ExactlyOneError<std::slice::Iter<'static, ()>>> {
    [].iter().exactly_one()?;
    Ok(())
}
