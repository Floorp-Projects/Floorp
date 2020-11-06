use crate::guards::UninitializedSliceMemoryGuard;
use core::mem::MaybeUninit;

/// This trait is a extended copy of unstable
/// [core::array::FixedSizeArray](core::array::FixedSizeArray).
///
/// This is not a perfect solution. Inheritance from `AsRef<[T]> + AsMut<[T]>` would be preferable.
/// But until we cannot implement `std` traits for `std` types so that inheritance limits us
/// and we cannot use `[T; n]` where `n > 32`.
pub trait FixedArray {
    type Item;
    const LEN: usize;
    fn as_slice(&self) -> &[Self::Item];
    fn as_slice_mut(&mut self) -> &mut [Self::Item];
}

macro_rules! impl_fixed_array_for_array {
    ($($length: expr),+) => {
        $(
            impl<T> FixedArray for [T; $length] {
                type Item = T;
                const LEN: usize = $length;
                #[inline]
                fn as_slice(&self) -> &[Self::Item] {
                    self
                }
                #[inline]
                fn as_slice_mut(&mut self) -> &mut [Self::Item] {
                    self
                }
            }
        )+
    };
}

macro_rules! impl_fixed_array_for_array_group_32 {
    ($($length: expr),+) => {
        $(
            impl_fixed_array_for_array!(
                $length, $length + 1, $length + 2, $length + 3,
                $length + 4, $length + 5, $length + 6, $length + 7,
                $length + 8, $length + 9, $length + 10, $length + 11,
                $length + 12, $length + 13, $length + 14, $length + 15,
                $length + 16, $length + 17, $length + 18, $length + 19,
                $length + 20, $length + 21, $length + 22, $length + 23,
                $length + 24, $length + 25, $length + 26, $length + 27,
                $length + 28, $length + 29, $length + 30, $length + 31
            );
        )+
    };
}

impl_fixed_array_for_array_group_32!(0, 32, 64, 96);

#[cfg(not(target_pointer_width = "8"))]
impl_fixed_array_for_array!(
    128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448, 480, 512, 544, 576, 608, 640, 672, 704,
    736, 768, 800, 832, 864, 896, 928, 960, 992, 1024, 1056, 1088, 1120, 1152, 1184, 1216, 1248,
    1280, 1312, 1344, 1376, 1408, 1440, 1472, 1504, 1536, 1568, 1600, 1632, 1664, 1696, 1728, 1760,
    1792, 1824, 1856, 1888, 1920, 1952, 1984, 2016, 2048, 2080, 2112, 2144, 2176, 2208, 2240, 2272,
    2304, 2336, 2368, 2400, 2432, 2464, 2496, 2528, 2560, 2592, 2624, 2656, 2688, 2720, 2752, 2784,
    2816, 2848, 2880, 2912, 2944, 2976, 3008, 3040, 3072, 3104, 3136, 3168, 3200, 3232, 3264, 3296,
    3328, 3360, 3392, 3424, 3456, 3488, 3520, 3552, 3584, 3616, 3648, 3680, 3712, 3744, 3776, 3808,
    3840, 3872, 3904, 3936, 3968, 4000, 4032, 4064, 4096
);

