macro_rules! doit_signed {
    ($($Algo:ident),*) => ($(
        impl $Algo<i8> for i8 {
            #[inline]
            fn swap_bits(self) -> i8 {
                $Algo::swap_bits(self as u8) as i8
            }
        }

        impl $Algo<i16> for i16 {
            #[inline]
            fn swap_bits(self) -> i16 {
                $Algo::swap_bits(self as u16) as i16
            }
        }

        impl $Algo<i32> for i32 {
            #[inline]
            fn swap_bits(self) -> i32 {
                $Algo::swap_bits(self as u32) as i32
            }
        }

        impl $Algo<i64> for i64 {
            #[inline]
            fn swap_bits(self) -> i64 {
                $Algo::swap_bits(self as u64) as i64
            }
        }

        impl $Algo<isize> for isize {
            #[inline]
            fn swap_bits(self) -> isize {
                $Algo::swap_bits(self as usize) as isize
            }
        }
    )*)
}

macro_rules! test_suite {
    () => (
        #[test]
        fn reverse_u8() {
            assert_eq!(0xABu8.swap_bits(), 0xD5u8);
        }

        #[test]
        fn reverse_u16() {
            assert_eq!(0xABCDu16.swap_bits(), 0xB3D5u16);
        }

        #[test]
        fn reverse_u32() {
            assert_eq!(0xABCD2345u32.swap_bits(), 0xA2C4B3D5u32);
        }

        #[test]
        fn reverse_u64() {
            assert_eq!(0x0123456789ABCDEFu64.swap_bits(), 0xF7B3D591E6A2C480u64);
        }

        #[test]
        fn reverse_usize() {
            assert_eq!(0xFFusize.swap_bits(), 0xFFusize.swap_bytes());
        }

        #[test]
        fn reverse_i8() {
            assert_eq!(0xABi8.swap_bits(), 0xD5i8);
        }

        #[test]
        fn reverse_i16() {
            assert_eq!(0xABCDi16.swap_bits(), 0xB3D5i16);
        }

        #[test]
        fn reverse_i32() {
            assert_eq!(0xABCD2345i32.swap_bits(), 0xA2C4B3D5i32);
        }

        #[test]
        fn reverse_i64() {
            assert_eq!(0x0123456789ABCDEFi64.swap_bits(), 0xF7B3D591E6A2C480i64);
        }

        #[test]
        fn reverse_isize() {
            assert_eq!(0xFFisize.swap_bits(), 0xFFisize.swap_bytes());
        }
    )
}