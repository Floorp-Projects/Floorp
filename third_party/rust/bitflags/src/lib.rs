// Copyright 2014 The Rust Project Developers. See the COPYRIGHT
// file at the top-level directory of this distribution and at
// http://rust-lang.org/COPYRIGHT.
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

//! A typesafe bitmask flag generator.

#![no_std]

#[cfg(test)]
#[macro_use]
extern crate std;

// Re-export libstd/libcore using an alias so that the macros can work in no_std
// crates while remaining compatible with normal crates.
#[allow(private_in_public)]
#[doc(hidden)]
pub use core as __core;

/// The `bitflags!` macro generates a `struct` that holds a set of C-style
/// bitmask flags. It is useful for creating typesafe wrappers for C APIs.
///
/// The flags should only be defined for integer types, otherwise unexpected
/// type errors may occur at compile time.
///
/// # Example
///
/// ```{.rust}
/// #[macro_use]
/// extern crate bitflags;
///
/// bitflags! {
///     flags Flags: u32 {
///         const FLAG_A       = 0b00000001,
///         const FLAG_B       = 0b00000010,
///         const FLAG_C       = 0b00000100,
///         const FLAG_ABC     = FLAG_A.bits
///                            | FLAG_B.bits
///                            | FLAG_C.bits,
///     }
/// }
///
/// fn main() {
///     let e1 = FLAG_A | FLAG_C;
///     let e2 = FLAG_B | FLAG_C;
///     assert_eq!((e1 | e2), FLAG_ABC);   // union
///     assert_eq!((e1 & e2), FLAG_C);     // intersection
///     assert_eq!((e1 - e2), FLAG_A);     // set difference
///     assert_eq!(!e2, FLAG_A);           // set complement
/// }
/// ```
///
/// The generated `struct`s can also be extended with type and trait
/// implementations:
///
/// ```{.rust}
/// #[macro_use]
/// extern crate bitflags;
///
/// use std::fmt;
///
/// bitflags! {
///     flags Flags: u32 {
///         const FLAG_A   = 0b00000001,
///         const FLAG_B   = 0b00000010,
///     }
/// }
///
/// impl Flags {
///     pub fn clear(&mut self) {
///         self.bits = 0;  // The `bits` field can be accessed from within the
///                         // same module where the `bitflags!` macro was invoked.
///     }
/// }
///
/// impl fmt::Display for Flags {
///     fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
///         write!(f, "hi!")
///     }
/// }
///
/// fn main() {
///     let mut flags = FLAG_A | FLAG_B;
///     flags.clear();
///     assert!(flags.is_empty());
///     assert_eq!(format!("{}", flags), "hi!");
///     assert_eq!(format!("{:?}", FLAG_A | FLAG_B), "FLAG_A | FLAG_B");
///     assert_eq!(format!("{:?}", FLAG_B), "FLAG_B");
/// }
/// ```
///
/// # Visibility
///
/// The generated struct and its associated flag constants are not exported
/// out of the current module by default. A definition can be exported out of
/// the current module by adding `pub` before `flags`:
///
/// ```{.rust},ignore
/// #[macro_use]
/// extern crate bitflags;
///
/// mod example {
///     bitflags! {
///         pub flags Flags1: u32 {
///             const FLAG_A   = 0b00000001,
///         }
///     }
///     bitflags! {
///         flags Flags2: u32 {
///             const FLAG_B   = 0b00000010,
///         }
///     }
/// }
///
/// fn main() {
///     let flag1 = example::FLAG_A;
///     let flag2 = example::FLAG_B; // error: const `FLAG_B` is private
/// }
/// ```
///
/// # Attributes
///
/// Attributes can be attached to the generated `struct` by placing them
/// before the `flags` keyword.
///
/// # Trait implementations
///
/// The `Copy`, `Clone`, `PartialEq`, `Eq`, `PartialOrd`, `Ord` and `Hash`
/// traits automatically derived for the `struct` using the `derive` attribute.
/// Additional traits can be derived by providing an explicit `derive`
/// attribute on `flags`.
///
/// The `Extend` and `FromIterator` traits are implemented for the `struct`,
/// too: `Extend` adds the union of the instances of the `struct` iterated over,
/// while `FromIterator` calculates the union.
///
/// The `Debug` trait is also implemented by displaying the bits value of the
/// internal struct.
///
/// ## Operators
///
/// The following operator traits are implemented for the generated `struct`:
///
/// - `BitOr` and `BitOrAssign`: union
/// - `BitAnd` and `BitAndAssign`: intersection
/// - `BitXor` and `BitXorAssign`: toggle
/// - `Sub` and `SubAssign`: set difference
/// - `Not`: set complement
///
/// As long as the assignment operators are unstable rust feature they are only
/// available with the crate feature `assignment_ops` enabled.
///
/// # Methods
///
/// The following methods are defined for the generated `struct`:
///
/// - `empty`: an empty set of flags
/// - `all`: the set of all flags
/// - `bits`: the raw value of the flags currently stored
/// - `from_bits`: convert from underlying bit representation, unless that
///                representation contains bits that do not correspond to a flag
/// - `from_bits_truncate`: convert from underlying bit representation, dropping
///                         any bits that do not correspond to flags
/// - `is_empty`: `true` if no flags are currently stored
/// - `is_all`: `true` if all flags are currently set
/// - `intersects`: `true` if there are flags common to both `self` and `other`
/// - `contains`: `true` all of the flags in `other` are contained within `self`
/// - `insert`: inserts the specified flags in-place
/// - `remove`: removes the specified flags in-place
/// - `toggle`: the specified flags will be inserted if not present, and removed
///             if they are.
#[macro_export]
macro_rules! bitflags {
    ($(#[$attr:meta])* pub flags $BitFlags:ident: $T:ty {
        $($(#[$Flag_attr:meta])* const $Flag:ident = $value:expr),+
    }) => {
        #[derive(Copy, PartialEq, Eq, Clone, PartialOrd, Ord, Hash)]
        $(#[$attr])*
        pub struct $BitFlags {
            bits: $T,
        }

        $($(#[$Flag_attr])* pub const $Flag: $BitFlags = $BitFlags { bits: $value };)+

        bitflags! {
            @_impl flags $BitFlags: $T {
                $($(#[$Flag_attr])* const $Flag = $value),+
            }
        }
    };
    ($(#[$attr:meta])* flags $BitFlags:ident: $T:ty {
        $($(#[$Flag_attr:meta])* const $Flag:ident = $value:expr),+
    }) => {
        #[derive(Copy, PartialEq, Eq, Clone, PartialOrd, Ord, Hash)]
        $(#[$attr])*
        struct $BitFlags {
            bits: $T,
        }

        $($(#[$Flag_attr])* const $Flag: $BitFlags = $BitFlags { bits: $value };)+

        bitflags! {
            @_impl flags $BitFlags: $T {
                $($(#[$Flag_attr])* const $Flag = $value),+
            }
        }
    };
    (@_impl flags $BitFlags:ident: $T:ty {
        $($(#[$Flag_attr:meta])* const $Flag:ident = $value:expr),+
    }) => {
        impl $crate::__core::fmt::Debug for $BitFlags {
            fn fmt(&self, f: &mut $crate::__core::fmt::Formatter) -> $crate::__core::fmt::Result {
                // This convoluted approach is to handle #[cfg]-based flag
                // omission correctly. Some of the $Flag variants may not be
                // defined in this module so we create an inner module which
                // defines *all* flags to the value of 0. We then create a
                // second inner module that defines all of the flags with #[cfg]
                // to their real values. Afterwards the glob will import
                // variants from the second inner module, shadowing all
                // defined variants, leaving only the undefined ones with the
                // bit value of 0.
                #[allow(dead_code)]
                #[allow(unused_assignments)]
                mod dummy {
                    // We can't use the real $BitFlags struct because it may be
                    // private, which prevents us from using it to define
                    // public constants.
                    pub struct $BitFlags {
                        bits: u64,
                    }
                    mod real_flags {
                        use super::$BitFlags;
                        $($(#[$Flag_attr])* pub const $Flag: $BitFlags = $BitFlags {
                            bits: super::super::$Flag.bits as u64
                        };)+
                    }
                    // Now we define the "undefined" versions of the flags.
                    // This way, all the names exist, even if some are #[cfg]ed
                    // out.
                    $(const $Flag: $BitFlags = $BitFlags { bits: 0 };)+

                    #[inline]
                    pub fn fmt(self_: u64,
                               f: &mut $crate::__core::fmt::Formatter)
                               -> $crate::__core::fmt::Result {
                        // Now we import the real values for the flags.
                        // Only ones that are #[cfg]ed out will be 0.
                        use self::real_flags::*;

                        let mut first = true;
                        $(
                            // $Flag.bits == 0 means that $Flag doesn't exist
                            if $Flag.bits != 0 && self_ & $Flag.bits as u64 == $Flag.bits as u64 {
                                if !first {
                                    try!(f.write_str(" | "));
                                }
                                first = false;
                                try!(f.write_str(stringify!($Flag)));
                            }
                        )+
                        Ok(())
                    }
                }
                dummy::fmt(self.bits as u64, f)
            }
        }

        #[allow(dead_code)]
        impl $BitFlags {
            /// Returns an empty set of flags.
            #[inline]
            pub fn empty() -> $BitFlags {
                $BitFlags { bits: 0 }
            }

            /// Returns the set containing all flags.
            #[inline]
            pub fn all() -> $BitFlags {
                // See above `dummy` module for why this approach is taken.
                #[allow(dead_code)]
                mod dummy {
                    pub struct $BitFlags {
                        bits: u64,
                    }
                    mod real_flags {
                        use super::$BitFlags;
                        $($(#[$Flag_attr])* pub const $Flag: $BitFlags = $BitFlags {
                            bits: super::super::$Flag.bits as u64
                        };)+
                    }
                    $(const $Flag: $BitFlags = $BitFlags { bits: 0 };)+

                    #[inline]
                    pub fn all() -> u64 {
                        use self::real_flags::*;
                        $($Flag.bits)|+
                    }
                }
                $BitFlags { bits: dummy::all() as $T }
            }

            /// Returns the raw value of the flags currently stored.
            #[inline]
            pub fn bits(&self) -> $T {
                self.bits
            }

            /// Convert from underlying bit representation, unless that
            /// representation contains bits that do not correspond to a flag.
            #[inline]
            pub fn from_bits(bits: $T) -> $crate::__core::option::Option<$BitFlags> {
                if (bits & !$BitFlags::all().bits()) == 0 {
                    $crate::__core::option::Option::Some($BitFlags { bits: bits })
                } else {
                    $crate::__core::option::Option::None
                }
            }

            /// Convert from underlying bit representation, dropping any bits
            /// that do not correspond to flags.
            #[inline]
            pub fn from_bits_truncate(bits: $T) -> $BitFlags {
                $BitFlags { bits: bits } & $BitFlags::all()
            }

            /// Returns `true` if no flags are currently stored.
            #[inline]
            pub fn is_empty(&self) -> bool {
                *self == $BitFlags::empty()
            }

            /// Returns `true` if all flags are currently set.
            #[inline]
            pub fn is_all(&self) -> bool {
                *self == $BitFlags::all()
            }

            /// Returns `true` if there are flags common to both `self` and `other`.
            #[inline]
            pub fn intersects(&self, other: $BitFlags) -> bool {
                !(*self & other).is_empty()
            }

            /// Returns `true` all of the flags in `other` are contained within `self`.
            #[inline]
            pub fn contains(&self, other: $BitFlags) -> bool {
                (*self & other) == other
            }

            /// Inserts the specified flags in-place.
            #[inline]
            pub fn insert(&mut self, other: $BitFlags) {
                self.bits |= other.bits;
            }

            /// Removes the specified flags in-place.
            #[inline]
            pub fn remove(&mut self, other: $BitFlags) {
                self.bits &= !other.bits;
            }

            /// Toggles the specified flags in-place.
            #[inline]
            pub fn toggle(&mut self, other: $BitFlags) {
                self.bits ^= other.bits;
            }
        }

        impl $crate::__core::ops::BitOr for $BitFlags {
            type Output = $BitFlags;

            /// Returns the union of the two sets of flags.
            #[inline]
            fn bitor(self, other: $BitFlags) -> $BitFlags {
                $BitFlags { bits: self.bits | other.bits }
            }
        }

        impl $crate::__core::ops::BitOrAssign for $BitFlags {

            /// Adds the set of flags.
            #[inline]
            fn bitor_assign(&mut self, other: $BitFlags) {
                self.bits |= other.bits;
            }
        }

        impl $crate::__core::ops::BitXor for $BitFlags {
            type Output = $BitFlags;

            /// Returns the left flags, but with all the right flags toggled.
            #[inline]
            fn bitxor(self, other: $BitFlags) -> $BitFlags {
                $BitFlags { bits: self.bits ^ other.bits }
            }
        }

        impl $crate::__core::ops::BitXorAssign for $BitFlags {

            /// Toggles the set of flags.
            #[inline]
            fn bitxor_assign(&mut self, other: $BitFlags) {
                self.bits ^= other.bits;
            }
        }

        impl $crate::__core::ops::BitAnd for $BitFlags {
            type Output = $BitFlags;

            /// Returns the intersection between the two sets of flags.
            #[inline]
            fn bitand(self, other: $BitFlags) -> $BitFlags {
                $BitFlags { bits: self.bits & other.bits }
            }
        }

        impl $crate::__core::ops::BitAndAssign for $BitFlags {

            /// Disables all flags disabled in the set.
            #[inline]
            fn bitand_assign(&mut self, other: $BitFlags) {
                self.bits &= other.bits;
            }
        }

        impl $crate::__core::ops::Sub for $BitFlags {
            type Output = $BitFlags;

            /// Returns the set difference of the two sets of flags.
            #[inline]
            fn sub(self, other: $BitFlags) -> $BitFlags {
                $BitFlags { bits: self.bits & !other.bits }
            }
        }

        impl $crate::__core::ops::SubAssign for $BitFlags {

            /// Disables all flags enabled in the set.
            #[inline]
            fn sub_assign(&mut self, other: $BitFlags) {
                self.bits &= !other.bits;
            }
        }

        impl $crate::__core::ops::Not for $BitFlags {
            type Output = $BitFlags;

            /// Returns the complement of this set of flags.
            #[inline]
            fn not(self) -> $BitFlags {
                $BitFlags { bits: !self.bits } & $BitFlags::all()
            }
        }

        impl $crate::__core::iter::Extend<$BitFlags> for $BitFlags {
            fn extend<T: $crate::__core::iter::IntoIterator<Item=$BitFlags>>(&mut self, iterator: T) {
                for item in iterator {
                    self.insert(item)
                }
            }
        }

        impl $crate::__core::iter::FromIterator<$BitFlags> for $BitFlags {
            fn from_iter<T: $crate::__core::iter::IntoIterator<Item=$BitFlags>>(iterator: T) -> $BitFlags {
                let mut result = Self::empty();
                result.extend(iterator);
                result
            }
        }
    };
    ($(#[$attr:meta])* pub flags $BitFlags:ident: $T:ty {
        $($(#[$Flag_attr:meta])* const $Flag:ident = $value:expr),+,
    }) => {
        bitflags! {
            $(#[$attr])*
            pub flags $BitFlags: $T {
                $($(#[$Flag_attr])* const $Flag = $value),+
            }
        }
    };
    ($(#[$attr:meta])* flags $BitFlags:ident: $T:ty {
        $($(#[$Flag_attr:meta])* const $Flag:ident = $value:expr),+,
    }) => {
        bitflags! {
            $(#[$attr])*
            flags $BitFlags: $T {
                $($(#[$Flag_attr])* const $Flag = $value),+
            }
        }
    };
}

#[cfg(test)]
#[allow(non_upper_case_globals, dead_code)]
mod tests {
    use std::hash::{SipHasher, Hash, Hasher};

    bitflags! {
        #[doc = "> The first principle is that you must not fool yourself — and"]
        #[doc = "> you are the easiest person to fool."]
        #[doc = "> "]
        #[doc = "> - Richard Feynman"]
        flags Flags: u32 {
            const FlagA       = 0b00000001,
            #[doc = "<pcwalton> macros are way better at generating code than trans is"]
            const FlagB       = 0b00000010,
            const FlagC       = 0b00000100,
            #[doc = "* cmr bed"]
            #[doc = "* strcat table"]
            #[doc = "<strcat> wait what?"]
            const FlagABC     = FlagA.bits
                               | FlagB.bits
                               | FlagC.bits,
        }
    }

    bitflags! {
        flags _CfgFlags: u32 {
            #[cfg(windows)]
            const _CfgA = 0b01,
            #[cfg(unix)]
            const _CfgB = 0b01,
            #[cfg(windows)]
            const _CfgC = _CfgA.bits | 0b10,
        }
    }

    bitflags! {
        flags AnotherSetOfFlags: i8 {
            const AnotherFlag = -1_i8,
        }
    }

    #[test]
    fn test_bits(){
        assert_eq!(Flags::empty().bits(), 0b00000000);
        assert_eq!(FlagA.bits(), 0b00000001);
        assert_eq!(FlagABC.bits(), 0b00000111);

        assert_eq!(AnotherSetOfFlags::empty().bits(), 0b00);
        assert_eq!(AnotherFlag.bits(), !0_i8);
    }

    #[test]
    fn test_from_bits() {
        assert_eq!(Flags::from_bits(0), Some(Flags::empty()));
        assert_eq!(Flags::from_bits(0b1), Some(FlagA));
        assert_eq!(Flags::from_bits(0b10), Some(FlagB));
        assert_eq!(Flags::from_bits(0b11), Some(FlagA | FlagB));
        assert_eq!(Flags::from_bits(0b1000), None);

        assert_eq!(AnotherSetOfFlags::from_bits(!0_i8), Some(AnotherFlag));
    }

    #[test]
    fn test_from_bits_truncate() {
        assert_eq!(Flags::from_bits_truncate(0), Flags::empty());
        assert_eq!(Flags::from_bits_truncate(0b1), FlagA);
        assert_eq!(Flags::from_bits_truncate(0b10), FlagB);
        assert_eq!(Flags::from_bits_truncate(0b11), (FlagA | FlagB));
        assert_eq!(Flags::from_bits_truncate(0b1000), Flags::empty());
        assert_eq!(Flags::from_bits_truncate(0b1001), FlagA);

        assert_eq!(AnotherSetOfFlags::from_bits_truncate(0_i8), AnotherSetOfFlags::empty());
    }

    #[test]
    fn test_is_empty(){
        assert!(Flags::empty().is_empty());
        assert!(!FlagA.is_empty());
        assert!(!FlagABC.is_empty());

        assert!(!AnotherFlag.is_empty());
    }

    #[test]
    fn test_is_all() {
        assert!(Flags::all().is_all());
        assert!(!FlagA.is_all());
        assert!(FlagABC.is_all());

        assert!(AnotherFlag.is_all());
    }

    #[test]
    fn test_two_empties_do_not_intersect() {
        let e1 = Flags::empty();
        let e2 = Flags::empty();
        assert!(!e1.intersects(e2));

        assert!(AnotherFlag.intersects(AnotherFlag));
    }

    #[test]
    fn test_empty_does_not_intersect_with_full() {
        let e1 = Flags::empty();
        let e2 = FlagABC;
        assert!(!e1.intersects(e2));
    }

    #[test]
    fn test_disjoint_intersects() {
        let e1 = FlagA;
        let e2 = FlagB;
        assert!(!e1.intersects(e2));
    }

    #[test]
    fn test_overlapping_intersects() {
        let e1 = FlagA;
        let e2 = FlagA | FlagB;
        assert!(e1.intersects(e2));
    }

    #[test]
    fn test_contains() {
        let e1 = FlagA;
        let e2 = FlagA | FlagB;
        assert!(!e1.contains(e2));
        assert!(e2.contains(e1));
        assert!(FlagABC.contains(e2));

        assert!(AnotherFlag.contains(AnotherFlag));
    }

    #[test]
    fn test_insert(){
        let mut e1 = FlagA;
        let e2 = FlagA | FlagB;
        e1.insert(e2);
        assert_eq!(e1, e2);

        let mut e3 = AnotherSetOfFlags::empty();
        e3.insert(AnotherFlag);
        assert_eq!(e3, AnotherFlag);
    }

    #[test]
    fn test_remove(){
        let mut e1 = FlagA | FlagB;
        let e2 = FlagA | FlagC;
        e1.remove(e2);
        assert_eq!(e1, FlagB);

        let mut e3 = AnotherFlag;
        e3.remove(AnotherFlag);
        assert_eq!(e3, AnotherSetOfFlags::empty());
    }

    #[test]
    fn test_operators() {
        let e1 = FlagA | FlagC;
        let e2 = FlagB | FlagC;
        assert_eq!((e1 | e2), FlagABC);     // union
        assert_eq!((e1 & e2), FlagC);       // intersection
        assert_eq!((e1 - e2), FlagA);       // set difference
        assert_eq!(!e2, FlagA);             // set complement
        assert_eq!(e1 ^ e2, FlagA | FlagB); // toggle
        let mut e3 = e1;
        e3.toggle(e2);
        assert_eq!(e3, FlagA | FlagB);

        let mut m4 = AnotherSetOfFlags::empty();
        m4.toggle(AnotherSetOfFlags::empty());
        assert_eq!(m4, AnotherSetOfFlags::empty());
    }

    #[test]
    fn test_assignment_operators() {
        let mut m1 = Flags::empty();
        let e1 = FlagA | FlagC;
        // union
        m1 |= FlagA;
        assert_eq!(m1, FlagA);
        // intersection
        m1 &= e1;
        assert_eq!(m1, FlagA);
        // set difference
        m1 -= m1;
        assert_eq!(m1, Flags::empty());
        // toggle
        m1 ^= e1;
        assert_eq!(m1, e1);
    }

    #[test]
    fn test_extend() {
        let mut flags;

        flags = Flags::empty();
        flags.extend([].iter().cloned());
        assert_eq!(flags, Flags::empty());

        flags = Flags::empty();
        flags.extend([FlagA, FlagB].iter().cloned());
        assert_eq!(flags, FlagA | FlagB);

        flags = FlagA;
        flags.extend([FlagA, FlagB].iter().cloned());
        assert_eq!(flags, FlagA | FlagB);

        flags = FlagB;
        flags.extend([FlagA, FlagABC].iter().cloned());
        assert_eq!(flags, FlagABC);
    }

    #[test]
    fn test_from_iterator() {
        assert_eq!([].iter().cloned().collect::<Flags>(), Flags::empty());
        assert_eq!([FlagA, FlagB].iter().cloned().collect::<Flags>(), FlagA | FlagB);
        assert_eq!([FlagA, FlagABC].iter().cloned().collect::<Flags>(), FlagABC);
    }

    #[test]
    fn test_lt() {
        let mut a = Flags::empty();
        let mut b = Flags::empty();

        assert!(!(a < b) && !(b < a));
        b = FlagB;
        assert!(a < b);
        a = FlagC;
        assert!(!(a < b) && b < a);
        b = FlagC | FlagB;
        assert!(a < b);
    }

    #[test]
    fn test_ord() {
        let mut a = Flags::empty();
        let mut b = Flags::empty();

        assert!(a <= b && a >= b);
        a = FlagA;
        assert!(a > b && a >= b);
        assert!(b < a && b <= a);
        b = FlagB;
        assert!(b > a && b >= a);
        assert!(a < b && a <= b);
    }

    fn hash<T: Hash>(t: &T) -> u64 {
        let mut s = SipHasher::new_with_keys(0, 0);
        t.hash(&mut s);
        s.finish()
    }

    #[test]
    fn test_hash() {
        let mut x = Flags::empty();
        let mut y = Flags::empty();
        assert_eq!(hash(&x), hash(&y));
        x = Flags::all();
        y = FlagABC;
        assert_eq!(hash(&x), hash(&y));
    }

    #[test]
    fn test_debug() {
        assert_eq!(format!("{:?}", FlagA | FlagB), "FlagA | FlagB");
        assert_eq!(format!("{:?}", FlagABC), "FlagA | FlagB | FlagC | FlagABC");
    }

    mod submodule {
        bitflags! {
            pub flags PublicFlags: i8 {
                const FlagX = 0,
            }
        }
        bitflags! {
            flags PrivateFlags: i8 {
                const FlagY = 0,
            }
        }

        #[test]
        fn test_private() {
            let _ = FlagY;
        }
    }

    #[test]
    fn test_public() {
        let _ = submodule::FlagX;
    }

    mod t1 {
        mod foo {
            pub type Bar = i32;
        }

        bitflags! {
            /// baz
            flags Flags: foo::Bar {
                const A       = 0b00000001,
                #[cfg(foo)]
                const B       = 0b00000010,
                #[cfg(foo)]
                const C       = 0b00000010,
            }
        }
    }
}
