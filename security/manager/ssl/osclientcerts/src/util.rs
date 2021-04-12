/* -*- Mode: rust; rust-indent-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use byteorder::{BigEndian, NativeEndian, ReadBytesExt, WriteBytesExt};
use std::convert::TryInto;

/// Accessing fields of packed structs is unsafe (it may be undefined behavior if the field isn't
/// aligned). Since we're implementing a PKCS#11 module, we already have to trust the caller not to
/// give us bad data, so normally we would deal with this by adding an unsafe block. If we do that,
/// though, the compiler complains that the unsafe block is unnecessary. Thus, we use this macro to
/// annotate the unsafe block to silence the compiler.
macro_rules! unsafe_packed_field_access {
    ($e:expr) => {{
        #[allow(unused_unsafe)]
        let tmp = unsafe { $e };
        tmp
    }};
}

// The following ENCODED_OID_BYTES_* consist of the encoded bytes of an ASN.1
// OBJECT IDENTIFIER specifying the indicated OID (in other words, the full
// tag, length, and value).
#[cfg(target_os = "macos")]
pub const ENCODED_OID_BYTES_SECP256R1: &[u8] =
    &[0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07];
#[cfg(target_os = "macos")]
pub const ENCODED_OID_BYTES_SECP384R1: &[u8] = &[0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x22];
#[cfg(target_os = "macos")]
pub const ENCODED_OID_BYTES_SECP521R1: &[u8] = &[0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x23];

// The following OID_BYTES_* consist of the contents of the bytes of an ASN.1
// OBJECT IDENTIFIER specifying the indicated OID (in other words, just the
// value, and not the tag or length).
#[cfg(target_os = "macos")]
pub const OID_BYTES_SHA_256: &[u8] = &[0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x01];
#[cfg(target_os = "macos")]
pub const OID_BYTES_SHA_384: &[u8] = &[0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x02];
#[cfg(target_os = "macos")]
pub const OID_BYTES_SHA_512: &[u8] = &[0x60, 0x86, 0x48, 0x01, 0x65, 0x03, 0x04, 0x02, 0x03];
#[cfg(target_os = "macos")]
pub const OID_BYTES_SHA_1: &[u8] = &[0x2b, 0x0e, 0x03, 0x02, 0x1a];

// This is a helper function to take a value and lay it out in memory how
// PKCS#11 is expecting it.
pub fn serialize_uint<T: TryInto<u64>>(value: T) -> Result<Vec<u8>, ()> {
    let value_size = std::mem::size_of::<T>();
    let mut value_buf = Vec::with_capacity(value_size);
    let value_as_u64 = value.try_into().map_err(|_| ())?;
    value_buf
        .write_uint::<NativeEndian>(value_as_u64, value_size)
        .map_err(|_| ())?;
    Ok(value_buf)
}

/// Given a slice of DER bytes representing an RSA public key, extracts the bytes of the modulus
/// as an unsigned integer. Also verifies that the public exponent is present (again as an
/// unsigned integer). Finally verifies that reading these values consumes the entirety of the
/// slice.
/// RSAPublicKey ::= SEQUENCE {
///     modulus           INTEGER,  -- n
///     publicExponent    INTEGER   -- e
/// }
pub fn read_rsa_modulus(public_key: &[u8]) -> Result<Vec<u8>, ()> {
    let mut sequence = Sequence::new(public_key)?;
    let modulus_value = sequence.read_unsigned_integer()?;
    let _exponent = sequence.read_unsigned_integer()?;
    if !sequence.at_end() {
        return Err(());
    }
    Ok(modulus_value.to_vec())
}

/// Given a slice of DER bytes representing a DigestInfo, extracts the bytes of
/// the OID of the hash algorithm and the digest.
/// DigestInfo ::= SEQUENCE {
///   digestAlgorithm DigestAlgorithmIdentifier,
///   digest Digest }
///
/// DigestAlgorithmIdentifier ::= AlgorithmIdentifier
///
/// AlgorithmIdentifier  ::=  SEQUENCE  {
///      algorithm               OBJECT IDENTIFIER,
///      parameters              ANY DEFINED BY algorithm OPTIONAL  }
///
/// Digest ::= OCTET STRING
pub fn read_digest_info<'a>(digest_info: &'a [u8]) -> Result<(&'a [u8], &'a [u8]), ()> {
    let mut sequence = Sequence::new(digest_info)?;
    let mut algorithm = sequence.read_sequence()?;
    let oid = algorithm.read_oid()?;
    algorithm.read_null()?;
    if !algorithm.at_end() {
        error!("read_digest: extra input");
        return Err(());
    }
    let digest = sequence.read_octet_string()?;
    if !sequence.at_end() {
        error!("read_digest: extra input");
        return Err(());
    }
    Ok((oid, digest))
}

/// Given a slice of DER bytes representing an ECDSA signature, extracts the bytes of `r` and `s`
/// as unsigned integers. Also verifies that this consumes the entirety of the slice.
///   Ecdsa-Sig-Value  ::=  SEQUENCE  {
///        r     INTEGER,
///        s     INTEGER  }
#[cfg(target_os = "macos")]
pub fn read_ec_sig_point<'a>(signature: &'a [u8]) -> Result<(&'a [u8], &'a [u8]), ()> {
    let mut sequence = Sequence::new(signature)?;
    let r = sequence.read_unsigned_integer()?;
    let s = sequence.read_unsigned_integer()?;
    if !sequence.at_end() {
        return Err(());
    }
    Ok((r, s))
}

/// Given a slice of DER bytes representing an X.509 certificate, extracts the encoded serial
/// number. Does not verify that the remainder of the certificate is in any way well-formed.
///   Certificate  ::=  SEQUENCE  {
///           tbsCertificate       TBSCertificate,
///           signatureAlgorithm   AlgorithmIdentifier,
///           signatureValue       BIT STRING  }
///
///   TBSCertificate  ::=  SEQUENCE  {
///           version         [0]  EXPLICIT Version DEFAULT v1,
///           serialNumber         CertificateSerialNumber,
///           ...
///
///   CertificateSerialNumber  ::=  INTEGER
pub fn read_encoded_serial_number(certificate: &[u8]) -> Result<Vec<u8>, ()> {
    let mut certificate_sequence = Sequence::new(certificate)?;
    let mut tbs_certificate_sequence = certificate_sequence.read_sequence()?;
    let _version = tbs_certificate_sequence.read_tagged_value(0)?;
    let serial_number = tbs_certificate_sequence.read_encoded_sequence_component(INTEGER)?;
    Ok(serial_number)
}

/// Helper macro for reading some bytes from a slice while checking the slice is long enough.
/// Returns a pair consisting of a slice of the bytes read and a slice of the rest of the bytes
/// from the original slice.
macro_rules! try_read_bytes {
    ($data:ident, $len:expr) => {{
        if $data.len() < $len {
            return Err(());
        }
        $data.split_at($len)
    }};
}

/// ASN.1 tag identifying an integer.
const INTEGER: u8 = 0x02;
/// ASN.1 tag identifying an octet string.
const OCTET_STRING: u8 = 0x04;
/// ASN.1 tag identifying a null value.
const NULL: u8 = 0x05;
/// ASN.1 tag identifying an object identifier (OID).
const OBJECT_IDENTIFIER: u8 = 0x06;
/// ASN.1 tag identifying a sequence.
const SEQUENCE: u8 = 0x10;
/// ASN.1 tag modifier identifying an item as constructed.
const CONSTRUCTED: u8 = 0x20;
/// ASN.1 tag modifier identifying an item as context-specific.
const CONTEXT_SPECIFIC: u8 = 0x80;

/// A helper struct for reading items from a DER SEQUENCE (in this case, all sequences are
/// assumed to be CONSTRUCTED).
struct Sequence<'a> {
    /// The contents of the SEQUENCE.
    contents: Der<'a>,
}

impl<'a> Sequence<'a> {
    fn new(input: &'a [u8]) -> Result<Sequence<'a>, ()> {
        let mut der = Der::new(input);
        let (_, _, sequence_bytes) = der.read_tlv(SEQUENCE | CONSTRUCTED)?;
        // We're assuming we want to consume the entire input for now.
        if !der.at_end() {
            return Err(());
        }
        Ok(Sequence {
            contents: Der::new(sequence_bytes),
        })
    }

    // TODO: we're not exhaustively validating this integer
    fn read_unsigned_integer(&mut self) -> Result<&'a [u8], ()> {
        let (_, _, bytes) = self.contents.read_tlv(INTEGER)?;
        if bytes.is_empty() {
            return Err(());
        }
        // There may be a leading zero (we should also check that the first bit
        // of the rest of the integer is set).
        if bytes[0] == 0 && bytes.len() > 1 {
            let (_, integer) = bytes.split_at(1);
            Ok(integer)
        } else {
            Ok(bytes)
        }
    }

    fn read_octet_string(&mut self) -> Result<&'a [u8], ()> {
        let (_, _, bytes) = self.contents.read_tlv(OCTET_STRING)?;
        Ok(bytes)
    }

    fn read_oid(&mut self) -> Result<&'a [u8], ()> {
        let (_, _, bytes) = self.contents.read_tlv(OBJECT_IDENTIFIER)?;
        Ok(bytes)
    }

    fn read_null(&mut self) -> Result<(), ()> {
        let (_, _, bytes) = self.contents.read_tlv(NULL)?;
        if bytes.len() == 0 {
            Ok(())
        } else {
            Err(())
        }
    }

    fn read_sequence(&mut self) -> Result<Sequence<'a>, ()> {
        let (_, _, sequence_bytes) = self.contents.read_tlv(SEQUENCE | CONSTRUCTED)?;
        Ok(Sequence {
            contents: Der::new(sequence_bytes),
        })
    }

    fn read_tagged_value(&mut self, tag: u8) -> Result<&'a [u8], ()> {
        let (_, _, tagged_value_bytes) = self
            .contents
            .read_tlv(CONTEXT_SPECIFIC | CONSTRUCTED | tag)?;
        Ok(tagged_value_bytes)
    }

    fn read_encoded_sequence_component(&mut self, tag: u8) -> Result<Vec<u8>, ()> {
        let (tag, length, value) = self.contents.read_tlv(tag)?;
        let mut encoded_component_bytes = length;
        encoded_component_bytes.insert(0, tag);
        encoded_component_bytes.extend_from_slice(value);
        Ok(encoded_component_bytes)
    }

    fn at_end(&self) -> bool {
        self.contents.at_end()
    }
}

/// A helper struct for reading DER data. The contents are treated like a cursor, so its position
/// is updated as data is read.
struct Der<'a> {
    contents: &'a [u8],
}

impl<'a> Der<'a> {
    fn new(contents: &'a [u8]) -> Der<'a> {
        Der { contents }
    }

    // In theory, a caller could encounter an error and try another operation, in which case we may
    // be in an inconsistent state. As long as this implementation isn't exposed to code that would
    // use it incorrectly (i.e. it stays in this module and we only expose a stateless API), it
    // should be safe.
    /// Given an expected tag, reads the next (tag, lengh, value) from the contents. Most
    /// consumers will only be interested in the value, but some may want the entire encoded
    /// contents, in which case the returned tuple can be concatenated.
    fn read_tlv(&mut self, tag: u8) -> Result<(u8, Vec<u8>, &'a [u8]), ()> {
        let contents = self.contents;
        let (tag_read, rest) = try_read_bytes!(contents, 1);
        if tag_read[0] != tag {
            return Err(());
        }
        let mut accumulated_length_bytes = Vec::with_capacity(4);
        let (length1, rest) = try_read_bytes!(rest, 1);
        accumulated_length_bytes.extend_from_slice(length1);
        let (length, to_read_from) = if length1[0] < 0x80 {
            (length1[0] as usize, rest)
        } else if length1[0] == 0x81 {
            let (length, rest) = try_read_bytes!(rest, 1);
            accumulated_length_bytes.extend_from_slice(length);
            if length[0] < 0x80 {
                return Err(());
            }
            (length[0] as usize, rest)
        } else if length1[0] == 0x82 {
            let (lengths, rest) = try_read_bytes!(rest, 2);
            accumulated_length_bytes.extend_from_slice(lengths);
            let length = (&mut &lengths[..])
                .read_u16::<BigEndian>()
                .map_err(|_| ())?;
            if length < 256 {
                return Err(());
            }
            (length as usize, rest)
        } else {
            return Err(());
        };
        let (contents, rest) = try_read_bytes!(to_read_from, length);
        self.contents = rest;
        Ok((tag, accumulated_length_bytes, contents))
    }

    fn at_end(&self) -> bool {
        self.contents.is_empty()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn der_test_empty_input() {
        let input = Vec::new();
        let mut der = Der::new(&input);
        assert!(der.read_tlv(INTEGER).is_err());
    }

    #[test]
    fn der_test_no_length() {
        let input = vec![INTEGER];
        let mut der = Der::new(&input);
        assert!(der.read_tlv(INTEGER).is_err());
    }

    #[test]
    fn der_test_empty_sequence() {
        let input = vec![SEQUENCE, 0];
        let mut der = Der::new(&input);
        let read_result = der.read_tlv(SEQUENCE);
        assert!(read_result.is_ok());
        let (tag, length, sequence_bytes) = read_result.unwrap();
        assert_eq!(tag, SEQUENCE);
        assert_eq!(length, vec![0]);
        assert_eq!(sequence_bytes.len(), 0);
        assert!(der.at_end());
    }

    #[test]
    fn der_test_not_at_end() {
        let input = vec![SEQUENCE, 0, 1];
        let mut der = Der::new(&input);
        let read_result = der.read_tlv(SEQUENCE);
        assert!(read_result.is_ok());
        let (tag, length, sequence_bytes) = read_result.unwrap();
        assert_eq!(tag, SEQUENCE);
        assert_eq!(length, vec![0]);
        assert_eq!(sequence_bytes.len(), 0);
        assert!(!der.at_end());
    }

    #[test]
    fn der_test_wrong_tag() {
        let input = vec![SEQUENCE, 0];
        let mut der = Der::new(&input);
        assert!(der.read_tlv(INTEGER).is_err());
    }

    #[test]
    fn der_test_truncated_two_byte_length() {
        let input = vec![SEQUENCE, 0x81];
        let mut der = Der::new(&input);
        assert!(der.read_tlv(SEQUENCE).is_err());
    }

    #[test]
    fn der_test_truncated_three_byte_length() {
        let input = vec![SEQUENCE, 0x82, 1];
        let mut der = Der::new(&input);
        assert!(der.read_tlv(SEQUENCE).is_err());
    }

    #[test]
    fn der_test_truncated_data() {
        let input = vec![SEQUENCE, 20, 1];
        let mut der = Der::new(&input);
        assert!(der.read_tlv(SEQUENCE).is_err());
    }

    #[test]
    fn der_test_sequence() {
        let input = vec![
            SEQUENCE, 20, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 0, 0,
        ];
        let mut der = Der::new(&input);
        let result = der.read_tlv(SEQUENCE);
        assert!(result.is_ok());
        let (tag, length, value) = result.unwrap();
        assert_eq!(tag, SEQUENCE);
        assert_eq!(length, vec![20]);
        assert_eq!(
            value,
            [1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 0, 0]
        );
        assert!(der.at_end());
    }

    #[test]
    fn der_test_not_shortest_two_byte_length_encoding() {
        let input = vec![SEQUENCE, 0x81, 1, 1];
        let mut der = Der::new(&input);
        assert!(der.read_tlv(SEQUENCE).is_err());
    }

    #[test]
    fn der_test_not_shortest_three_byte_length_encoding() {
        let input = vec![SEQUENCE, 0x82, 0, 1, 1];
        let mut der = Der::new(&input);
        assert!(der.read_tlv(SEQUENCE).is_err());
    }

    #[test]
    fn der_test_indefinite_length_unsupported() {
        let input = vec![SEQUENCE, 0x80, 1, 2, 3, 0x00, 0x00];
        let mut der = Der::new(&input);
        assert!(der.read_tlv(SEQUENCE).is_err());
    }

    #[test]
    fn der_test_input_too_long() {
        // This isn't valid DER (the contents of the SEQUENCE are truncated), but it demonstrates
        // that we don't try to read too much if we're given a long length (and also that we don't
        // support lengths 2^16 and up).
        let input = vec![SEQUENCE, 0x83, 0x01, 0x00, 0x01, 1, 1, 1, 1];
        let mut der = Der::new(&input);
        assert!(der.read_tlv(SEQUENCE).is_err());
    }

    #[test]
    fn empty_input_fails() {
        let empty = Vec::new();
        assert!(read_rsa_modulus(&empty).is_err());
        #[cfg(target_os = "macos")]
        assert!(read_ec_sig_point(&empty).is_err());
        assert!(read_encoded_serial_number(&empty).is_err());
    }

    #[test]
    fn empty_sequence_fails() {
        let empty = vec![SEQUENCE | CONSTRUCTED];
        assert!(read_rsa_modulus(&empty).is_err());
        #[cfg(target_os = "macos")]
        assert!(read_ec_sig_point(&empty).is_err());
        assert!(read_encoded_serial_number(&empty).is_err());
    }

    #[test]
    fn test_read_rsa_modulus() {
        let rsa_key = include_bytes!("../test/rsa.bin");
        let result = read_rsa_modulus(rsa_key);
        assert!(result.is_ok());
        let modulus = result.unwrap();
        assert_eq!(modulus, include_bytes!("../test/modulus.bin").to_vec());
    }

    #[test]
    fn test_read_serial_number() {
        let certificate = include_bytes!("../test/certificate.bin");
        let result = read_encoded_serial_number(certificate);
        assert!(result.is_ok());
        let serial_number = result.unwrap();
        assert_eq!(
            serial_number,
            &[
                0x02, 0x14, 0x3f, 0xed, 0x7b, 0x43, 0x47, 0x8a, 0x53, 0x42, 0x5b, 0x0d, 0x50, 0xe1,
                0x37, 0x88, 0x2a, 0x20, 0x3f, 0x31, 0x17, 0x20
            ]
        );
    }

    #[test]
    #[cfg(target_os = "windows")]
    fn test_read_digest() {
        // SEQUENCE
        //   SEQUENCE
        //     OBJECT IDENTIFIER 2.16.840.1.101.3.4.2.1 sha-256
        //       NULL
        //   OCTET STRING 1A7FCDB9A5F649F954885CFE145F3E93F0D1FA72BE980CC6EC82C70E1407C7D2
        let digest_info = [
            0x30, 0x31, 0x30, 0x0d, 0x06, 0x09, 0x60, 0x86, 0x48, 0x1, 0x65, 0x03, 0x04, 0x02,
            0x01, 0x05, 0x00, 0x04, 0x20, 0x1a, 0x7f, 0xcd, 0xb9, 0xa5, 0xf6, 0x49, 0xf9, 0x54,
            0x88, 0x5c, 0xfe, 0x14, 0x5f, 0x3e, 0x93, 0xf0, 0xd1, 0xfa, 0x72, 0xbe, 0x98, 0x0c,
            0xc6, 0xec, 0x82, 0xc7, 0x0e, 0x14, 0x07, 0xc7, 0xd2,
        ];
        let result = read_digest(&digest_info);
        assert!(result.is_ok());
        let digest = result.unwrap();
        assert_eq!(
            digest,
            &[
                0x1a, 0x7f, 0xcd, 0xb9, 0xa5, 0xf6, 0x49, 0xf9, 0x54, 0x88, 0x5c, 0xfe, 0x14, 0x5f,
                0x3e, 0x93, 0xf0, 0xd1, 0xfa, 0x72, 0xbe, 0x98, 0x0c, 0xc6, 0xec, 0x82, 0xc7, 0x0e,
                0x14, 0x07, 0xc7, 0xd2
            ]
        );
    }
}
