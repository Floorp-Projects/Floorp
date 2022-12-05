// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

pub mod aead;
pub mod hkdf;
pub mod hpke;

use crate::err::Res;

pub struct SymKey(Vec<u8>);

impl SymKey {
    #[allow(clippy::unnecessary_wraps)]
    pub fn key_data(&self) -> Res<&[u8]> {
        Ok(&self.0)
    }
}

impl From<Vec<u8>> for SymKey {
    fn from(v: Vec<u8>) -> Self {
        SymKey(v)
    }
}
impl From<&[u8]> for SymKey {
    fn from(v: &[u8]) -> Self {
        SymKey(v.to_owned())
    }
}

impl AsRef<[u8]> for SymKey {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

impl std::fmt::Debug for SymKey {
    fn fmt(&self, f: &mut std::fmt::Formatter) -> std::fmt::Result {
        if let Ok(b) = self.key_data() {
            write!(f, "SymKey {}", hex::encode(b))
        } else {
            write!(f, "Opaque SymKey")
        }
    }
}
