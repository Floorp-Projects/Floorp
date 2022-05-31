extern crate std;

use self::std::vec::Vec;
use super::*;

type TestCache = LRUCache<i32, 4>;

/// Convenience function for test assertions
fn items<T, const N: usize>(cache: &mut LRUCache<T, N>) -> Vec<T>
where
    T: Clone,
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
    assert_eq!(cache.is_empty(), true);
    assert_eq!(items(&mut cache), []);
    cache.insert(1);
    assert_eq!(cache.is_empty(), false);
}

#[test]
fn len() {
    let mut cache = TestCache::default();
    cache.insert(1);
    assert_eq!(cache.len(), 1);
    assert_eq!(items(&mut cache), [1]);
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

#[test]
fn touch() {
    let mut cache = TestCache::default();

    cache.insert(0);
    cache.insert(1);
    cache.insert(2);
    cache.insert(3);

    cache.touch(|x| *x == 5);

    assert_eq!(items(&mut cache), [3, 2, 1, 0], "Nothing is touched.");

    cache.touch(|x| *x == 1);

    assert_eq!(
        items(&mut cache),
        [1, 3, 2, 0],
        "Touched item is moved to front."
    );
}

#[test]
fn find() {
    let mut cache = TestCache::default();

    cache.insert(0);
    cache.insert(1);
    cache.insert(2);
    cache.insert(3);

    let result = cache.find(|x| *x == 5).copied();

    assert_eq!(result, None);
    assert_eq!(items(&mut cache), [3, 2, 1, 0], "Nothing is touched.");

    let result = cache.find(|x| *x == 1).copied();

    assert_eq!(result, Some(1));
    assert_eq!(
        items(&mut cache),
        [1, 3, 2, 0],
        "Retrieved item is moved to front."
    );
}

#[test]
fn front() {
    let mut cache = TestCache::default();

    assert_eq!(cache.front(), None, "Nothing is in the front.");

    cache.insert(0);
    cache.insert(1);

    assert_eq!(
        cache.front(),
        Some(&1),
        "The last inserted item should be in the front."
    );

    cache.touch(|x| *x == 0);

    assert_eq!(
        cache.front(),
        Some(&0),
        "Touched item should be in the front."
    );
}

#[test]
fn get() {
    let mut cache = TestCache::default();

    assert_eq!(cache.get(0), None, "Nothing at index 0.");

    cache.insert(42);
    cache.insert(64);

    assert_eq!(
        cache.get(0),
        Some(&64),
        "The last inserted item should be at index 0."
    );

    assert_eq!(
        cache.get(1),
        Some(&42),
        "The first inserted item should be at index 1."
    );

    cache.touch(|x| *x == 42);

    assert_eq!(
        cache.get(0),
        Some(&42),
        "The last touched item should be at index 0."
    );

    assert_eq!(
        cache.get(1),
        Some(&64),
        "The previously front item should be at index 1."
    );
}
