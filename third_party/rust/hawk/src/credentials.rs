use crate::crypto::{self, HmacKey};

#[derive(Clone, Copy, PartialEq, PartialOrd, Eq, Ord, Hash, Debug)]
pub enum DigestAlgorithm {
    Sha256,
    Sha384,
    Sha512,
    // Indicate that this isn't an enum that anyone should match on, and that we
    // reserve the right to add to this enumeration without making a major
    // version bump. Once https://github.com/rust-lang/rfcs/blob/master/text/2008-non-exhaustive.md
    // is stabilized, that should be used instead.
    #[doc(hidden)]
    _Nonexhaustive,
}

/// Hawk key.
///
/// While any sequence of bytes can be specified as a key, note that each digest algorithm has
/// a suggested key length, and that passwords should *not* be used as keys.  Keys of incorrect
/// length are handled according to the digest's implementation.
pub struct Key(Box<dyn HmacKey>);

impl Key {
    pub fn new<B>(key: B, algorithm: DigestAlgorithm) -> crate::Result<Key>
    where
        B: AsRef<[u8]>,
    {
        Ok(Key(crypto::new_key(algorithm, key.as_ref())?))
    }

    pub fn sign(&self, data: &[u8]) -> crate::Result<Vec<u8>> {
        Ok(self.0.sign(data)?)
    }
}

/// Hawk credentials: an ID and a key associated with that ID.  The digest algorithm
/// must be agreed between the server and the client, and the length of the key is
/// specific to that algorithm.
pub struct Credentials {
    pub id: String,
    pub key: Key,
}

#[cfg(all(test, any(feature = "use_ring", feature = "use_openssl")))]
mod test {
    use super::*;

    #[test]
    fn test_new_sha256() {
        let key = vec![77u8; 32];
        // hmac::SigningKey doesn't allow any visibilty inside, so we just build the
        // key and assume it works..
        Key::new(key, DigestAlgorithm::Sha256).unwrap();
    }

    #[test]
    fn test_new_sha256_bad_length() {
        let key = vec![0u8; 99];
        Key::new(key, DigestAlgorithm::Sha256).unwrap();
    }
}
