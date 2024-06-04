//! Conversion between units of time.

use self::sealed::Per;

mod sealed {
    /// A trait for defining the ratio of two units of time.
    ///
    /// This trait is used to implement the `per` method on the various structs.
    pub trait Per<T> {
        /// The smallest unsigned integer type that can represent [`VALUE`](Self::VALUE).
        type Output;

        /// The number of one unit of time in the other.
        const VALUE: Self::Output;
    }
}

/// Declare and implement `Per` for all relevant types. Identity implementations are automatic.
macro_rules! impl_per {
    ($($t:ident ($str:literal) per {$(
        $larger:ident : $output:ty = $value:expr
    )*})*) => {$(
        #[doc = concat!("A unit of time representing exactly one ", $str, ".")]
        #[derive(Debug, Clone, Copy)]
        pub struct $t;

        impl $t {
            #[doc = concat!("Obtain the number of times `", stringify!($t), "` can fit into `T`.")]
            #[doc = concat!("If `T` is smaller than `", stringify!($t), "`, the code will fail to")]
            /// compile. The return type is the smallest unsigned integer type that can represent
            /// the value.
            ///
            /// Valid calls:
            ///
            #[doc = concat!("  - `", stringify!($t), "::per(", stringify!($t), ")` (returns `u8`)")]
            $(#[doc = concat!("  - `", stringify!($t), "::per(", stringify!($larger), ")` (returns `", stringify!($output), "`)")])*
            pub const fn per<T>(_larger: T) -> <Self as Per<T>>::Output
            where
                Self: Per<T>,
                T: Copy,
            {
                Self::VALUE
            }
        }

        impl Per<$t> for $t {
            type Output = u8;

            const VALUE: u8 = 1;
        }

        $(impl Per<$larger> for $t {
            type Output = $output;

            const VALUE: $output = $value;
        })*
    )*};
}

impl_per! {
    Nanosecond ("nanosecond") per {
        Microsecond: u16 = 1_000
        Millisecond: u32 = 1_000_000
        Second: u32 = 1_000_000_000
        Minute: u64 = 60_000_000_000
        Hour: u64 = 3_600_000_000_000
        Day: u64 = 86_400_000_000_000
        Week: u64 = 604_800_000_000_000
    }
    Microsecond ("microsecond") per {
        Millisecond: u16 = 1_000
        Second: u32 = 1_000_000
        Minute: u32 = 60_000_000
        Hour: u32 = 3_600_000_000
        Day: u64 = 86_400_000_000
        Week: u64 = 604_800_000_000
    }
    Millisecond ("millisecond") per {
        Second: u16 = 1_000
        Minute: u16 = 60_000
        Hour: u32 = 3_600_000
        Day: u32 = 86_400_000
        Week: u32 = 604_800_000
    }
    Second ("second") per {
        Minute: u8 = 60
        Hour: u16 = 3_600
        Day: u32 = 86_400
        Week: u32 = 604_800
    }
    Minute ("minute") per {
        Hour: u8 = 60
        Day: u16 = 1_440
        Week: u16 = 10_080
    }
    Hour ("hour") per {
        Day: u8 = 24
        Week: u8 = 168
    }
    Day ("day") per {
        Week: u8 = 7
    }
    Week ("week") per {}
}
