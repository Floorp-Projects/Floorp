/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/// The Send Tab functionality is backed by Firefox Accounts device commands.
/// A device shows it can handle "Send Tab" commands by advertising the "open-uri"
/// command in its on own device record.
/// This command data bundle contains a one-time generated `PublicSendTabKeys`
/// (while keeping locally `PrivateSendTabKeys` containing the private key),
/// wrapped by the account oldsync scope `kSync` to form a `SendTabKeysPayload`.
///
/// When a device sends a tab to another, it decrypts that `SendTabKeysPayload` using `kSync`,
/// uses the obtained public key to encrypt the `SendTabPayload` it created that
/// contains the tab to send and finally forms the `EncryptedSendTabPayload` that is
/// then sent to the target device.
use crate::{device::Device, error::*, scoped_keys::ScopedKey, scopes, telemetry};
use rc_crypto::ece::{self, Aes128GcmEceWebPush, EcKeyComponents, WebPushParams};
use rc_crypto::ece_crypto::{RcCryptoLocalKeyPair, RcCryptoRemotePublicKey};
use serde_derive::*;
use sync15::{EncryptedPayload, KeyBundle};

pub const COMMAND_NAME: &str = "https://identity.mozilla.com/cmd/open-uri";

#[derive(Debug, Serialize, Deserialize)]
pub struct EncryptedSendTabPayload {
    /// URL Safe Base 64 encrypted send-tab payload.
    encrypted: String,
}

