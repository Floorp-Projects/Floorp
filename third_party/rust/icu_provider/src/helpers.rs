// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Internal helper functions.

/// Const function to compute the FxHash of a byte array with little-endian byte order.
///
/// FxHash is a speedy hash algorithm used within rustc. The algorithm is satisfactory for our
/// use case since the strings being hashed originate from a trusted source (the ICU4X
/// components), and the hashes are computed at compile time, so we can check for collisions.
///
/// We could have considered a SHA or other cryptographic hash function. However, we are using
/// FxHash because:
///
/// 1. There is precedent for this algorithm in Rust
/// 2. The algorithm is easy to implement as a const function
/// 3. The amount of code is small enough that we can reasonably keep the algorithm in-tree
/// 4. FxHash is designed to output 32-bit or 64-bit values, whereas SHA outputs more bits,
///    such that truncation would be required in order to fit into a u32, partially reducing
///    the benefit of a cryptographically secure algorithm
// The indexing operations in this function have been reviewed in detail and won't panic.
#[allow(clippy::indexing_slicing)]
pub const fn fxhash_32(bytes: &[u8], ignore_leading: usize, ignore_trailing: usize) -> u32 {
    // This code is adapted from https://github.com/rust-lang/rustc-hash,
    // whose license text is reproduced below.
    //
    // Copyright 2015 The Rust Project Developers. See the COPYRIGHT
    // file at the top-level directory of this distribution and at
    // http://rust-lang.org/COPYRIGHT.
    //
    // Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
    // http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
    // <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
    // option. This file may not be copied, modified, or distributed
    // except according to those terms.

    if ignore_leading + ignore_trailing >= bytes.len() {
        return 0;
    }

    #[inline]
    const fn hash_word_32(mut hash: u32, word: u32) -> u32 {
        const ROTATE: u32 = 5;
        const SEED32: u32 = 0x9e_37_79_b9;
        hash = hash.rotate_left(ROTATE);
        hash ^= word;
        hash = hash.wrapping_mul(SEED32);
        hash
    }

    let mut cursor = ignore_leading;
    let end = bytes.len() - ignore_trailing;
    let mut hash = 0;

    while end - cursor >= 4 {
        let word = u32::from_le_bytes([
            bytes[cursor],
            bytes[cursor + 1],
            bytes[cursor + 2],
            bytes[cursor + 3],
        ]);
        hash = hash_word_32(hash, word);
        cursor += 4;
    }

    if end - cursor >= 2 {
        let word = u16::from_le_bytes([bytes[cursor], bytes[cursor + 1]]);
        hash = hash_word_32(hash, word as u32);
        cursor += 2;
    }

    if end - cursor >= 1 {
        hash = hash_word_32(hash, bytes[cursor] as u32);
    }

    hash
}

#[test]
fn test_hash_word_32() {
    assert_eq!(0, fxhash_32(b"", 0, 0));
    assert_eq!(0, fxhash_32(b"a", 1, 0));
    assert_eq!(0, fxhash_32(b"a", 0, 1));
    assert_eq!(0, fxhash_32(b"a", 0, 10));
    assert_eq!(0, fxhash_32(b"a", 10, 0));
    assert_eq!(0, fxhash_32(b"a", 1, 1));
    assert_eq!(0xF3051F19, fxhash_32(b"a", 0, 0));
    assert_eq!(0x2F9DF119, fxhash_32(b"ab", 0, 0));
    assert_eq!(0xCB1D9396, fxhash_32(b"abc", 0, 0));
    assert_eq!(0x8628F119, fxhash_32(b"abcd", 0, 0));
    assert_eq!(0xBEBDB56D, fxhash_32(b"abcde", 0, 0));
    assert_eq!(0x1CE8476D, fxhash_32(b"abcdef", 0, 0));
    assert_eq!(0xC0F176A4, fxhash_32(b"abcdefg", 0, 0));
    assert_eq!(0x09AB476D, fxhash_32(b"abcdefgh", 0, 0));
    assert_eq!(0xB72F5D88, fxhash_32(b"abcdefghi", 0, 0));

    assert_eq!(
        fxhash_32(crate::tagged!("props/sc=Khmr@1").as_bytes(), 0, 0),
        fxhash_32(crate::tagged!("props/sc=Samr@1").as_bytes(), 0, 0)
    );

    assert_ne!(
        fxhash_32(
            crate::tagged!("props/sc=Khmr@1").as_bytes(),
            crate::leading_tag!().len(),
            crate::trailing_tag!().len()
        ),
        fxhash_32(
            crate::tagged!("props/sc=Samr@1").as_bytes(),
            crate::leading_tag!().len(),
            crate::trailing_tag!().len()
        )
    );
}

