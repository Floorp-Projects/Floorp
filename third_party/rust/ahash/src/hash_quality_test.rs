use core::hash::{Hash, Hasher};

fn assert_sufficiently_different(a: u64, b: u64, tolerance: i32) {
    let (same_byte_count, same_nibble_count) = count_same_bytes_and_nibbles(a, b);
    assert!(same_byte_count <= tolerance, "{:x} vs {:x}: {:}", a, b, same_byte_count);
    assert!(
        same_nibble_count <= tolerance * 3,
        "{:x} vs {:x}: {:}",
        a,
        b,
        same_nibble_count
    );
    let flipped_bits = (a ^ b).count_ones();
    assert!(
        flipped_bits > 12 && flipped_bits < 52,
        "{:x} and {:x}: {:}",
        a,
        b,
        flipped_bits
    );
    for rotate in 0..64 {
        let flipped_bits2 = (a ^ (b.rotate_left(rotate))).count_ones();
        assert!(
            flipped_bits2 > 10 && flipped_bits2 < 54,
            "{:x} and {:x}: {:}",
            a,
            b.rotate_left(rotate),
            flipped_bits2
        );
    }
}

fn count_same_bytes_and_nibbles(a: u64, b: u64) -> (i32, i32) {
    let mut same_byte_count = 0;
    let mut same_nibble_count = 0;
    for byte in 0..8 {
        let ba = (a >> (8 * byte)) as u8;
        let bb = (b >> (8 * byte)) as u8;
        if ba == bb {
            same_byte_count += 1;
        }
        if ba & 0xF0u8 == bb & 0xF0u8 {
            same_nibble_count += 1;
        }
        if ba & 0x0Fu8 == bb & 0x0Fu8 {
            same_nibble_count += 1;
        }
    }
    (same_byte_count, same_nibble_count)
}

fn test_keys_change_output<T: Hasher>(constructor: impl Fn(u64, u64) -> T) {
    let mut a = constructor(0, 0);
    let mut b = constructor(0, 1);
    let mut c = constructor(1, 0);
    let mut d = constructor(1, 1);
    "test".hash(&mut a);
    "test".hash(&mut b);
    "test".hash(&mut c);
    "test".hash(&mut d);
    assert_sufficiently_different(a.finish(), b.finish(), 1);
    assert_sufficiently_different(a.finish(), c.finish(), 1);
    assert_sufficiently_different(a.finish(), d.finish(), 1);
    assert_sufficiently_different(b.finish(), c.finish(), 1);
    assert_sufficiently_different(b.finish(), d.finish(), 1);
    assert_sufficiently_different(c.finish(), d.finish(), 1);
}

fn test_input_affect_every_byte<T: Hasher>(constructor: impl Fn(u64, u64) -> T) {
    let mut base = constructor(0, 0);
    0.hash(&mut base);
    let base = base.finish();
    for shift in 0..16 {
        let mut alternitives = vec![];
        for v in 0..256 {
            let input = (v as u128) << (shift * 8);
            let mut hasher = constructor(0, 0);
            input.hash(&mut hasher);
            alternitives.push(hasher.finish());
        }
        assert_each_byte_differes(base, alternitives);
    }
}

fn test_keys_affect_every_byte<T: Hasher>(constructor: impl Fn(u64, u64) -> T) {
    let mut base = constructor(0, 0);
    0.hash(&mut base);
    let base = base.finish();
    for shift in 0..8 {
        let mut alternitives1 = vec![];
        let mut alternitives2 = vec![];
        for v in 0..256 {
            let input = (v as u64) << (shift * 8);
            let mut hasher1 = constructor(input, 0);
            let mut hasher2 = constructor(0, input);
            0.hash(&mut hasher1);
            0.hash(&mut hasher2);
            alternitives1.push(hasher1.finish());
            alternitives2.push(hasher2.finish());
        }
        assert_each_byte_differes(base, alternitives1);
        assert_each_byte_differes(base, alternitives2);
    }
}

