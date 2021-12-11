## [0.6.0]
- API incompatible change: depend on hashbrown 0.9, re-export renamed
  hashbrown::TryReserveError type.
- Add a `Debug` impl to `LruCache` (thanks @thomcc!)
- Adjust trait bounds for `LinkedHashMap::retain`, `LinkedHashSet::default` to
  be less strict (to match hashbrown)
- Adjust trait bounds for all `Debug` impls to be less strict (to match
  hashbrown).
- Adjust trait bounds for all `IntoIterator` impls to be less strict (to match
  hashbrown).
- Adjust trait bounds for `LruCache::with_hasher`, `LruCache::capacity`,
  `LruCache::len`, `LruCache::is_empty`, `LruCache::clear`, `LruCache::iter`,
  `LruCache::iter_mut`, and `LruCache::drain` to be less strict
- Add optional serde support for `LinkedHashMap` and `LinkedHashSet`.
- Add `to_back` and `to_front` methods for LinkedHashSet to control entry order.

## [0.5.1]
- Add `LinkedHashMap::remove_entry` and `LruCache::remove_entry`
- Add `LruCache::new_unbounded` constructor that sets capacity to usize::MAX
- Add `LruCache::get` method to go with `LruCache::get_mut`
- Add `LruCache::peek` and `LruCache::peek_mut` to access the cache without
  moving the entry in the LRU list

## [0.5.0]
- API incompatible change: depend on hashbrown 0.7

## [0.4.0]
- API incompatible change: depend on hashbrown 0.6
- Passes miri

## [0.3.0]
- Add some *minimal* documentation for methods that change the internal ordering.
- Decide on a pattern for methods that change the internal ordering: the word
  "insert" means that it will move an existing entry to the back.
- Some methods have been renamed to conform to the above system.

## [0.2.1]
- Fix variance for LinkedHashMap (now covariant where appropriate)
- Add Debug impls to many more associated types
- Add LinkedHashSet
- Add `LinkedHashMap::retain`

## [0.2.0]
- Move `linked_hash_map` into its own module
- Add `LruCache` type ported from `lru-cache` crate into its own module
- Add `LruCache` entry and raw-entry API
- Add `linked_hash_map` `IntoIter` iterator that is different from `Drain` iterator
- Make `Drain` iterator recycle freed linked list nodes

## [0.1.0]
- Initial release
