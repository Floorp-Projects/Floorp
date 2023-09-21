use super::CryptoError;

pub const TAG_INTEGER: u8 = 0x02;
pub const TAG_BIT_STRING: u8 = 0x03;
#[cfg(all(test, feature = "crypto_nss"))]
pub const TAG_OCTET_STRING: u8 = 0x04;
pub const TAG_NULL: u8 = 0x05;
pub const TAG_OBJECT_ID: u8 = 0x06;
pub const TAG_SEQUENCE: u8 = 0x30;

// Object identifiers in DER tag-length-value form
pub const OID_EC_PUBLIC_KEY_BYTES: &[u8] = &[
    /* RFC 5480 (id-ecPublicKey) */
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x02, 0x01,
];
pub const OID_SECP256R1_BYTES: &[u8] = &[
    /* RFC 5480 (secp256r1) */
    0x2a, 0x86, 0x48, 0xce, 0x3d, 0x03, 0x01, 0x07,
];
pub const OID_ED25519_BYTES: &[u8] = &[/* RFC 8410 (id-ed25519) */ 0x2b, 0x65, 0x70];
pub const OID_RS256_BYTES: &[u8] = &[
    /* RFC 4055 (sha256WithRSAEncryption) */
    0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01, 0x0b,
];

pub type Result<T> = std::result::Result<T, CryptoError>;

const MAX_TAG_AND_LENGTH_BYTES: usize = 4;
fn write_tag_and_length(out: &mut Vec<u8>, tag: u8, len: usize) -> Result<()> {
    if len > 0xFFFF {
        return Err(CryptoError::LibraryFailure);
    }
    out.push(tag);
    if len > 0xFF {
        out.push(0x82);
        out.push((len >> 8) as u8);
        out.push(len as u8);
    } else if len > 0x7F {
        out.push(0x81);
        out.push(len as u8);
    } else {
        out.push(len as u8);
    }
    Ok(())
}

pub fn integer(val: &[u8]) -> Result<Vec<u8>> {
    if val.is_empty() {
        return Err(CryptoError::MalformedInput);
    }
    // trim leading zeros, leaving a single zero if the input is the zero vector.
    let mut val = val;
    while val.len() > 1 && val[0] == 0 {
        val = &val[1..];
    }
    let mut out = Vec::with_capacity(MAX_TAG_AND_LENGTH_BYTES + 1 + val.len());
    if val[0] & 0x80 != 0 {
        // needs zero prefix
        write_tag_and_length(&mut out, TAG_INTEGER, 1 + val.len())?;
        out.push(0x00);
        out.extend_from_slice(val);
    } else {
        write_tag_and_length(&mut out, TAG_INTEGER, val.len())?;
        out.extend_from_slice(val);
    }
    Ok(out)
}

pub fn bit_string(val: &[u8]) -> Result<Vec<u8>> {
    let mut out = Vec::with_capacity(MAX_TAG_AND_LENGTH_BYTES + 1 + val.len());
    write_tag_and_length(&mut out, TAG_BIT_STRING, 1 + val.len())?;
    out.push(0x00); // trailing bits aren't supported
    out.extend_from_slice(val);
    Ok(out)
}

pub fn null() -> Result<Vec<u8>> {
    let mut out = Vec::with_capacity(MAX_TAG_AND_LENGTH_BYTES);
    write_tag_and_length(&mut out, TAG_NULL, 0)?;
    Ok(out)
}

pub fn object_id(val: &[u8]) -> Result<Vec<u8>> {
    let mut out = Vec::with_capacity(MAX_TAG_AND_LENGTH_BYTES + val.len());
    write_tag_and_length(&mut out, TAG_OBJECT_ID, val.len())?;
    out.extend_from_slice(val);
    Ok(out)
}

pub fn sequence(items: &[&[u8]]) -> Result<Vec<u8>> {
    let len = items.iter().map(|i| i.len()).sum();
    let mut out = Vec::with_capacity(MAX_TAG_AND_LENGTH_BYTES + len);
    write_tag_and_length(&mut out, TAG_SEQUENCE, len)?;
    for item in items {
        out.extend_from_slice(item);
    }
    Ok(out)
}

#[cfg(all(test, feature = "crypto_nss"))]
pub fn octet_string(val: &[u8]) -> Result<Vec<u8>> {
    let mut out = Vec::with_capacity(MAX_TAG_AND_LENGTH_BYTES + val.len());
    write_tag_and_length(&mut out, TAG_OCTET_STRING, val.len())?;
    out.extend_from_slice(val);
    Ok(out)
}

#[cfg(all(test, feature = "crypto_nss"))]
pub fn context_specific_explicit_tag(tag: u8, content: &[u8]) -> Result<Vec<u8>> {
    let mut out = Vec::with_capacity(MAX_TAG_AND_LENGTH_BYTES + content.len());
    write_tag_and_length(&mut out, 0xa0 + tag, content.len())?;
    out.extend_from_slice(content);
    Ok(out)
}

// Given "tag || len || value || rest" where tag and len are of length one, len is in [0, 127],
// and value is of length len, returns (value, rest)
#[cfg(all(test, feature = "crypto_nss"))]
fn expect_tag_with_short_len(tag: u8, z: &[u8]) -> Result<(&[u8], &[u8])> {
    if z.is_empty() {
        return Err(CryptoError::MalformedInput);
    }
    let (h, z) = z.split_at(1);
    if h[0] != tag || z.is_empty() {
        return Err(CryptoError::MalformedInput);
    }
    let (h, z) = z.split_at(1);
    if h[0] >= 0x80 || h[0] as usize > z.len() {
        return Err(CryptoError::MalformedInput);
    }
    Ok(z.split_at(h[0] as usize))
}

// Given a DER encoded RFC 3279 Ecdsa-Sig-Value,
//   Ecdsa-Sig-Value  ::=  SEQUENCE  {
//        r     INTEGER,
//        s     INTEGER },
// with r and s < 2^256, returns a 64 byte array containing
// r and s encoded as 32 byte zero-padded big endian unsigned
// integers
#[cfg(all(test, feature = "crypto_nss"))]
pub fn read_p256_sig(z: &[u8]) -> Result<Vec<u8>> {
    // Strip the tag and length.
    let (z, rest) = expect_tag_with_short_len(TAG_SEQUENCE, z)?;

    // The input should not have any trailing data.
    if !rest.is_empty() {
        return Err(CryptoError::MalformedInput);
    }

    let read_u256 = |z| -> Result<(&[u8], &[u8])> {
        let (r, z) = expect_tag_with_short_len(TAG_INTEGER, z)?;
        // We're expecting r < 2^256, so no more than 33 bytes as a signed integer.
        if r.is_empty() || r.len() > 33 {
            return Err(CryptoError::MalformedInput);
        }
        // If it is 33 bytes the leading byte must be zero.
        if r.len() == 33 && r[0] != 0 {
            return Err(CryptoError::MalformedInput);
        }
        // Ensure r is no more than 32 bytes.
        if r.len() == 33 {
            Ok((&r[1..], z))
        } else {
            Ok((r, z))
        }
    };

    let (r, z) = read_u256(z)?;
    let (s, z) = read_u256(z)?;

    // We should have consumed the entire buffer
    if !z.is_empty() {
        return Err(CryptoError::MalformedInput);
    }

    // Left pad each integer with zeros to length 32 and concatenate the results
    let mut out = vec![0u8; 64];
    {
        let (r_out, s_out) = out.split_at_mut(32);
        r_out[32 - r.len()..].copy_from_slice(r);
        s_out[32 - s.len()..].copy_from_slice(s);
    }
    Ok(out)
}