fn assert_each_byte_differes(base: u64, alternitives: Vec<u64>) {
    let mut changed_bits = 0_u64;
    for alternitive in alternitives {
        changed_bits |= base ^ alternitive
    }
    assert_eq!(core::u64::MAX, changed_bits, "Bits changed: {:x}", changed_bits);
}

fn test_finish_is_consistant<T: Hasher>(constructor: impl Fn(u64, u64) -> T) {
    let mut hasher = constructor(1, 2);
    "Foo".hash(&mut hasher);
    let a = hasher.finish();
    let b = hasher.finish();
    assert_eq!(a, b);
}

fn test_single_key_bit_flip<T: Hasher>(constructor: impl Fn(u64, u64) -> T) {
    for bit in 0..64 {
        let mut a = constructor(0, 0);
        let mut b = constructor(0, 1 << bit);
        let mut c = constructor(1 << bit, 0);
        "1234".hash(&mut a);
        "1234".hash(&mut b);
        "1234".hash(&mut c);
        assert_sufficiently_different(a.finish(), b.finish(), 2);
        assert_sufficiently_different(a.finish(), c.finish(), 2);
        assert_sufficiently_different(b.finish(), c.finish(), 2);
        let mut a = constructor(0, 0);
        let mut b = constructor(0, 1 << bit);
        let mut c = constructor(1 << bit, 0);
        "12345678".hash(&mut a);
        "12345678".hash(&mut b);
        "12345678".hash(&mut c);
        assert_sufficiently_different(a.finish(), b.finish(), 2);
        assert_sufficiently_different(a.finish(), c.finish(), 2);
        assert_sufficiently_different(b.finish(), c.finish(), 2);
        let mut a = constructor(0, 0);
        let mut b = constructor(0, 1 << bit);
        let mut c = constructor(1 << bit, 0);
        "1234567812345678".hash(&mut a);
        "1234567812345678".hash(&mut b);
        "1234567812345678".hash(&mut c);
        assert_sufficiently_different(a.finish(), b.finish(), 2);
        assert_sufficiently_different(a.finish(), c.finish(), 2);
        assert_sufficiently_different(b.finish(), c.finish(), 2);
    }
}

fn test_all_bytes_matter<T: Hasher>(hasher: impl Fn() -> T) {
    let mut item = vec![0; 256];
    let base_hash = hash(&item, &hasher);
    for pos in 0..256 {
        item[pos] = 255;
        let hash = hash(&item, &hasher);
        assert_ne!(base_hash, hash, "Position {} did not affect output", pos);
        item[pos] = 0;
    }
}

fn hash<T: Hasher>(b: &impl Hash, hasher: &dyn Fn() -> T) -> u64 {
    let mut hasher = hasher();
    b.hash(&mut hasher);
    hasher.finish()
}

fn test_single_bit_flip<T: Hasher>(hasher: impl Fn() -> T) {
    let size = 32;
    let compare_value = hash(&0u32, &hasher);
    for pos in 0..size {
        let test_value = hash(&(1u32 << pos), &hasher);
        assert_sufficiently_different(compare_value, test_value, 2);
    }
    let size = 64;
    let compare_value = hash(&0u64, &hasher);
    for pos in 0..size {
        let test_value = hash(&(1u64 << pos), &hasher);
        assert_sufficiently_different(compare_value, test_value, 2);
    }
    let size = 128;
    let compare_value = hash(&0u128, &hasher);
    for pos in 0..size {
        let test_value = hash(&(1u128 << pos), &hasher);
        assert_sufficiently_different(compare_value, test_value, 2);
    }
}

