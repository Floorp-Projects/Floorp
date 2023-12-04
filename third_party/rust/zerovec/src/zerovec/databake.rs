// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::ZeroVec;
use crate::{ule::AsULE, ZeroSlice};
use databake::*;

impl<T> Bake for ZeroVec<'_, T>
where
    T: AsULE + ?Sized + Bake,
{
    fn bake(&self, env: &CrateEnv) -> TokenStream {
        env.insert("zerovec");
        if self.is_empty() {
            quote! { zerovec::ZeroVec::new() }
        } else {
            let bytes = databake::Bake::bake(&self.as_bytes(), env);
            quote! { unsafe { zerovec::ZeroVec::from_bytes_unchecked(#bytes) } }
        }
    }
}

impl<T> Bake for &ZeroSlice<T>
where
    T: AsULE + ?Sized,
{
    fn bake(&self, env: &CrateEnv) -> TokenStream {
        env.insert("zerovec");
        if self.is_empty() {
            quote! { zerovec::ZeroSlice::new_empty() }
        } else {
            let bytes = databake::Bake::bake(&self.as_bytes(), env);
            quote! { unsafe { zerovec::ZeroSlice::from_bytes_unchecked(#bytes) } }
        }
    }
}

#[test]
fn test_baked_vec() {
    test_bake!(
        ZeroVec<u32>,
        const: crate::ZeroVec::new(),
        zerovec
    );
    test_bake!(
        ZeroVec<u32>,
        const: unsafe {
            crate::ZeroVec::from_bytes_unchecked(b"\x02\x01\0\x16\0M\x01\\")
        },
        zerovec
    );
}

#[test]
fn test_baked_slice() {
    test_bake!(
        &ZeroSlice<u32>,
        const: crate::ZeroSlice::new_empty(),
        zerovec
    );
    test_bake!(
        &ZeroSlice<u32>,
        const: unsafe {
            crate::ZeroSlice::from_bytes_unchecked(b"\x02\x01\0\x16\0M\x01\\")
        },
        zerovec
    );
}
