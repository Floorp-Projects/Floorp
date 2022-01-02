use crate::convert::*;

/// This is a constant with a lot of special properties found by automated search.
/// See the unit tests below. (Below are alternative values)
#[cfg(all(target_feature = "ssse3", not(miri)))]
const SHUFFLE_MASK: u128 = 0x020a0700_0c01030e_050f0d08_06090b04_u128;
//const SHUFFLE_MASK: u128 = 0x000d0702_0a040301_05080f0c_0e0b0609_u128;
//const SHUFFLE_MASK: u128 = 0x040A0700_030E0106_0D050F08_020B0C09_u128;

pub(crate) const fn folded_multiply(s: u64, by: u64) -> u64 {
    let result = (s as u128).wrapping_mul(by as u128);
    ((result & 0xffff_ffff_ffff_ffff) as u64) ^ ((result >> 64) as u64)
}

#[inline(always)]
pub(crate) fn shuffle(a: u128) -> u128 {
    #[cfg(all(target_feature = "ssse3", not(miri)))]
        {
            use core::mem::transmute;
            #[cfg(target_arch = "x86")]
            use core::arch::x86::*;
            #[cfg(target_arch = "x86_64")]
            use core::arch::x86_64::*;
            unsafe {
                transmute(_mm_shuffle_epi8(transmute(a), transmute(SHUFFLE_MASK)))
            }
        }
    #[cfg(not(all(target_feature = "ssse3", not(miri))))]
        {
            a.swap_bytes()
        }
}

#[allow(unused)] //not used by fallback
#[inline(always)]
pub(crate) fn add_and_shuffle(a: u128, b: u128) -> u128 {
    let sum = add_by_64s(a.convert(), b.convert());
    shuffle(sum.convert())
}

#[allow(unused)] //not used by fallbac
#[inline(always)]
pub(crate) fn shuffle_and_add(base: u128, to_add: u128) -> u128 {
    let shuffled: [u64; 2] = shuffle(base).convert();
    add_by_64s(shuffled, to_add.convert()).convert()
}

#[cfg(all(any(target_arch = "x86", target_arch = "x86_64"), target_feature = "sse2", not(miri)))]
#[inline(always)]
pub(crate) fn add_by_64s(a: [u64; 2], b: [u64; 2]) -> [u64; 2] {
    use core::mem::transmute;
    unsafe {
        #[cfg(target_arch = "x86")]
        use core::arch::x86::*;
        #[cfg(target_arch = "x86_64")]
        use core::arch::x86_64::*;
        transmute(_mm_add_epi64(transmute(a), transmute(b)))
    }
}

#[cfg(not(all(any(target_arch = "x86", target_arch = "x86_64"), target_feature = "sse2", not(miri))))]
#[inline(always)]
pub(crate) fn add_by_64s(a: [u64; 2], b: [u64; 2]) -> [u64; 2] {
    [a[0].wrapping_add(b[0]), a[1].wrapping_add(b[1])]
}

