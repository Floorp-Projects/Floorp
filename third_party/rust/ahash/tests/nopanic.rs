use ahash::AHasher;

#[macro_use]
extern crate no_panic;

#[inline(never)]
#[no_panic]
fn hash_test_final(num: i32, string: &str) -> (u64, u64) {
    use core::hash::Hasher;
    let mut hasher1 = AHasher::new_with_keys(0, 1);
    let mut hasher2 = AHasher::new_with_keys(0, 2);
    hasher1.write_i32(num);
    hasher2.write(string.as_bytes());
    (hasher1.finish(), hasher2.finish())
}

#[inline(never)]
fn hash_test_final_wrapper(num: i32, string: &str) {
    hash_test_final(num, string);
}

#[test]
fn test_no_panic() {
    hash_test_final_wrapper(2, "");
}
