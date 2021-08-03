extern crate std;

use self::std::vec::Vec;
use super::*;

type TestCache = LRUCache<[Entry<i32>; 4]>;

/// Convenience function for test assertions
fn items<T, A>(cache: &mut LRUCache<A>) -> Vec<T>
where
    T: Clone,
    A: Array<Item = Entry<T>>,
{
    let mut v = Vec::new();
    let mut iter = cache.iter_mut();
    while let Some((_idx, val)) = iter.next() {
        v.push(val.clone())
    }
    v
}

#[test]
fn empty() {
    let mut cache = TestCache::default();
    assert_eq!(cache.len(), 0);
    assert_eq!(items(&mut cache), []);
}

#[test]
fn insert() {
    let mut cache = TestCache::default();
    cache.insert(1);
    assert_eq!(cache.len(), 1);
    cache.insert(2);
    assert_eq!(cache.len(), 2);
    cache.insert(3);
    assert_eq!(cache.len(), 3);
    cache.insert(4);
    assert_eq!(cache.len(), 4);
    assert_eq!(
        items(&mut cache),
        [4, 3, 2, 1],
        "Ordered from most- to least-recent."
    );

    cache.insert(5);
    assert_eq!(cache.len(), 4);
    assert_eq!(
        items(&mut cache),
        [5, 4, 3, 2],
        "Least-recently-used item evicted."
    );

    cache.insert(6);
    cache.insert(7);
    cache.insert(8);
    cache.insert(9);
    assert_eq!(
        items(&mut cache),
        [9, 8, 7, 6],
        "Least-recently-used item evicted."
    );
}

#[test]
fn lookup() {
    let mut cache = TestCache::default();
    cache.insert(1);
    cache.insert(2);
    cache.insert(3);
    cache.insert(4);

    let result = cache.lookup(|x| if *x == 5 { Some(()) } else { None });
    assert_eq!(result, None, "Cache miss.");
    assert_eq!(items(&mut cache), [4, 3, 2, 1], "Order not changed.");

    // Cache hit
    let result = cache.lookup(|x| if *x == 3 { Some(*x * 2) } else { None });
    assert_eq!(result, Some(6), "Cache hit.");
    assert_eq!(
        items(&mut cache),
        [3, 4, 2, 1],
        "Matching item moved to front."
    );
}

#[test]
fn clear() {
    let mut cache = TestCache::default();
    cache.insert(1);
    cache.clear();
    assert_eq!(items(&mut cache), [], "all items evicted");

    cache.insert(1);
    cache.insert(2);
    cache.insert(3);
    cache.insert(4);
    assert_eq!(items(&mut cache), [4, 3, 2, 1]);
    cache.clear();
    assert_eq!(items(&mut cache), [], "all items evicted again");
}

#[quickcheck]
fn touch(num: i32) {
    let first = num;
    let second = num + 1;
    let third = num + 2;
    let fourth = num + 3;

    let mut cache = TestCache::default();

    cache.insert(first);
    cache.insert(second);
    cache.insert(third);
    cache.insert(fourth);

    cache.touch(|x| *x == fourth + 1);

    assert_eq!(
        items(&mut cache),
        [fourth, third, second, first],
        "Nothing is touched."
    );

    cache.touch(|x| *x == second);

    assert_eq!(
        items(&mut cache),
        [second, fourth, third, first],
        "Touched item is moved to front."
    );
}

#[quickcheck]
fn find(num: i32) {
    let first = num;
    let second = num + 1;
    let third = num + 2;
    let fourth = num + 3;

    let mut cache = TestCache::default();

    cache.insert(first);
    cache.insert(second);
    cache.insert(third);
    cache.insert(fourth);

    cache.find(|x| *x == fourth + 1);

    assert_eq!(
        items(&mut cache),
        [fourth, third, second, first],
        "Nothing is touched."
    );

    cache.find(|x| *x == second);

    assert_eq!(
        items(&mut cache),
        [second, fourth, third, first],
        "Touched item is moved to front."
    );
}

#[quickcheck]
fn front(num: i32) {
    let first = num;
    let second = num + 1;

    let mut cache = TestCache::default();

    assert_eq!(cache.front(), None, "Nothing is in the front.");

    cache.insert(first);
    cache.insert(second);

    assert_eq!(
        cache.front(),
        Some(&second),
        "The last inserted item should be in the front."
    );

    cache.touch(|x| *x == first);

    assert_eq!(
        cache.front(),
        Some(&first),
        "Touched item should be in the front."
    );
}
