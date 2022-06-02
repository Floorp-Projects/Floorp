/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at https://mozilla.org/MPL/2.0/. */

//! Implements a simple processor for
//! https://github.com/chromium/chromium/blob/d7da0240cae77824d1eda25745c4022757499131/third_party/blink/public/common/origin_trials/origin_trials_token_structure.md
//!
//! This crate intentionally leaves the cryptography to the caller. See the
//! tools/ directory for example usages.


/// Latest version as documented.
pub const LATEST_VERSION: u8 = 3;

#[repr(C)]
pub struct RawToken {
    version: u8,
    signature: [u8; 64],
    payload_length: [u8; 4],
    /// Payload is an slice of payload_length bytes, and has to be verified
    /// before returning it to the caller.
    payload: [u8; 0],
}

#[derive(Debug)]
pub enum TokenValidationError {
    BufferTooSmall,
    MismatchedPayloadSize { expected: usize, actual: usize },
    InvalidSignature,
    UnknownVersion,
    UnsupportedThirdPartyToken,
    UnexpectedUsageInNonThirdPartyToken,
    MalformedPayload(serde_json::Error),
}

impl RawToken {
    const HEADER_SIZE: usize = std::mem::size_of::<Self>();

    #[inline]
    pub fn version(&self) -> u8 {
        self.version
    }

    #[inline]
    pub fn signature(&self) -> &[u8; 64] {
        &self.signature
    }

    #[inline]
    pub fn payload_length(&self) -> usize {
        u32::from_be_bytes(self.payload_length) as usize
    }

    #[inline]
    pub fn as_buffer(&self) -> &[u8] {
        let buffer_size = Self::HEADER_SIZE + self.payload_length();
        unsafe { std::slice::from_raw_parts(self as *const _ as *const u8, buffer_size) }
    }

    #[inline]
    pub fn payload(&self) -> &[u8] {
        let len = self.payload_length();
        unsafe { std::slice::from_raw_parts(self.payload.as_ptr(), len) }
    }

    /// Returns a RawToken from a raw buffer.
    pub fn from_buffer<'a>(buffer: &'a [u8]) -> Result<&'a Self, TokenValidationError> {
        if buffer.len() <= Self::HEADER_SIZE {
            return Err(TokenValidationError::BufferTooSmall);
        }
        assert_eq!(
            std::mem::align_of::<Self>(),
            1,
            "RawToken is a view over the buffer"
        );
        let raw_token = unsafe { &*(buffer.as_ptr() as *const Self) };
        let payload = &buffer[Self::HEADER_SIZE..];
        let expected = raw_token.payload_length();
        let actual = payload.len();
        if expected != actual {
            return Err(TokenValidationError::MismatchedPayloadSize { expected, actual });
        }
        Ok(raw_token)
    }

    /// The data to verify the signature in this raw token.
    fn signature_data(&self) -> Vec<u8> {
        Self::raw_signature_data(self.version, self.payload())
    }

    /// The data to sign or verify given a payload and a version.
    fn raw_signature_data(version: u8, payload: &[u8]) -> Vec<u8> {
        let mut data = Vec::with_capacity(payload.len() + 5);
        data.push(version);
        data.extend((payload.len() as u32).to_be_bytes());
        data.extend(payload);
        data
    }

    /// Verify the signature of this raw token.
    pub fn verify(&self, verify_signature: impl FnOnce(&[u8; 64], &[u8]) -> bool) -> bool {
        let signature_data = self.signature_data();
        verify_signature(&self.signature, &signature_data)
    }
}

#[derive(serde::Deserialize, serde::Serialize, Debug, Eq, PartialEq)]
#[serde(rename_all = "camelCase")]
pub enum Usage {
    #[serde(rename = "")]
    None,
    Subset,
}

impl Usage {
    fn is_none(&self) -> bool {
        *self == Self::None
    }
}

impl Default for Usage {
    fn default() -> Self {
        Self::None
    }
}

fn is_false(t: &bool) -> bool {
    *t == false
}

/// An already decoded and maybe-verified token.
#[derive(serde::Deserialize, serde::Serialize, Debug, Eq, PartialEq)]
#[serde(rename_all = "camelCase")]
pub struct Token {
    pub origin: String,
    pub feature: String,
    pub expiry: u64, // Timestamp. Seconds since epoch.
    #[serde(default, skip_serializing_if = "is_false")]
    pub is_subdomain: bool,
    #[serde(default, skip_serializing_if = "is_false")]
    pub is_third_party: bool,
    #[serde(default, skip_serializing_if = "Usage::is_none")]
    pub usage: Usage,
}

