// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use super::{FlexZeroSlice, FlexZeroVec};
use databake::*;

impl Bake for FlexZeroVec<'_> {
    fn bake(&self, env: &CrateEnv) -> TokenStream {
        env.insert("zerovec");
        if self.is_empty() {
            quote! { zerovec::vecs::FlexZeroVec::new() }
        } else {
            let slice = self.as_ref().bake(env);
            quote! { #slice.as_flexzerovec() }
        }
    }
}

impl Bake for &FlexZeroSlice {
    fn bake(&self, env: &CrateEnv) -> TokenStream {
        env.insert("zerovec");
        if self.is_empty() {
            quote! { zerovec::vecs::FlexZeroSlice::new_empty() }
        } else {
            let bytes = databake::Bake::bake(&self.as_bytes(), env);
            quote! { unsafe { zerovec::vecs::FlexZeroSlice::from_byte_slice_unchecked(#bytes) } }
        }
    }
}

#[test]
fn test_baked_vec() {
    test_bake!(
        FlexZeroVec,
        const: crate::vecs::FlexZeroVec::new(),
        zerovec
    );
    test_bake!(
        FlexZeroVec,
        const: unsafe {
            crate::vecs::FlexZeroSlice::from_byte_slice_unchecked(
                b"\x02\x01\0\x16\0M\x01\x11"
            )
        }.as_flexzerovec(),
        zerovec
    );
}

#[test]
fn test_baked_slice() {
    test_bake!(
        &FlexZeroSlice,
        const: crate::vecs::FlexZeroSlice::new_empty(),
        zerovec
    );
    test_bake!(
        &FlexZeroSlice,
        const: unsafe {
            crate::vecs::FlexZeroSlice::from_byte_slice_unchecked(
                b"\x02\x01\0\x16\0M\x01\x11"
            )
        },
        zerovec
    );
}
