extern crate std;

use self::std::vec::Vec;
use super::*;

type TestCache = LRUCache<[Entry<i32>; 4]>;

/// Convenience function for test assertions
fn items<T, A>(cache: &mut LRUCache<A>) -> Vec<T>
where
    T: Clone,
    A: Array<Item=Entry<T>>
{
    cache.iter_mut().map(|(_, x)| x.clone()).collect()
}

#[test]
fn empty() {
    let mut cache = TestCache::default();
    assert_eq!(cache.num_entries(), 0);
    assert_eq!(items(&mut cache), []);
}

#[test]
fn insert() {
    let mut cache = TestCache::default();
    cache.insert(1);
    assert_eq!(cache.num_entries(), 1);
    cache.insert(2);
    assert_eq!(cache.num_entries(), 2);
    cache.insert(3);
    assert_eq!(cache.num_entries(), 3);
    cache.insert(4);
    assert_eq!(cache.num_entries(), 4);
    assert_eq!(items(&mut cache), [4, 3, 2, 1], "Ordered from most- to least-recent.");

    cache.insert(5);
    assert_eq!(cache.num_entries(), 4);
    assert_eq!(items(&mut cache), [5, 4, 3, 2], "Least-recently-used item evicted.");

    cache.insert(6);
    cache.insert(7);
    cache.insert(8);
    cache.insert(9);
    assert_eq!(items(&mut cache), [9, 8, 7, 6], "Least-recently-used item evicted.");
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
    assert_eq!(items(&mut cache), [3, 4, 2, 1], "Matching item moved to front.");
}

#[test]
fn evict_all() {
    let mut cache = TestCache::default();
    cache.insert(1);
    cache.evict_all();
    assert_eq!(items(&mut cache), [], "all items evicted");

    cache.insert(1);
    cache.insert(2);
    cache.insert(3);
    cache.insert(4);
    assert_eq!(items(&mut cache), [4, 3, 2, 1]);
    cache.evict_all();
    assert_eq!(items(&mut cache), [], "all items evicted again");
}

