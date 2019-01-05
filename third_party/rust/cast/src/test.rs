// If `src` can be promoted to `$dst`, then it must be Ok to cast `dst` back to
// `$src`
macro_rules! promote_and_back {
    ($($src:ident => $($dst:ident),+);+;) => {
        mod demoting_to {
            $(
                mod $src {
                    mod from {
                        use From;

                        $(
                            quickcheck! {
                                fn $dst(src: $src) -> bool {
                                    $src::cast($dst::cast(src)).is_ok()
                                }
                            }
                         )+
                    }
                }
             )+
        }
    }
}

#[cfg(target_pointer_width = "32")]
promote_and_back! {
    i8    => f32, f64,     i16, i32, isize, i64                          ;
    i16   => f32, f64,          i32, isize, i64                          ;
    i32   => f32, f64,                      i64                          ;
    isize => f32, f64,                      i64                          ;
    i64   => f32, f64                                                    ;
    u8    => f32, f64,     i16, i32, isize, i64,     u16, u32, usize, u64;
    u16   => f32, f64,          i32, isize, i64,          u32, usize, u64;
    u32   => f32, f64,                      i64,                      u64;
    usize => f32, f64,                      i64,                      u64;
    u64   => f32, f64                                                    ;
}

#[cfg(target_pointer_width = "64")]
promote_and_back! {
    i8    => f32, f64,     i16, i32, i64, isize                          ;
    i16   => f32, f64,          i32, i64, isize                          ;
    i32   => f32, f64,               i64, isize                          ;
    i64   => f32, f64                                                    ;
    isize => f32, f64                                                    ;
    u8    => f32, f64,     i16, i32, i64, isize,     u16, u32, u64, usize;
    u16   => f32, f64,          i32, i64, isize,          u32, u64, usize;
    u32   => f32, f64,               i64, isize,               u64, usize;
    u64   => f32, f64                                                    ;
    usize => f32, f64                                                    ;
}

// TODO uncomment this once quickcheck supports Arbitrary for i128/u128
// https://github.com/BurntSushi/quickcheck/issues/162
/*#[cfg(feature = "x128")]
promote_and_back! {
    i8    =>           i128      ;
    i16   =>           i128      ;
    i32   =>           i128      ;
    isize =>           i128      ;
    i64   =>           i128      ;
    i128  => f32, f64            ;
    u8    =>           i128, u128;
    u16   =>           i128, u128;
    u32   =>           i128, u128;
    usize =>           i128, u128;
    u64   =>           i128, u128;
    u128  => f32, f64            ;
}*/

// If it's Ok to cast `src` to `$dst`, it must also be Ok to cast `dst` back to
// `$src`
macro_rules! symmetric_cast_between {
    ($($src:ident => $($dst:ident),+);+;) => {
        mod symmetric_cast_between {
            $(
                mod $src {
                    mod and {
                        use quickcheck::TestResult;

                        use From;

                        $(
                            quickcheck! {
                                fn $dst(src: $src) -> TestResult {
                                    if let Ok(dst) = $dst::cast(src) {
                                        TestResult::from_bool(
                                            $src::cast(dst).is_ok())
                                    } else {
                                        TestResult::discard()
                                    }
                                }
                            }
                         )+
                    }
                }
             )+
        }
    }
}

#[cfg(target_pointer_width = "32")]
symmetric_cast_between! {
    u8    =>           i8                      ;
    u16   =>           i8, i16                 ;
    u32   =>           i8, i16, i32            ;
    usize =>           i8, i16, i32            ;
    u64   =>           i8, i16, i32, i64, isize;
}

#[cfg(target_pointer_width = "64")]
symmetric_cast_between! {
    u8    =>           i8                      ;
    u16   =>           i8, i16                 ;
    u32   =>           i8, i16, i32            ;
    u64   =>           i8, i16, i32, i64, isize;
    usize =>           i8, i16, i32, i64, isize;
}

// TODO uncomment this once quickcheck supports Arbitrary for i128/u128
// https://github.com/BurntSushi/quickcheck/issues/162
/*#[cfg(feature = "x128")]
symmetric_cast_between! {
    u128  => i8, i16, i32, isize, i64, i128;
}*/

macro_rules! from_float {
    ($($src:ident => $($dst:ident),+);+;) => {
        $(
            mod $src {
                mod inf {
                    mod to {
                        use {Error, From};

                        $(
                            #[test]
                            fn $dst() {
                                let _0: $src = 0.;
                                let _1: $src = 1.;
                                let inf = _1 / _0;
                                let neg_inf = -_1 / _0;

                                assert_eq!($dst::cast(inf),
                                           Err(Error::Infinite));
                                assert_eq!($dst::cast(neg_inf),
                                           Err(Error::Infinite));
                            }
                         )+
                    }
                }

                mod nan {
                    mod to {
                        use {Error, From};

                        $(
                            #[test]
                            fn $dst() {
                                let _0: $src = 0.;
                                let nan = _0 / _0;

                                assert_eq!($dst::cast(nan),
                                           Err(Error::NaN));
                            }
                         )+
                    }
                }
            }
         )+
    }
}

from_float! {
    f32 => i8, i16, i32, i64, isize, u8, u16, u32, u64, usize;
    f64 => i8, i16, i32, i64, isize, u8, u16, u32, u64, usize;
}

// TODO uncomment this once quickcheck supports Arbitrary for i128/u128
// https://github.com/BurntSushi/quickcheck/issues/162
/*#[cfg(feature = "x128")]
from_float! {
    f32 => i128, u128;
    f64 => i128, u128;
}*/

#[test]
#[cfg(feature = "x128")]
fn test_fl_conversion() {
    use u128;
    assert_eq!(u128(42.0f32), Ok(42));
}