#[cfg(all(any(target_arch = "x86", target_arch = "x86_64"), target_feature = "aes", not(miri)))]
#[allow(unused)]
#[inline(always)]
pub(crate) fn aesenc(value: u128, xor: u128) -> u128 {
    #[cfg(target_arch = "x86")]
    use core::arch::x86::*;
    #[cfg(target_arch = "x86_64")]
    use core::arch::x86_64::*;
    use core::mem::transmute;
    unsafe {
        let value = transmute(value);
        transmute(_mm_aesenc_si128(value, transmute(xor)))
    }
}
#[cfg(all(any(target_arch = "x86", target_arch = "x86_64"), target_feature = "aes", not(miri)))]
#[allow(unused)]
#[inline(always)]
pub(crate) fn aesdec(value: u128, xor: u128) -> u128 {
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

#[cfg(test)]
mod test {
    use super::*;
    use crate::convert::Convert;

    // This is code to search for the shuffle constant
    //
    //thread_local! { static MASK: Cell<u128> = Cell::new(0); }
    //
    // fn shuffle(a: u128) -> u128 {
    //     use std::intrinsics::transmute;
    //     #[cfg(target_arch = "x86")]
    //     use core::arch::x86::*;
    //     #[cfg(target_arch = "x86_64")]
    //     use core::arch::x86_64::*;
    //     MASK.with(|mask| {
    //         unsafe { transmute(_mm_shuffle_epi8(transmute(a), transmute(mask.get()))) }
    //     })
    // }
    //
    // #[test]
    // fn find_shuffle() {
    //     use rand::prelude::*;
    //     use SliceRandom;
    //     use std::panic;
    //     use std::io::Write;
    //
    //     let mut value: [u8; 16] = [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 ,13, 14, 15];
    //     let mut rand = thread_rng();
    //     let mut successful_list = HashMap::new();
    //     for _attempt in 0..10000000 {
    //         rand.shuffle(&mut value);
    //         let test_val = value.convert();
    //         MASK.with(|mask| {
    //             mask.set(test_val);
    //         });
    //         if let Ok(successful) = panic::catch_unwind(|| {
    //             test_shuffle_does_not_collide_with_aes();
    //             test_shuffle_moves_high_bits();
    //             test_shuffle_moves_every_value();
    //             //test_shuffle_does_not_loop();
    //             value
    //         }) {
    //             let successful: u128 = successful.convert();
    //             successful_list.insert(successful, iters_before_loop());
    //         }
    //     }
    //     let write_file = File::create("/tmp/output").unwrap();
    //     let mut writer = BufWriter::new(&write_file);
    //
    //     for success in successful_list {
    //         writeln!(writer, "Found successful: {:x?} - {:?}", success.0, success.1);
    //     }
    // }
    //
    // fn iters_before_loop() -> u32 {
    //     let numbered = 0x00112233_44556677_8899AABB_CCDDEEFF;
    //     let mut shuffled = shuffle(numbered);
    //     let mut count = 0;
    //     loop {
    //         // println!("{:>16x}", shuffled);
    //         if numbered == shuffled {
    //             break;
    //         }
    //         count += 1;
    //         shuffled = shuffle(shuffled);
    //     }
    //     count
    // }

    #[cfg(all(
        any(target_arch = "x86", target_arch = "x86_64"),
        target_feature = "ssse3",
        target_feature = "aes",
        not(miri)
    ))]
    #[test]
    fn test_shuffle_does_not_collide_with_aes() {
        let mut value: [u8; 16] = [0; 16];
        let zero_mask_enc = aesenc(0, 0);
        let zero_mask_dec = aesdec(0, 0);
        for index in 0..16 {
            value[index] = 1;
            let excluded_positions_enc: [u8; 16] = aesenc(value.convert(), zero_mask_enc).convert();
            let excluded_positions_dec: [u8; 16] = aesdec(value.convert(), zero_mask_dec).convert();
            let actual_location: [u8; 16] = shuffle(value.convert()).convert();
            for pos in 0..16 {
                if actual_location[pos] != 0 {
                    assert_eq!(
                        0, excluded_positions_enc[pos],
                        "Forward Overlap between {:?} and {:?} at {}",
                        excluded_positions_enc, actual_location, index
                    );
                    assert_eq!(
                        0, excluded_positions_dec[pos],
                        "Reverse Overlap between {:?} and {:?} at {}",
                        excluded_positions_dec, actual_location, index
                    );
                }
            }
            value[index] = 0;
        }
    }

    #[test]
    fn test_shuffle_contains_each_value() {
        let value: [u8; 16] = 0x00010203_04050607_08090A0B_0C0D0E0F_u128.convert();
        let shuffled: [u8; 16] = shuffle(value.convert()).convert();
        for index in 0..16_u8 {
            assert!(shuffled.contains(&index), "Value is missing {}", index);
        }
    }

    #[test]
    fn test_shuffle_moves_every_value() {
        let mut value: [u8; 16] = [0; 16];
        for index in 0..16 {
            value[index] = 1;
            let shuffled: [u8; 16] = shuffle(value.convert()).convert();
            assert_eq!(0, shuffled[index], "Value is not moved {}", index);
            value[index] = 0;
        }
    }

    #[test]
    fn test_shuffle_moves_high_bits() {
        assert!(
            shuffle(1) > (1_u128 << 80),
            "Low bits must be moved to other half {:?} -> {:?}",
            0,
            shuffle(1)
        );

        assert!(
            shuffle(1_u128 << 58) >= (1_u128 << 64),
            "High bits must be moved to other half {:?} -> {:?}",
            7,
            shuffle(1_u128 << 58)
        );
        assert!(
            shuffle(1_u128 << 58) < (1_u128 << 112),
            "High bits must not remain high {:?} -> {:?}",
            7,
            shuffle(1_u128 << 58)
        );
        assert!(
            shuffle(1_u128 << 64) < (1_u128 << 64),
            "Low bits must be moved to other half {:?} -> {:?}",
            8,
            shuffle(1_u128 << 64)
        );
        assert!(
            shuffle(1_u128 << 64) >= (1_u128 << 16),
            "Low bits must not remain low {:?} -> {:?}",
            8,
            shuffle(1_u128 << 64)
        );

        assert!(
            shuffle(1_u128 << 120) < (1_u128 << 50),
            "High bits must be moved to low half {:?} -> {:?}",
            15,
            shuffle(1_u128 << 120)
        );
    }

    #[cfg(all(
        any(target_arch = "x86", target_arch = "x86_64"),
        target_feature = "ssse3",
        not(miri)
    ))]
    #[test]
    fn test_shuffle_does_not_loop() {
        let numbered = 0x00112233_44556677_8899AABB_CCDDEEFF;
        let mut shuffled = shuffle(numbered);
        for count in 0..100 {
            // println!("{:>16x}", shuffled);
            assert_ne!(numbered, shuffled, "Equal after {} vs {:x}", count, shuffled);
            shuffled = shuffle(shuffled);
        }
    }
}