impl EncryptedSendTabPayload {
    pub(crate) fn decrypt(self, keys: &PrivateSendTabKeysV1) -> Result<SendTabPayload> {
        rc_crypto::ensure_initialized();
        let encrypted = base64::decode_config(&self.encrypted, base64::URL_SAFE_NO_PAD)?;
        let private_key = RcCryptoLocalKeyPair::from_raw_components(&keys.p256key)?;
        let decrypted = Aes128GcmEceWebPush::decrypt(&private_key, &keys.auth_secret, &encrypted)?;
        Ok(serde_json::from_slice(&decrypted)?)
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct SendTabPayload {
    pub entries: Vec<TabHistoryEntry>,
    #[serde(rename = "flowID", default)]
    pub flow_id: String,
    #[serde(rename = "streamID", default)]
    pub stream_id: String,
}

impl SendTabPayload {
    pub fn single_tab(title: &str, url: &str) -> (Self, telemetry::SentCommand) {
        let sent_telemetry: telemetry::SentCommand = Default::default();
        (
            SendTabPayload {
                entries: vec![TabHistoryEntry {
                    title: title.to_string(),
                    url: url.to_string(),
                }],
                flow_id: sent_telemetry.flow_id.clone(),
                stream_id: sent_telemetry.stream_id.clone(),
            },
            sent_telemetry,
        )
    }
    fn encrypt(&self, keys: PublicSendTabKeys) -> Result<EncryptedSendTabPayload> {
        rc_crypto::ensure_initialized();
        let bytes = serde_json::to_vec(&self)?;
        let public_key = base64::decode_config(&keys.public_key, base64::URL_SAFE_NO_PAD)?;
        let public_key = RcCryptoRemotePublicKey::from_raw(&public_key)?;
        let auth_secret = base64::decode_config(&keys.auth_secret, base64::URL_SAFE_NO_PAD)?;
        let encrypted = Aes128GcmEceWebPush::encrypt(
            &public_key,
            &auth_secret,
            &bytes,
            WebPushParams::default(),
        )?;
        let encrypted = base64::encode_config(&encrypted, base64::URL_SAFE_NO_PAD);
        Ok(EncryptedSendTabPayload { encrypted })
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct TabHistoryEntry {
    pub title: String,
    pub url: String,
}

#[derive(Serialize, Deserialize, Clone)]
pub(crate) enum VersionnedPrivateSendTabKeys {
    V1(PrivateSendTabKeysV1),
}

#[derive(Serialize, Deserialize, Clone)]
pub(crate) struct PrivateSendTabKeysV1 {
    p256key: EcKeyComponents,
    auth_secret: Vec<u8>,
}
pub(crate) type PrivateSendTabKeys = PrivateSendTabKeysV1;

impl PrivateSendTabKeys {
    // We define this method so the type-checker prevents us from
    // trying to serialize `PrivateSendTabKeys` directly since
    // `serde_json::to_string` would compile because both types derive
    // `Serialize`.
    pub(crate) fn serialize(&self) -> Result<String> {
        Ok(serde_json::to_string(&VersionnedPrivateSendTabKeys::V1(
            self.clone(),
        ))?)
    }

    pub(crate) fn deserialize(s: &str) -> Result<Self> {
        let versionned: VersionnedPrivateSendTabKeys = serde_json::from_str(s)?;
        match versionned {
            VersionnedPrivateSendTabKeys::V1(prv_key) => Ok(prv_key),
        }
    }
}

impl PrivateSendTabKeys {
    pub fn from_random() -> Result<Self> {
        rc_crypto::ensure_initialized();
        let (key_pair, auth_secret) = ece::generate_keypair_and_auth_secret()?;
        Ok(Self {
            p256key: key_pair.raw_components()?,
            auth_secret: auth_secret.to_vec(),
        })
    }
}

#[derive(Serialize, Deserialize)]
pub(crate) struct SendTabKeysPayload {
    /// Hex encoded kid.
    kid: String,
    /// Base 64 encoded IV.
    #[serde(rename = "IV")]
    iv: String,
    /// Hex encoded hmac.
    hmac: String,
    /// Base 64 encoded ciphertext.
    ciphertext: String,
}

impl SendTabKeysPayload {
    pub(crate) fn decrypt(self, scoped_key: &ScopedKey) -> Result<PublicSendTabKeys> {
        let (ksync, kxcs) = extract_oldsync_key_components(scoped_key)?;
        if hex::decode(self.kid)? != kxcs {
            return Err(ErrorKind::MismatchedKeys.into());
        }
        let key = KeyBundle::from_ksync_bytes(&ksync)?;
        let encrypted_bso = EncryptedPayload {
            iv: self.iv,
            hmac: self.hmac,
            ciphertext: self.ciphertext,
        };
        Ok(encrypted_bso.decrypt_and_parse_payload(&key)?)
    }
}

#[derive(Serialize, Deserialize)]
pub struct PublicSendTabKeys {
    /// URL Safe Base 64 encoded push public key.
    #[serde(rename = "publicKey")]
    public_key: String,
    /// URL Safe Base 64 encoded auth secret.
    #[serde(rename = "authSecret")]
    auth_secret: String,
}

impl PublicSendTabKeys {
    fn encrypt(&self, scoped_key: &ScopedKey) -> Result<SendTabKeysPayload> {
        let (ksync, kxcs) = extract_oldsync_key_components(scoped_key)?;
        let key = KeyBundle::from_ksync_bytes(&ksync)?;
        let encrypted_payload = EncryptedPayload::from_cleartext_payload(&key, &self)?;
        Ok(SendTabKeysPayload {
            kid: hex::encode(kxcs),
            iv: encrypted_payload.iv,
            hmac: encrypted_payload.hmac,
            ciphertext: encrypted_payload.ciphertext,
        })
    }
    pub fn as_command_data(&self, scoped_key: &ScopedKey) -> Result<String> {
        let encrypted_public_keys = self.encrypt(scoped_key)?;
        Ok(serde_json::to_string(&encrypted_public_keys)?)
    }
    pub(crate) fn public_key(&self) -> &str {
        &self.public_key
    }
    pub(crate) fn auth_secret(&self) -> &str {
        &self.auth_secret
    }
}

impl From<PrivateSendTabKeys> for PublicSendTabKeys {
    fn from(internal: PrivateSendTabKeys) -> Self {
        Self {
            public_key: base64::encode_config(
                &internal.p256key.public_key(),
                base64::URL_SAFE_NO_PAD,
            ),
            auth_secret: base64::encode_config(&internal.auth_secret, base64::URL_SAFE_NO_PAD),
        }
    }
}

pub fn build_send_command(
    scoped_key: &ScopedKey,
    target: &Device,
    send_tab_payload: &SendTabPayload,
) -> Result<serde_json::Value> {
    let command = target
        .available_commands
        .get(COMMAND_NAME)
        .ok_or_else(|| ErrorKind::UnsupportedCommand(COMMAND_NAME))?;
    let bundle: SendTabKeysPayload = serde_json::from_str(command)?;
    let public_keys = bundle.decrypt(scoped_key)?;
    let encrypted_payload = send_tab_payload.encrypt(public_keys)?;
    Ok(serde_json::to_value(&encrypted_payload)?)
}

fn extract_oldsync_key_components(oldsync_key: &ScopedKey) -> Result<(Vec<u8>, Vec<u8>)> {
    if oldsync_key.scope != scopes::OLD_SYNC {
        return Err(ErrorKind::IllegalState(
            "Only oldsync scoped keys are supported at the moment.",
        )
        .into());
    }
    let kxcs: &str = oldsync_key.kid.splitn(2, '-').collect::<Vec<_>>()[1];
    let kxcs = base64::decode_config(&kxcs, base64::URL_SAFE_NO_PAD)?;
    let ksync = oldsync_key.key_bytes()?;
    Ok((ksync, kxcs))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_minimal_parse_payload() {
        let minimal = r#"{ "entries": []}"#;
        let payload: SendTabPayload = serde_json::from_str(minimal).expect("should work");
        assert_eq!(payload.flow_id, "".to_string());
    }

    #[test]
    fn test_payload() {
        let (payload, telem) = SendTabPayload::single_tab("title", "http://example.com");
        let json = serde_json::to_string(&payload).expect("should work");
        assert_eq!(telem.flow_id.len(), 12);
        assert_eq!(telem.stream_id.len(), 12);
        assert_ne!(telem.flow_id, telem.stream_id);
        let p2: SendTabPayload = serde_json::from_str(&json).expect("should work");
        // no 'PartialEq' derived so check each field individually...
        assert_eq!(payload.entries[0].url, "http://example.com".to_string());
        assert_eq!(payload.flow_id, p2.flow_id);
        assert_eq!(payload.stream_id, p2.stream_id);
    }
}
