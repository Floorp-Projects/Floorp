/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub use crate::oauth::{AuthorizationPKCEParams, AuthorizationParameters};
use crate::{error::*, http_client, scoped_keys::ScopedKey, util::Xorable, Config};
pub use http_client::{
    derive_auth_key_from_session_token, send_authorization_request, send_verification,
    AuthorizationRequestParameters,
};
use jwcrypto::{EncryptionAlgorithm, EncryptionParameters};
use rc_crypto::{digest, hkdf, hmac, pbkdf2};
use serde_derive::{Deserialize, Serialize};
use std::collections::HashMap;
pub fn get_sync_keys(
    config: &Config,
    key_fetch_token: &str,
    email: &str,
    pw: &str,
) -> Result<(Vec<u8>, Vec<u8>)> {
    let acct_keys = get_account_keys(config, key_fetch_token)?;
    let wrap_kb = &acct_keys[32..];
    let sync_key = derive_sync_key(email, pw, wrap_kb)?;
    let xcs_key = derive_xcs_key(email, pw, wrap_kb)?;
    Ok((sync_key, xcs_key))
}

pub fn create_keys_jwe(
    client_id: &str,
    scope: &str,
    jwk: &str,
    auth_key: &[u8],
    config: &Config,
    acct_keys: (&[u8], &[u8]),
) -> anyhow::Result<String> {
    let scoped: HashMap<String, ScopedKey> =
        get_scoped_keys(scope, client_id, auth_key, config, acct_keys)?;
    let scoped = serde_json::to_string(&scoped)?;
    let scoped = scoped.as_bytes();
    let jwk = serde_json::from_str(jwk)?;
    let res = jwcrypto::encrypt_to_jwe(
        scoped,
        EncryptionParameters::ECDH_ES {
            enc: EncryptionAlgorithm::A256GCM,
            peer_jwk: &jwk,
        },
    )?;
    Ok(res)
}

#[derive(Serialize, Deserialize)]
struct Epk {
    crv: String,
    kty: String,
    x: String,
    y: String,
}

fn kwe(name: &str, email: &str) -> Vec<u8> {
    format!("identity.mozilla.com/picl/v1/{}:{}", name, email)
        .as_bytes()
        .to_vec()
}

fn kw(name: &str) -> Vec<u8> {
    format!("identity.mozilla.com/picl/v1/{}", name)
        .as_bytes()
        .to_vec()
}

pub fn get_scoped_keys(
    scope: &str,
    client_id: &str,
    auth_key: &[u8],
    config: &Config,
    acct_keys: (&[u8], &[u8]),
) -> anyhow::Result<HashMap<String, ScopedKey>> {
    let key_data = http_client::get_scoped_key_data_response(scope, client_id, auth_key, config)?;
    let mut scoped_keys: HashMap<String, ScopedKey> = HashMap::new();
    key_data
        .as_object()
        .ok_or_else(|| anyhow::Error::msg("Key data not an object"))?
        .keys()
        .try_for_each(|key| -> anyhow::Result<()> {
            let val = key_data
                .as_object()
                .ok_or_else(|| anyhow::Error::msg("Key data not an object"))?
                .get(key)
                .ok_or_else(|| anyhow::Error::msg("Key does not exist"))?;
            scoped_keys.insert(key.clone(), get_key_for_scope(&key, val, acct_keys)?);
            Ok(())
        })?;
    Ok(scoped_keys)
}

fn get_key_for_scope(
    key: &str,
    val: &serde_json::Value,
    acct_keys: (&[u8], &[u8]),
) -> anyhow::Result<ScopedKey> {
    let (sync_key, xcs_key) = acct_keys;
    let sync_key = base64::encode_config(sync_key, base64::URL_SAFE_NO_PAD);
    let xcs_key = base64::encode_config(xcs_key, base64::URL_SAFE_NO_PAD);
    let kid = format!(
        "{}-{}",
        val.as_object()
            .ok_or_else(|| anyhow::Error::msg("Json is not an object"))?
            .get("keyRotationTimestamp")
            .ok_or_else(|| anyhow::Error::msg("Key rotation timestamp doesn't exist"))?
            .as_u64()
            .ok_or_else(|| anyhow::Error::msg("Key rotation timestamp is not a number"))?,
        xcs_key
    );
    Ok(ScopedKey {
        scope: key.to_string(),
        kid,
        k: sync_key,
        kty: "oct".to_string(),
    })
}

fn derive_xcs_key(email: &str, pwd: &str, wrap_kb: &[u8]) -> Result<Vec<u8>> {
    let unwrap_kb = derive_unwrap_kb(email, pwd)?;
    let kb = xored(wrap_kb, &unwrap_kb)?;
    Ok(sha256(&kb)?[0..16].into())
}

fn sha256(kb: &[u8]) -> Result<Vec<u8>> {
    let ret = digest::digest(&digest::SHA256, kb)?;
    let ret: &[u8] = ret.as_ref();
    Ok(ret.to_vec())
}