#[doc(hidden)]
#[macro_export]
macro_rules! gen_any_buffer_docs {
    (ANY, $krate:path, $see_also:path) => {
        concat!(
            "Creates a new instance using an [`AnyProvider`](",
            stringify!($krate),
            "::AnyProvider).\n\n",
            "For details on the behavior of this function, see: [`",
            stringify!($see_also),
            "`]\n\n",
            "[📚 Help choosing a constructor](",
            stringify!($krate),
            "::constructors)",
        )
    };
    (BUFFER, $krate:path, $see_also:path) => {
        concat!(
            "✨ **Enabled with the `\"serde\"` feature.**\n\n",
            "Creates a new instance using a [`BufferProvider`](",
            stringify!($krate),
            "::BufferProvider).\n\n",
            "For details on the behavior of this function, see: [`",
            stringify!($see_also),
            "`]\n\n",
            "[📚 Help choosing a constructor](",
            stringify!($krate),
            "::constructors)",
        )
    };
}

#[doc(hidden)]
#[macro_export]
macro_rules! gen_any_buffer_constructors {
    (locale: skip, options: skip, error: $error_ty:path) => {
        $crate::gen_any_buffer_constructors!(
            locale: skip,
            options: skip,
            error: $error_ty,
            functions: [
                Self::try_new_unstable,
                try_new_with_any_provider,
                try_new_with_buffer_provider
            ]
        );
    };
    (locale: skip, options: skip, error: $error_ty:path, functions: [$f1:path, $f2:ident, $f3:ident]) => {
        #[doc = $crate::gen_any_buffer_docs!(ANY, $crate, $f1)]
        pub fn $f2(provider: &(impl $crate::AnyProvider + ?Sized)) -> Result<Self, $error_ty> {
            use $crate::AsDowncastingAnyProvider;
            $f1(&provider.as_downcasting())
        }
        #[cfg(feature = "serde")]
        #[doc = $crate::gen_any_buffer_docs!(BUFFER, $crate, $f1)]
        pub fn $f3(provider: &(impl $crate::BufferProvider + ?Sized)) -> Result<Self, $error_ty> {
            use $crate::AsDeserializingBufferProvider;
            $f1(&provider.as_deserializing())
        }
    };


    (locale: skip, options: skip, result: $result_ty:path, functions: [$f1:path, $f2:ident, $f3:ident]) => {
        #[doc = $crate::gen_any_buffer_docs!(ANY, $crate, $f1)]
        pub fn $f2(provider: &(impl $crate::AnyProvider + ?Sized)) -> $result_ty {
            use $crate::AsDowncastingAnyProvider;
            $f1(&provider.as_downcasting())
        }
        #[cfg(feature = "serde")]
        #[doc = $crate::gen_any_buffer_docs!(BUFFER, $crate, $f1)]
        pub fn $f3(provider: &(impl $crate::BufferProvider + ?Sized)) -> $result_ty {
            use $crate::AsDeserializingBufferProvider;
            $f1(&provider.as_deserializing())
        }
    };

    (locale: skip, $options_arg:ident: $options_ty:ty, error: $error_ty:path) => {
        $crate::gen_any_buffer_constructors!(
            locale: skip,
            $options_arg: $options_ty,
            error: $error_ty,
            functions: [
                Self::try_new_unstable,
                try_new_with_any_provider,
                try_new_with_buffer_provider
            ]
        );
    };
    (locale: skip, $options_arg:ident: $options_ty:ty, result: $result_ty:ty, functions: [$f1:path, $f2:ident, $f3:ident]) => {
        #[doc = $crate::gen_any_buffer_docs!(ANY, $crate, $f1)]
        pub fn $f2(provider: &(impl $crate::AnyProvider + ?Sized), $options_arg: $options_ty) -> $result_ty {
            use $crate::AsDowncastingAnyProvider;
            $f1(&provider.as_downcasting(), $options_arg)
        }
        #[cfg(feature = "serde")]
        #[doc = $crate::gen_any_buffer_docs!(BUFFER, $crate, $f1)]
        pub fn $f3(provider: &(impl $crate::BufferProvider + ?Sized), $options_arg: $options_ty) -> $result_ty {
            use $crate::AsDeserializingBufferProvider;
            $f1(&provider.as_deserializing(), $options_arg)
        }
    };
    (locale: skip, $options_arg:ident: $options_ty:ty, error: $error_ty:ty, functions: [$f1:path, $f2:ident, $f3:ident]) => {
        #[doc = $crate::gen_any_buffer_docs!(ANY, $crate, $f1)]
        pub fn $f2(provider: &(impl $crate::AnyProvider + ?Sized), $options_arg: $options_ty) -> Result<Self, $error_ty> {
            use $crate::AsDowncastingAnyProvider;
            $f1(&provider.as_downcasting(), $options_arg)
        }
        #[cfg(feature = "serde")]
        #[doc = $crate::gen_any_buffer_docs!(BUFFER, $crate, $f1)]
        pub fn $f3(provider: &(impl $crate::BufferProvider + ?Sized), $options_arg: $options_ty) -> Result<Self, $error_ty> {
            use $crate::AsDeserializingBufferProvider;
            $f1(&provider.as_deserializing(), $options_arg)
        }
    };
    (locale: include, options: skip, error: $error_ty:path) => {
        $crate::gen_any_buffer_constructors!(
            locale: include,
            options: skip,
            error: $error_ty,
            functions: [
                Self::try_new_unstable,
                try_new_with_any_provider,
                try_new_with_buffer_provider
            ]
        );
    };
    (locale: include, options: skip, error: $error_ty:path, functions: [$f1:path, $f2:ident, $f3:ident]) => {
        #[doc = $crate::gen_any_buffer_docs!(ANY, $crate, $f1)]
        pub fn $f2(provider: &(impl $crate::AnyProvider + ?Sized), locale: &$crate::DataLocale) -> Result<Self, $error_ty> {
            use $crate::AsDowncastingAnyProvider;
            $f1(&provider.as_downcasting(), locale)
        }
        #[cfg(feature = "serde")]
        #[doc = $crate::gen_any_buffer_docs!(BUFFER, $crate, $f1)]
        pub fn $f3(provider: &(impl $crate::BufferProvider + ?Sized), locale: &$crate::DataLocale) -> Result<Self, $error_ty> {
            use $crate::AsDeserializingBufferProvider;
            $f1(&provider.as_deserializing(), locale)
        }
    };

    (locale: include, $config_arg:ident: $config_ty:path, $options_arg:ident: $options_ty:path, error: $error_ty:path) => {
        $crate::gen_any_buffer_constructors!(
            locale: include,
            $config_arg: $config_ty,
            $options_arg: $options_ty,
            error: $error_ty,
            functions: [
                Self::try_new_unstable,
                try_new_with_any_provider,
                try_new_with_buffer_provider
            ]
        );
    };
    (locale: include, $config_arg:ident: $config_ty:path, $options_arg:ident: $options_ty:path, error: $error_ty:path, functions: [$f1:path, $f2:ident, $f3:ident]) => {
        #[doc = $crate::gen_any_buffer_docs!(ANY, $crate, $f1)]
        pub fn $f2(provider: &(impl $crate::AnyProvider + ?Sized), locale: &$crate::DataLocale, $config_arg: $config_ty, $options_arg: $options_ty) -> Result<Self, $error_ty> {
            use $crate::AsDowncastingAnyProvider;
            $f1(&provider.as_downcasting(), locale, $config_arg, $options_arg)
        }
        #[cfg(feature = "serde")]
        #[doc = $crate::gen_any_buffer_docs!(BUFFER, $crate, $f1)]
        pub fn $f3(provider: &(impl $crate::BufferProvider + ?Sized), locale: &$crate::DataLocale, $config_arg: $config_ty, $options_arg: $options_ty) -> Result<Self, $error_ty> {
            use $crate::AsDeserializingBufferProvider;
            $f1(&provider.as_deserializing(), locale, $config_arg, $options_arg)
        }
    };

    (locale: include, $options_arg:ident: $options_ty:path, error: $error_ty:path) => {
        $crate::gen_any_buffer_constructors!(
            locale: include,
            $options_arg: $options_ty,
            error: $error_ty,
            functions: [
                Self::try_new_unstable,
                try_new_with_any_provider,
                try_new_with_buffer_provider
            ]
        );
    };
    (locale: include, $options_arg:ident: $options_ty:path, error: $error_ty:path, functions: [$f1:path, $f2:ident, $f3:ident]) => {
        #[doc = $crate::gen_any_buffer_docs!(ANY, $crate, $f1)]
        pub fn $f2(provider: &(impl $crate::AnyProvider + ?Sized), locale: &$crate::DataLocale, $options_arg: $options_ty) -> Result<Self, $error_ty> {
            use $crate::AsDowncastingAnyProvider;
            $f1(&provider.as_downcasting(), locale, $options_arg)
        }
        #[cfg(feature = "serde")]
        #[doc = $crate::gen_any_buffer_docs!(BUFFER, $crate, $f1)]
        pub fn $f3(provider: &(impl $crate::BufferProvider + ?Sized), locale: &$crate::DataLocale, $options_arg: $options_ty) -> Result<Self, $error_ty> {
            use $crate::AsDeserializingBufferProvider;
            $f1(&provider.as_deserializing(), locale, $options_arg)
        }
    };
}
