// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::{maps::ZeroMap2dBorrowed, maps::ZeroMapKV, ZeroMap2d};
use databake::*;

impl<'a, K0, K1, V> Bake for ZeroMap2d<'a, K0, K1, V>
where
    K0: ZeroMapKV<'a> + ?Sized,
    K1: ZeroMapKV<'a> + ?Sized,
    V: ZeroMapKV<'a> + ?Sized,
    K0::Container: Bake,
    K1::Container: Bake,
    V::Container: Bake,
{
    fn bake(&self, env: &CrateEnv) -> TokenStream {
        env.insert("zerovec");
        let keys0 = self.keys0.bake(env);
        let joiner = self.joiner.bake(env);
        let keys1 = self.keys1.bake(env);
        let values = self.values.bake(env);
        quote! { unsafe { #[allow(unused_unsafe)] ::zerovec::ZeroMap2d::from_parts_unchecked(#keys0, #joiner, #keys1, #values) } }
    }
}

impl<'a, K0, K1, V> Bake for ZeroMap2dBorrowed<'a, K0, K1, V>
where
    K0: ZeroMapKV<'a> + ?Sized,
    K1: ZeroMapKV<'a> + ?Sized,
    V: ZeroMapKV<'a> + ?Sized,
    &'a K0::Slice: Bake,
    &'a K1::Slice: Bake,
    &'a V::Slice: Bake,
{
    fn bake(&self, env: &CrateEnv) -> TokenStream {
        env.insert("zerovec");
        let keys0 = self.keys0.bake(env);
        let joiner = self.joiner.bake(env);
        let keys1 = self.keys1.bake(env);
        let values = self.values.bake(env);
        quote! { unsafe { #[allow(unused_unsafe)] ::zerovec::maps::ZeroMap2dBorrowed::from_parts_unchecked(#keys0, #joiner, #keys1, #values) } }
    }
}

#[test]
fn test_baked_map() {
    test_bake!(
        ZeroMap2d<str, str, str>,
        const: unsafe {
            #[allow(unused_unsafe)]
            crate::ZeroMap2d::from_parts_unchecked(
                unsafe {
                    crate::VarZeroVec::from_bytes_unchecked(
                        b"arcaz\0cu\0en\0ff\0grckk\0ku\0ky\0lifmanmn\0pa\0palsd\0tg\0ug\0unruz\0yuezh\0"
                    )
                },
                unsafe {
                    crate::ZeroVec::from_bytes_unchecked(
                        b"\x02\0\0\0\x03\0\0\0\x04\0\0\0\x05\0\0\0\x06\0\0\0\x07\0\0\0\x08\0\0\0\n\0\0\0\x0C\0\0\0\r\0\0\0\x0E\0\0\0\x0F\0\0\0\x10\0\0\0\x11\0\0\0\x14\0\0\0\x15\0\0\0\x16\0\0\0\x17\0\0\0\x18\0\0\0\x19\0\0\0\x1C\0\0\0"
                    )
                },
                unsafe {
                    crate::VarZeroVec::from_bytes_unchecked(
                        b"NbatPalmArabGlagShawAdlmLinbArabArabYeziArabLatnLimbNkooMongArabPhlpDevaKhojSindArabCyrlDevaArabHansBopoHanbHant"
                    )
                },
                unsafe {
                    crate::VarZeroVec::from_bytes_unchecked(
                        b"JO\0SY\0IR\0BG\0GB\0GN\0GR\0CN\0IQ\0GE\0CN\0TR\0IN\0GN\0CN\0PK\0CN\0IN\0IN\0IN\0PK\0KZ\0NP\0AF\0CN\0TW\0TW\0TW\0"
                    )
                },
            )
        },
        zerovec
    );
}

#[test]
fn test_baked_borrowed_map() {
    test_bake!(
        ZeroMap2dBorrowed<str, str, str>,
        const: unsafe {
            #[allow(unused_unsafe)]
            crate::maps::ZeroMap2dBorrowed::from_parts_unchecked(
                unsafe {
                    crate::VarZeroSlice::from_bytes_unchecked(
                        b"arcaz\0cu\0en\0ff\0grckk\0ku\0ky\0lifmanmn\0pa\0palsd\0tg\0ug\0unruz\0yuezh\0"
                    )
                },
                unsafe {
                    crate::ZeroSlice::from_bytes_unchecked(
                        b"\x02\0\0\0\x03\0\0\0\x04\0\0\0\x05\0\0\0\x06\0\0\0\x07\0\0\0\x08\0\0\0\n\0\0\0\x0C\0\0\0\r\0\0\0\x0E\0\0\0\x0F\0\0\0\x10\0\0\0\x11\0\0\0\x14\0\0\0\x15\0\0\0\x16\0\0\0\x17\0\0\0\x18\0\0\0\x19\0\0\0\x1C\0\0\0"
                    )
                },
                unsafe {
                    crate::VarZeroSlice::from_bytes_unchecked(
                        b"NbatPalmArabGlagShawAdlmLinbArabArabYeziArabLatnLimbNkooMongArabPhlpDevaKhojSindArabCyrlDevaArabHansBopoHanbHant"
                    )
                },
                unsafe {
                    crate::VarZeroSlice::from_bytes_unchecked(
                        b"JO\0SY\0IR\0BG\0GB\0GN\0GR\0CN\0IQ\0GE\0CN\0TR\0IN\0GN\0CN\0PK\0CN\0IN\0IN\0IN\0PK\0KZ\0NP\0AF\0CN\0TW\0TW\0TW\0"
                    )
                },
            )
        },
        zerovec
    );
}