/// `try_inplace_array` trying to place an array of `T` on the stack and pass the guard of memory into the
/// `consumer` closure. `consumer`'s result will be returned as `Ok(result)`.
///
/// If the result of array of `T` is more than 4096 then `Err(consumer)` will be returned.
///
/// Sometimes size of allocated array might be more than requested. For sizes larger than 32,
/// the following formula is used: `roundUp(size/32)*32`. This is a simplification that used
/// for keeping code short, simple and able to optimize.
/// For example, for requested 50 item `[T; 64]` will be allocated.
/// For 120 items - `[T; 128]` and so on.
///
/// Note that rounding size up is working for fixed-sized arrays only. If function decides to
/// allocate a vector then its size will be equal to requested.
///
/// # Examples
///
/// ```rust
/// use inplace_it::{
///     try_inplace_array,
///     UninitializedSliceMemoryGuard,
/// };
///
/// let sum = try_inplace_array(100, |uninit_guard: UninitializedSliceMemoryGuard<u16>| {
///     assert_eq!(uninit_guard.len(), 128);
///     // For now, our memory is placed/allocated but uninitialized.
///     // Let's initialize it!
///     let guard = uninit_guard.init(|index| index as u16 * 2);
///     // For now, memory contains content like [0, 2, 4, 6, ..., 252, 254]
///     let sum: u16 = guard.iter().sum();
///     sum
/// });
/// // Sum of [0, 2, 4, 6, ..., 252, 254] = sum of [0, 1, 2, 3, ..., 126, 127] * 2 = ( 127 * (127+1) ) / 2 * 2
/// match sum {
///     Ok(sum) => assert_eq!(sum, 127 * 128),
///     Err(_) => unreachable!("Placing fails"),
/// };
/// ```
pub fn try_inplace_array<T, R, Consumer>(size: usize, consumer: Consumer) -> Result<R, Consumer>
    where Consumer: FnOnce(UninitializedSliceMemoryGuard<T>) -> R
{
    macro_rules! inplace {
        ($size: expr) => {unsafe {
            indirect(move || {
                let mut memory: [MaybeUninit<T>; $size] = MaybeUninit::uninit().assume_init();
                consumer(UninitializedSliceMemoryGuard::new(&mut memory))
            })
        }};
    }
    #[cfg(target_pointer_width = "8")]
    let result = match size {
        0 => inplace!(0),
        1 => inplace!(1),
        2 => inplace!(2),
        3 => inplace!(3),
        4 => inplace!(4),
        5 => inplace!(5),
        6 => inplace!(6),
        7 => inplace!(7),
        8 => inplace!(8),
        9 => inplace!(9),
        10 => inplace!(10),
        11 => inplace!(11),
        12 => inplace!(12),
        13 => inplace!(13),
        14 => inplace!(14),
        15 => inplace!(15),
        16 => inplace!(16),
        17 => inplace!(17),
        18 => inplace!(18),
        19 => inplace!(19),
        20 => inplace!(20),
        21 => inplace!(21),
        22 => inplace!(22),
        23 => inplace!(23),
        24 => inplace!(24),
        25 => inplace!(25),
        26 => inplace!(26),
        27 => inplace!(27),
        28 => inplace!(28),
        29 => inplace!(29),
        30 => inplace!(30),
        31 => inplace!(31),
        32 => inplace!(32),
        33..=64 => inplace!(64),
        65..=96 => inplace!(96),
        97..=127 => inplace!(127),
        _ => return Err(consumer),
    };
    #[cfg(not(target_pointer_width = "8"))]
    let result = match size {
        0 => inplace!(0),
        1 => inplace!(1),
        2 => inplace!(2),
        3 => inplace!(3),
        4 => inplace!(4),
        5 => inplace!(5),
        6 => inplace!(6),
        7 => inplace!(7),
        8 => inplace!(8),
        9 => inplace!(9),
        10 => inplace!(10),
        11 => inplace!(11),
        12 => inplace!(12),
        13 => inplace!(13),
        14 => inplace!(14),
        15 => inplace!(15),
        16 => inplace!(16),
        17 => inplace!(17),
        18 => inplace!(18),
        19 => inplace!(19),
        20 => inplace!(20),
        21 => inplace!(21),
        22 => inplace!(22),
        23 => inplace!(23),
        24 => inplace!(24),
        25 => inplace!(25),
        26 => inplace!(26),
        27 => inplace!(27),
        28 => inplace!(28),
        29 => inplace!(29),
        30 => inplace!(30),
        31 => inplace!(31),
        32 => inplace!(32),
        33..=64 => inplace!(64),
        65..=96 => inplace!(96),
        97..=128 => inplace!(128),
        129..=160 => inplace!(160),
        161..=192 => inplace!(192),
        193..=224 => inplace!(224),
        225..=256 => inplace!(256),
        257..=288 => inplace!(288),
        289..=320 => inplace!(320),
        321..=352 => inplace!(352),
        353..=384 => inplace!(384),
        385..=416 => inplace!(416),
        417..=448 => inplace!(448),
        449..=480 => inplace!(480),
        481..=512 => inplace!(512),
        513..=544 => inplace!(544),
        545..=576 => inplace!(576),
        577..=608 => inplace!(608),
        609..=640 => inplace!(640),
        641..=672 => inplace!(672),
        673..=704 => inplace!(704),
        705..=736 => inplace!(736),
        737..=768 => inplace!(768),
        769..=800 => inplace!(800),
        801..=832 => inplace!(832),
        833..=864 => inplace!(864),
        865..=896 => inplace!(896),
        897..=928 => inplace!(928),
        929..=960 => inplace!(960),
        961..=992 => inplace!(992),
        993..=1024 => inplace!(1024),
        1025..=1056 => inplace!(1056),
        1057..=1088 => inplace!(1088),
        1089..=1120 => inplace!(1120),
        1121..=1152 => inplace!(1152),
        1153..=1184 => inplace!(1184),
        1185..=1216 => inplace!(1216),
        1217..=1248 => inplace!(1248),
        1249..=1280 => inplace!(1280),
        1281..=1312 => inplace!(1312),
        1313..=1344 => inplace!(1344),
        1345..=1376 => inplace!(1376),
        1377..=1408 => inplace!(1408),
        1409..=1440 => inplace!(1440),
        1441..=1472 => inplace!(1472),
        1473..=1504 => inplace!(1504),
        1505..=1536 => inplace!(1536),
        1537..=1568 => inplace!(1568),
        1569..=1600 => inplace!(1600),
        1601..=1632 => inplace!(1632),
        1633..=1664 => inplace!(1664),
        1665..=1696 => inplace!(1696),
        1697..=1728 => inplace!(1728),
        1729..=1760 => inplace!(1760),
        1761..=1792 => inplace!(1792),
        1793..=1824 => inplace!(1824),
        1825..=1856 => inplace!(1856),
        1857..=1888 => inplace!(1888),
        1889..=1920 => inplace!(1920),
        1921..=1952 => inplace!(1952),
        1953..=1984 => inplace!(1984),
        1985..=2016 => inplace!(2016),
        2017..=2048 => inplace!(2048),
        2049..=2080 => inplace!(2080),
        2081..=2112 => inplace!(2112),
        2113..=2144 => inplace!(2144),
        2145..=2176 => inplace!(2176),
        2177..=2208 => inplace!(2208),
        2209..=2240 => inplace!(2240),
        2241..=2272 => inplace!(2272),
        2273..=2304 => inplace!(2304),
        2305..=2336 => inplace!(2336),
        2337..=2368 => inplace!(2368),
        2369..=2400 => inplace!(2400),
        2401..=2432 => inplace!(2432),
        2433..=2464 => inplace!(2464),
        2465..=2496 => inplace!(2496),
        2497..=2528 => inplace!(2528),
        2529..=2560 => inplace!(2560),
        2561..=2592 => inplace!(2592),
        2593..=2624 => inplace!(2624),
        2625..=2656 => inplace!(2656),
        2657..=2688 => inplace!(2688),
        2689..=2720 => inplace!(2720),
        2721..=2752 => inplace!(2752),
        2753..=2784 => inplace!(2784),
        2785..=2816 => inplace!(2816),
        2817..=2848 => inplace!(2848),
        2849..=2880 => inplace!(2880),
        2881..=2912 => inplace!(2912),
        2913..=2944 => inplace!(2944),
        2945..=2976 => inplace!(2976),
        2977..=3008 => inplace!(3008),
        3009..=3040 => inplace!(3040),
        3041..=3072 => inplace!(3072),
        3073..=3104 => inplace!(3104),
        3105..=3136 => inplace!(3136),
        3137..=3168 => inplace!(3168),
        3169..=3200 => inplace!(3200),
        3201..=3232 => inplace!(3232),
        3233..=3264 => inplace!(3264),
        3265..=3296 => inplace!(3296),
        3297..=3328 => inplace!(3328),
        3329..=3360 => inplace!(3360),
        3361..=3392 => inplace!(3392),
        3393..=3424 => inplace!(3424),
        3425..=3456 => inplace!(3456),
        3457..=3488 => inplace!(3488),
        3489..=3520 => inplace!(3520),
        3521..=3552 => inplace!(3552),
        3553..=3584 => inplace!(3584),
        3585..=3616 => inplace!(3616),
        3617..=3648 => inplace!(3648),
        3649..=3680 => inplace!(3680),
        3681..=3712 => inplace!(3712),
        3713..=3744 => inplace!(3744),
        3745..=3776 => inplace!(3776),
        3777..=3808 => inplace!(3808),
        3809..=3840 => inplace!(3840),
        3841..=3872 => inplace!(3872),
        3873..=3904 => inplace!(3904),
        3905..=3936 => inplace!(3936),
        3937..=3968 => inplace!(3968),
        3969..=4000 => inplace!(4000),
        4001..=4032 => inplace!(4032),
        4033..=4064 => inplace!(4064),
        4065..=4096 => inplace!(4096),
        _ => return Err(consumer),
    };
    Ok(result)
}

#[inline(never)]
fn indirect<R>(fun: impl FnOnce() -> R) -> R {
    fun()
}
