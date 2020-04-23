use crate::convert::*;
use core::hash::Hasher;

/// A `Hasher` for hashing an arbitrary stream of bytes.
///
/// Instances of [`AHasher`] represent state that is updated while hashing data.
///
/// Each method updates the internal state based on the new data provided. Once
/// all of the data has been provided, the resulting hash can be obtained by calling
/// `finish()`
///
/// [Clone] is also provided in case you wish to calculate hashes for two different items that
/// start with the same data.
///
#[derive(Debug, Clone)]
pub struct AHasher {
    buffer: [u64; 2],
    key: u128,
}

impl AHasher {
    /// Creates a new hasher keyed to the provided keys.
    /// # Example
    ///
    /// ```
    /// use std::hash::Hasher;
    /// use ahash::AHasher;
    ///
    /// let mut hasher = AHasher::new_with_keys(123, 456);
    ///
    /// hasher.write_u32(1989);
    /// hasher.write_u8(11);
    /// hasher.write_u8(9);
    /// hasher.write(b"Huh?");
    ///
    /// println!("Hash is {:x}!", hasher.finish());
    /// ```
    #[inline]
    pub fn new_with_keys(key0: u64, key1: u64) -> Self {
        Self {
            buffer: [key0, key1],
            key: [key1, key0].convert(),
        }
    }

    #[cfg(test)]
    pub(crate) fn test_with_keys(key1: u64, key2: u64) -> AHasher {
        use crate::random_state::scramble_keys;
        let (k1, k2) = scramble_keys(key1, key2);
        AHasher {
            buffer: [k1, k2],
            key: [k2, k1].convert(),
        }
    }

    #[inline(always)]
    fn hash_in(&mut self, new_value: u128) {
        self.buffer = aeshashx2(self.buffer.convert(), new_value, self.key).convert();
    }

    #[inline(always)]
    fn hash_in_2(&mut self, v1: u128, v2: u128) {
        let updated = aeshash(self.buffer.convert(), v1);
        self.buffer = aeshashx2(updated, v2, updated).convert();
    }
}

/// Provides methods to hash all of the primitive types.
impl Hasher for AHasher {
    #[inline]
    fn write_u8(&mut self, i: u8) {
        self.write_u128(i as u128);
    }

    #[inline]
    fn write_u16(&mut self, i: u16) {
        self.write_u128(i as u128);
    }

    #[inline]
    fn write_u32(&mut self, i: u32) {
        self.write_u128(i as u128);
    }

    #[inline]
    fn write_u128(&mut self, i: u128) {
        self.hash_in(i);
    }

    #[inline]
    fn write_usize(&mut self, i: usize) {
        self.write_u64(i as u64);
    }

    #[inline]
    fn write_u64(&mut self, i: u64) {
        self.write_u128(i as u128);
    }

    #[inline]
    fn write(&mut self, input: &[u8]) {
        let mut data = input;
        let length = data.len() as u64;
        //This will be scrambled by the first AES round in any branch.
        self.buffer[1] = self.buffer[1].wrapping_add(length);
        //A 'binary search' on sizes reduces the number of comparisons.
        if data.len() <= 8 {
            let value: [u64; 2] = if data.len() >= 2 {
                if data.len() >= 4 {
                    //len 4-8
                    [data.read_u32().0 as u64, data.read_last_u32() as u64]
                } else {
                    //len 2-3
                    [data.read_u16().0 as u64, data[data.len() - 1] as u64]
                }
            } else {
                if data.len() > 0 {
                    [data[0] as u64, 0]
                } else {
                    [0, 0]
                }
            };
            self.hash_in(value.convert());
        } else {
            if data.len() > 32 {
                if data.len() > 64 {
                    let tail = data.read_last_u128x4();
                    let mut par_block: u128 = self.buffer.convert();
                    while data.len() > 64 {
                        let (blocks, rest) = data.read_u128x4();
                        data = rest;
                        self.hash_in_2(blocks[0], blocks[1]);
                        par_block = aeshash(par_block, blocks[2]);
                        par_block = aeshashx2(par_block, blocks[3], par_block);
                    }
                    self.hash_in_2(tail[0], tail[1]);
                    par_block = aeshash(par_block, tail[2]);
                    par_block = aeshashx2(par_block, tail[3], par_block);
                    self.hash_in(par_block);
                } else {
                    //len 33-64
                    let (head, _) = data.read_u128x2();
                    let tail = data.read_last_u128x2();
                    self.hash_in_2(head[0], head[1]);
                    self.hash_in_2(tail[0], tail[1]);
                }
            } else {
                if data.len() > 16 {
                    //len 17-32
                    self.hash_in_2(data.read_u128().0, data.read_last_u128());
                } else {
                    //len 9-16
                    let value: [u64; 2] = [data.read_u64().0, data.read_last_u64()];
                    self.hash_in(value.convert());
                }
            }
        }
    }
    #[inline]
    fn finish(&self) -> u64 {
        let result: [u64; 2] = aeshash(self.buffer.convert(), self.key).convert();
        result[0] //.wrapping_add(result[1])
    }
}