impl Token {
    #[inline]
    pub fn origin(&self) -> &str {
        &self.origin
    }

    #[inline]
    pub fn feature(&self) -> &str {
        &self.feature
    }

    #[inline]
    pub fn expiry_since_unix_epoch(&self) -> std::time::Duration {
        std::time::Duration::from_secs(self.expiry)
    }

    #[inline]
    pub fn expiry_time(&self) -> Option<std::time::SystemTime> {
        std::time::UNIX_EPOCH.checked_add(self.expiry_since_unix_epoch())
    }

    #[inline]
    pub fn is_expired(&self) -> bool {
        let now_duration = std::time::SystemTime::now()
            .duration_since(std::time::UNIX_EPOCH)
            .expect("System time before epoch?");
        now_duration >= self.expiry_since_unix_epoch()
    }

    /// Most high-level function: For a given buffer, tries to parse it and
    /// verify it as a token.
    pub fn from_buffer(
        buffer: &[u8],
        verify_signature: impl FnOnce(&[u8; 64], &[u8]) -> bool,
    ) -> Result<Self, TokenValidationError> {
        Self::from_raw_token(RawToken::from_buffer(buffer)?, verify_signature)
    }

    /// Validates a RawToken's signature and converts the token if valid.
    pub fn from_raw_token(
        token: &RawToken,
        verify_signature: impl FnOnce(&[u8; 64], &[u8]) -> bool,
    ) -> Result<Self, TokenValidationError> {
        if !token.verify(verify_signature) {
            return Err(TokenValidationError::InvalidSignature);
        }
        Self::from_raw_token_unverified(token)
    }

    /// Converts the token from a raw token, without verifying first.
    pub fn from_raw_token_unverified(token: &RawToken) -> Result<Self, TokenValidationError> {
        Self::from_payload(token.version, token.payload())
    }

    /// Converts the token from a raw payload, version pair.
    pub fn from_payload(version: u8, payload: &[u8]) -> Result<Self, TokenValidationError> {
        if version != 2 && version != 3 {
            assert_ne!(version, LATEST_VERSION);
            return Err(TokenValidationError::UnknownVersion);
        }

        let token: Token = match serde_json::from_slice(payload) {
            Ok(t) => t,
            Err(e) => return Err(TokenValidationError::MalformedPayload(e)),
        };

        // Third-party tokens are not supported in version 2.
        if token.is_third_party {
            if version == 2 {
                return Err(TokenValidationError::UnsupportedThirdPartyToken);
            }
        } else if !token.usage.is_none() {
            return Err(TokenValidationError::UnexpectedUsageInNonThirdPartyToken);
        }

        Ok(token)
    }

    /// Converts the token to a raw payload.
    pub fn to_payload(&self) -> Vec<u8> {
        serde_json::to_string(self)
            .expect("Should always be able to turn a token into a payload")
            .into_bytes()
    }

    /// Converts the token to the data that should be signed.
    pub fn to_signature_data(&self) -> Vec<u8> {
        RawToken::raw_signature_data(LATEST_VERSION, &self.to_payload())
    }

    /// Turns the token into a fully signed token.
    pub fn to_signed_token(&self, sign: impl FnOnce(&[u8]) -> [u8; 64]) -> Vec<u8> {
        self.to_signed_token_with_payload(sign, &self.to_payload())
    }

    /// DO NOT EXPOSE: This is intended for testing only. We need to test with
    /// the original payload so that the tokens match, but we assert
    /// that self.to_payload() and payload are equivalent.
    fn to_signed_token_with_payload(
        &self,
        sign: impl FnOnce(&[u8]) -> [u8; 64],
        payload: &[u8],
    ) -> Vec<u8> {
        let signature_data_with_payload = RawToken::raw_signature_data(LATEST_VERSION, &payload);
        let signature = sign(&signature_data_with_payload);

        let mut buffer = Vec::with_capacity(1 + signature.len() + 4 + payload.len());
        buffer.push(LATEST_VERSION);
        buffer.extend(signature);
        buffer.extend((payload.len() as u32).to_be_bytes());
        buffer.extend(payload);

        if cfg!(debug_assertions) {
            let token = Self::from_buffer(&buffer, |_, _| true).expect("Creating malformed token?");
            assert_eq!(self, &token, "Token differs after deserialization?");
        }

        buffer
    }
}

#[cfg(test)]
mod tests;