fn derive_hkdf_sha256_key(ikm: &[u8], salt: &[u8], info: &[u8], len: usize) -> Result<Vec<u8>> {
    let salt = hmac::SigningKey::new(&digest::SHA256, salt);
    let mut out = vec![0u8; len];
    hkdf::extract_and_expand(&salt, ikm, info, &mut out)?;
    Ok(out)
}

fn quick_strech_pwd(email: &str, pwd: &str) -> Result<Vec<u8>> {
    let salt = kwe("quickStretch", email);
    let mut out = [0u8; 32];
    pbkdf2::derive(
        pwd.as_bytes(),
        &salt,
        1000,
        pbkdf2::HashAlgorithm::SHA256,
        &mut out,
    )?;
    Ok(out.to_vec())
}

pub fn auth_pwd(email: &str, pwd: &str) -> Result<String> {
    let streched = quick_strech_pwd(email, pwd)?;
    let salt = b"";
    let context = kw("authPW");
    let derived = derive_hkdf_sha256_key(&streched, salt, &context, 32)?;
    Ok(hex::encode(derived))
}

#[derive(Serialize, Deserialize)]
struct Credentials {
    key: Vec<u8>,
    id: Vec<u8>,
    extra: Vec<u8>,
    out: Vec<u8>,
}

fn derive_hawk_credentials(token_hex: &str, context: &str, size: usize) -> Result<Credentials> {
    let token = hex::decode(token_hex)?;
    let out = derive_hkdf_sha256_key(&token, &[0u8; 0], &kw(context), size)?;
    let key = out[32..64].to_vec();
    let extra = out[64..].to_vec();
    Ok(Credentials {
        key,
        id: out[0..32].to_vec(),
        extra,
        out: out.to_vec(),
    })
}

fn xored(a: &[u8], b: &[u8]) -> Result<Vec<u8>> {
    a.xored_with(b)
}

fn derive_unwrap_kb(email: &str, pwd: &str) -> Result<Vec<u8>> {
    let streched_pw = quick_strech_pwd(email, pwd)?;
    let out = derive_hkdf_sha256_key(&streched_pw, &[0u8; 0], &kw("unwrapBkey"), 32)?;
    Ok(out)
}

fn derive_sync_key(email: &str, pwd: &str, wrap_kb: &[u8]) -> Result<Vec<u8>> {
    let unwrap_kb = derive_unwrap_kb(email, pwd)?;
    let kb = xored(wrap_kb, &unwrap_kb)?;
    derive_hkdf_sha256_key(
        &kb,
        &[0u8; 0],
        "identity.mozilla.com/picl/v1/oldsync".as_bytes(),
        64,
    )
}

fn get_account_keys(config: &Config, key_fetch_token: &str) -> Result<Vec<u8>> {
    let creds = derive_hawk_credentials(key_fetch_token, "keyFetchToken", 96)?;
    let key_request_key = &creds.extra[0..32];
    let more_creds = derive_hkdf_sha256_key(key_request_key, &[0u8; 0], &kw("account/keys"), 96)?;
    let _resp_hmac_key = &more_creds[0..32];
    let resp_xor_key = &more_creds[32..96];
    let bundle = http_client::get_keys_bundle(&config, &creds.out)?;
    // Missing MAC matching since this is only for tests
    xored(resp_xor_key, &bundle[0..64])
}

#[cfg(test)]
mod tests {

    // Test vectors used from https://github.com/mozilla/fxa-auth-server/wiki/onepw-protocol#test-vectors
    use super::*;
    const EMAIL: &str = "andré@example.org";
    const PASSWORD: &str = "pässwörd";
    #[test]
    fn test_derive_quick_stretch() {
        let qs = quick_strech_pwd(EMAIL, PASSWORD).unwrap();
        let expected = "e4e8889bd8bd61ad6de6b95c059d56e7b50dacdaf62bd84644af7e2add84345d";
        assert_eq!(expected, hex::encode(qs));
    }

    #[test]
    fn test_auth_pw() {
        let auth_pw = auth_pwd(EMAIL, PASSWORD).unwrap();
        let expected = "247b675ffb4c46310bc87e26d712153abe5e1c90ef00a4784594f97ef54f2375";
        assert_eq!(auth_pw, expected);
    }

    #[test]
    fn test_derive_unwrap_kb() {
        let unwrap_kb = derive_unwrap_kb(EMAIL, PASSWORD).unwrap();
        let expected = "de6a2648b78284fcb9ffa81ba95803309cfba7af583c01a8a1a63e567234dd28";
        assert_eq!(hex::encode(unwrap_kb), expected);
    }

    #[test]
    fn test_kb() {
        let wrap_kb =
            hex::decode("7effe354abecbcb234a8dfc2d7644b4ad339b525589738f2d27341bb8622ecd8")
                .unwrap();
        let unwrap_kb =
            hex::decode("de6a2648b78284fcb9ffa81ba95803309cfba7af583c01a8a1a63e567234dd28")
                .unwrap();
        let kb = xored(&wrap_kb, &unwrap_kb).unwrap();
        let expected = "a095c51c1c6e384e8d5777d97e3c487a4fc2128a00ab395a73d57fedf41631f0";
        assert_eq!(expected, hex::encode(kb));
    }
}
