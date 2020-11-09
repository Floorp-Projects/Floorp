/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::error::*;
use nss::pbkdf2::pbkdf2_key_derive;
pub use nss::pbkdf2::HashAlgorithm;
/// Extend passwords using pbkdf2, based on the following [rfc](https://www.ietf.org/rfc/rfc2898.txt) it runs the NSS implementation
/// # Arguments
///
///  * `passphrase` - The password to stretch
///  * `salt` - A salt to use in the generation process
///  * `iterations` - The number of iterations the hashing algorithm will run on each section of the key
///  * `hash_algorithm` - The hash algorithm to use
///  * `out` - The slice the algorithm will populate
///
/// # Examples
///
/// ```
/// use rc_crypto::pbkdf2;
/// let password = b"password";
/// let salt = b"salt";
/// let mut out = vec![0u8; 32];
/// let iterations = 2; // Real code should have a MUCH higher number of iterations (Think 1000+)
/// pbkdf2::derive(password, salt, iterations, pbkdf2::HashAlgorithm::SHA256, &mut out).unwrap(); // Oh oh should handle the error!
/// assert_eq!(hex::encode(out), "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43");
//
///```
///
/// # Errors
///
/// Could possibly return an error if the HMAC algorithm fails, or if the NSS algorithm returns an error
pub fn derive(
    passphrase: &[u8],
    salt: &[u8],
    iterations: u32,
    hash_algorithm: HashAlgorithm,
    out: &mut [u8],
) -> Result<()> {
    pbkdf2_key_derive(passphrase, salt, iterations, hash_algorithm, out)?;
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    #[test]
    fn test_generate_correct_out() {
        let expected = "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b";
        let mut out = vec![0u8; 32];
        let password = b"password";
        let salt = b"salt";
        derive(password, salt, 1, HashAlgorithm::SHA256, &mut out).unwrap();
        assert_eq!(expected, hex::encode(out));
    }

    #[test]
    fn test_longer_key() {
        let expected = "120fb6cffcf8b32c43e7225256c4f837a86548c92ccc35480805987cb70be17b4dbf3a2f3dad3377264bb7b8e8330d4efc7451418617dabef683735361cdc18c";
        let password = b"password";
        let salt = b"salt";
        let mut out = vec![0u8; 64];
        derive(password, salt, 1, HashAlgorithm::SHA256, &mut out).unwrap();
        assert_eq!(expected, hex::encode(out));
    }

    #[test]
    fn test_more_iterations() {
        let expected = "ae4d0c95af6b46d32d0adff928f06dd02a303f8ef3c251dfd6e2d85a95474c43";
        let password = b"password";
        let salt = b"salt";
        let mut out = vec![0u8; 32];
        derive(password, salt, 2, HashAlgorithm::SHA256, &mut out).unwrap();
        assert_eq!(expected, hex::encode(out));
    }

    #[test]
    fn test_odd_length() {
        let expected = "ad35240ac683febfaf3cd49d845473fbbbaa2437f5f82d5a415ae00ac76c6bfccf";
        let password = b"password";
        let salt = b"salt";
        let mut out = vec![0u8; 33];
        derive(password, salt, 3, HashAlgorithm::SHA256, &mut out).unwrap();
        assert_eq!(expected, hex::encode(out));
    }

    #[test]
    fn test_nulls() {
        let expected = "e25d526987819f966e324faa4a";
        let password = b"passw\x00rd";
        let salt = b"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
        let mut out = vec![0u8; 13];
        derive(password, salt, 5, HashAlgorithm::SHA256, &mut out).unwrap();
        assert_eq!(expected, hex::encode(out));
    }

    #[test]
    fn test_password_null() {
        let expected = "62384466264daadc4144018c6bd864648272b34da8980d31521ffcce92ae003b";
        let password = b"\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
        let salt = b"salt";
        let mut out = vec![0u8; 32];
        derive(password, salt, 2, HashAlgorithm::SHA256, &mut out).unwrap();
        assert_eq!(expected, hex::encode(out));
    }

    #[test]
    fn test_empty_password() {
        let expected = "f135c27993baf98773c5cdb40a5706ce6a345cde61b000a67858650cd6a324d7";
        let mut out = vec![0u8; 32];
        let password = b"";
        let salt = b"salt";
        derive(password, salt, 1, HashAlgorithm::SHA256, &mut out).unwrap();
        assert_eq!(expected, hex::encode(out));
    }

    #[test]
    fn test_empty_salt() {
        let expected = "c1232f10f62715fda06ae7c0a2037ca19b33cf103b727ba56d870c11f290a2ab";
        let mut out = vec![0u8; 32];
        let password = b"password";
        let salt = b"";
        derive(password, salt, 1, HashAlgorithm::SHA256, &mut out).unwrap();
        assert_eq!(expected, hex::encode(out));
    }

    #[test]
    fn test_tiny_out() {
        let expected = "12";
        let mut out = vec![0u8; 1];
        let password = b"password";
        let salt = b"salt";
        derive(password, salt, 1, HashAlgorithm::SHA256, &mut out).unwrap();
        assert_eq!(expected, hex::encode(out));
    }

    #[test]
    fn test_rejects_zero_iterations() {
        let mut out = vec![0u8; 32];
        let password = b"password";
        let salt = b"salt";
        assert!(derive(password, salt, 0, HashAlgorithm::SHA256, &mut out).is_err());
    }

    #[test]
    fn test_rejects_empty_out() {
        let mut out = vec![0u8; 0];
        let password = b"password";
        let salt = b"salt";
        assert!(derive(password, salt, 1, HashAlgorithm::SHA256, &mut out).is_err());
    }

    #[test]
    fn test_rejects_gaigantic_salt() {
        if (std::u32::MAX as usize) < std::usize::MAX {
            let salt = vec![0; (std::u32::MAX as usize) + 1];
            let mut out = vec![0u8; 1];
            let password = b"password";
            assert!(derive(password, &salt, 1, HashAlgorithm::SHA256, &mut out).is_err());
        }
    }
    #[test]
    fn test_rejects_gaigantic_password() {
        if (std::u32::MAX as usize) < std::usize::MAX {
            let password = vec![0; (std::u32::MAX as usize) + 1];
            let mut out = vec![0u8; 1];
            let salt = b"salt";
            assert!(derive(&password, salt, 1, HashAlgorithm::SHA256, &mut out).is_err());
        }
    }

    #[test]
    fn test_rejects_gaigantic_out() {
        if (std::u32::MAX as usize) < std::usize::MAX {
            let password = b"password";
            let mut out = vec![0; (std::u32::MAX as usize) + 1];
            let salt = b"salt";
            assert!(derive(password, salt, 1, HashAlgorithm::SHA256, &mut out).is_err());
        }
    }

    #[test]
    fn test_rejects_gaigantic_iterations() {
        let password = b"password";
        let mut out = vec![0; 32];
        let salt = b"salt";
        assert!(derive(
            password,
            salt,
            std::u32::MAX,
            HashAlgorithm::SHA256,
            &mut out
        )
        .is_err());
    }
}