#[cfg(all(any(target_arch = "x86", target_arch = "x86_64"), target_feature = "aes"))]
#[inline(always)]
fn aeshash(value: u128, xor: u128) -> u128 {
    #[cfg(target_arch = "x86")]
    use core::arch::x86::*;
    #[cfg(target_arch = "x86_64")]
    use core::arch::x86_64::*;
    use core::mem::transmute;
    unsafe {
        let value = transmute(value);
        transmute(_mm_aesdec_si128(value, transmute(xor)))
    }
}
#[cfg(all(any(target_arch = "x86", target_arch = "x86_64"), target_feature = "aes"))]
#[inline(always)]
fn aeshashx2(value: u128, k1: u128, k2: u128) -> u128 {
    #[cfg(target_arch = "x86")]
    use core::arch::x86::*;
    #[cfg(target_arch = "x86_64")]
    use core::arch::x86_64::*;
    use core::mem::transmute;
    unsafe {
        let value = transmute(value);
        let value = _mm_aesdec_si128(value, transmute(k1));
        transmute(_mm_aesdec_si128(value, transmute(k2)))
    }
}

#[cfg(test)]
mod tests {
    use crate::aes_hash::*;
    use crate::convert::Convert;
    use std::collections::HashMap;
    use std::hash::BuildHasherDefault;

    #[cfg(feature = "compile-time-rng")]
    #[test]
    fn test_builder() {
        let mut map = HashMap::<u32, u64, BuildHasherDefault<AHasher>>::default();
        map.insert(1, 3);
    }

    #[cfg(feature = "compile-time-rng")]
    #[test]
    fn test_default() {
        let hasher_a = AHasher::default();
        assert_ne!(0, hasher_a.buffer[0]);
        assert_ne!(0, hasher_a.buffer[1]);
        assert_ne!(hasher_a.buffer[0], hasher_a.buffer[1]);
        let hasher_b = AHasher::default();
        assert_eq!(hasher_a.buffer[0], hasher_b.buffer[0]);
        assert_eq!(hasher_a.buffer[1], hasher_b.buffer[1]);
    }

    #[test]
    fn test_hash() {
        let mut result: [u64; 2] = [0x6c62272e07bb0142, 0x62b821756295c58d];
        let value: [u64; 2] = [1 << 32, 0xFEDCBA9876543210];
        result = aeshash(value.convert(), result.convert()).convert();
        result = aeshash(result.convert(), result.convert()).convert();
        let mut result2: [u64; 2] = [0x6c62272e07bb0142, 0x62b821756295c58d];
        let value2: [u64; 2] = [1, 0xFEDCBA9876543210];
        result2 = aeshash(value2.convert(), result2.convert()).convert();
        result2 = aeshash(result2.convert(), result.convert()).convert();
        let result: [u8; 16] = result.convert();
        let result2: [u8; 16] = result2.convert();
        assert_ne!(hex::encode(result), hex::encode(result2));
    }

    #[test]
    fn test_conversion() {
        let input: &[u8] = "dddddddd".as_bytes();
        let bytes: u64 = as_array!(input, 8).convert();
        assert_eq!(bytes, 0x6464646464646464);
    }
}
