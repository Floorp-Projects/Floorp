use ahash::{AHasher, CallHasher, RandomState};
use std::hash::BuildHasher;

#[macro_use]
extern crate no_panic;

#[inline(never)]
#[no_panic]
fn hash_test_final(num: i32, string: &str) -> (u64, u64) {
    use core::hash::Hasher;
    let mut hasher1 = AHasher::new_with_keys(1, 2);
    let mut hasher2 = AHasher::new_with_keys(3, 4);
    hasher1.write_i32(num);
    hasher2.write(string.as_bytes());
    (hasher1.finish(), hasher2.finish())
}

#[inline(never)]
fn hash_test_final_wrapper(num: i32, string: &str) {
    hash_test_final(num, string);
}

#[inline(never)]
#[no_panic]
fn hash_test_specialize(num: i32, string: &str) -> (u64, u64) {
    let hasher1 = AHasher::new_with_keys(1, 2);
    let hasher2 = AHasher::new_with_keys(1, 2);
    (num.get_hash(hasher1), string.as_bytes().get_hash(hasher2))
}

#[inline(never)]
fn hash_test_random_wrapper(num: i32, string: &str) {
    hash_test_specialize(num, string);
}

#[inline(never)]
#[no_panic]
fn hash_test_random(num: i32, string: &str) -> (u64, u64) {
    let hasher1 = RandomState::with_seeds(1, 2).build_hasher();
    let hasher2 = RandomState::with_seeds(1, 2).build_hasher();
    (num.get_hash(hasher1), string.as_bytes().get_hash(hasher2))
}

#[inline(never)]
fn hash_test_specialize_wrapper(num: i32, string: &str) {
    hash_test_specialize(num, string);
}

#[test]
fn test_no_panic() {
    hash_test_final_wrapper(2, "Foo");
    hash_test_specialize_wrapper(2, "Bar");
    hash_test_random_wrapper(2, "Baz");
}
