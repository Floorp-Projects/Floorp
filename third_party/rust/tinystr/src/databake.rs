// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

use crate::TinyAsciiStr;
use databake::*;

impl<const N: usize> Bake for TinyAsciiStr<N> {
    fn bake(&self, env: &CrateEnv) -> TokenStream {
        env.insert("tinystr");
        let string = self.as_str();
        quote! {
            ::tinystr::tinystr!(#N, #string)
        }
    }
}

#[test]
fn test() {
    test_bake!(TinyAsciiStr<10>, const: crate::tinystr!(10usize, "foo"), tinystr);
}