fn test_padding_doesnot_collide<T: Hasher>(hasher: impl Fn() -> T) {
    for c in 0..128u8 {
        for string in ["", "1234", "12345678", "1234567812345678"].iter() {
            let mut short = hasher();
            string.hash(&mut short);
            let value = short.finish();
            let mut string = string.to_string();
            for num in 1..=128 {
                let mut long = hasher();
                string.push(c as char);
                string.hash(&mut long);
                let (same_bytes, same_nibbles) = count_same_bytes_and_nibbles(value, long.finish());
                assert!(
                    same_bytes <= 2,
                    format!("{} bytes of {} -> {:x} vs {:x}", num, c, value, long.finish())
                );
                assert!(
                    same_nibbles <= 8,
                    format!("{} bytes of {} -> {:x} vs {:x}", num, c, value, long.finish())
                );
                let flipped_bits = (value ^ long.finish()).count_ones();
                assert!(flipped_bits > 10);
            }
        }
    }
}

#[cfg(test)]
mod fallback_tests {
    use crate::fallback_hash::*;
    use crate::hash_quality_test::*;

    #[test]
    fn fallback_single_bit_flip() {
        test_single_bit_flip(|| AHasher::test_with_keys(0, 0))
    }

    #[test]
    fn fallback_single_key_bit_flip() {
        test_single_key_bit_flip(AHasher::test_with_keys)
    }

    #[test]
    fn fallback_all_bytes_matter() {
        test_all_bytes_matter(|| AHasher::test_with_keys(0, 0));
    }

    #[test]
    fn fallback_keys_change_output() {
        test_keys_change_output(AHasher::test_with_keys);
    }

    #[test]
    fn fallback_input_affect_every_byte() {
        test_input_affect_every_byte(AHasher::test_with_keys);
    }

    #[test]
    fn fallback_keys_affect_every_byte() {
        test_keys_affect_every_byte(AHasher::test_with_keys);
    }

    #[test]
    fn fallback_finish_is_consistant() {
        test_finish_is_consistant(AHasher::test_with_keys)
    }

    #[test]
    fn fallback_padding_doesnot_collide() {
        test_padding_doesnot_collide(|| AHasher::test_with_keys(0, 1))
    }
}

///Basic sanity tests of the cypto properties of aHash.
#[cfg(all(any(target_arch = "x86", target_arch = "x86_64"), target_feature = "aes"))]
#[cfg(test)]
mod aes_tests {
    use crate::aes_hash::*;
    use crate::hash_quality_test::*;
    use std::hash::{Hash, Hasher};

    const BAD_KEY: u64 = 0x5252_5252_5252_5252; //Thi   s encrypts to 0.

    #[test]
    fn test_single_bit_in_byte() {
        let mut hasher1 = AHasher::new_with_keys(64, 64);
        8_u32.hash(&mut hasher1);
        let mut hasher2 = AHasher::new_with_keys(64, 64);
        0_u32.hash(&mut hasher2);
        assert_sufficiently_different(hasher1.finish(), hasher2.finish(), 1);
    }

    #[test]
    fn aes_single_bit_flip() {
        test_single_bit_flip(|| AHasher::test_with_keys(BAD_KEY, BAD_KEY))
    }

    #[test]
    fn aes_single_key_bit_flip() {
        test_single_key_bit_flip(|k1, k2| AHasher::test_with_keys(k1, k2))
    }

    #[test]
    fn aes_all_bytes_matter() {
        test_all_bytes_matter(|| AHasher::test_with_keys(BAD_KEY, BAD_KEY));
    }

    #[test]
    fn aes_keys_change_output() {
        test_keys_change_output(AHasher::test_with_keys);
    }

    #[test]
    fn aes_input_affect_every_byte() {
        test_input_affect_every_byte(AHasher::test_with_keys);
    }

    #[test]
    fn aes_keys_affect_every_byte() {
        test_keys_affect_every_byte(AHasher::test_with_keys);
    }
    #[test]
    fn aes_finish_is_consistant() {
        test_finish_is_consistant(AHasher::test_with_keys)
    }

    #[test]
    fn aes_padding_doesnot_collide() {
        test_padding_doesnot_collide(|| AHasher::test_with_keys(BAD_KEY, BAD_KEY))
    }
}
