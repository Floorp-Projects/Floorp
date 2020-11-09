/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

pub mod attached_clients;

use crate::{
    error::*,
    http_client::{AuthorizationRequestParameters, OAuthTokenResponse},
    scoped_keys::{ScopedKey, ScopedKeysFlow},
    util, FirefoxAccount,
};
use jwcrypto::{EncryptionAlgorithm, EncryptionParameters};
use rc_crypto::digest;
use serde_derive::*;
use std::convert::TryFrom;
use std::{
    collections::{HashMap, HashSet},
    iter::FromIterator,
    time::{SystemTime, UNIX_EPOCH},
};
use url::Url;
// If a cached token has less than `OAUTH_MIN_TIME_LEFT` seconds left to live,
// it will be considered already expired.
const OAUTH_MIN_TIME_LEFT: u64 = 60;
// Special redirect urn based on the OAuth native spec, signals that the
// WebChannel flow is used
pub const OAUTH_WEBCHANNEL_REDIRECT: &str = "urn:ietf:wg:oauth:2.0:oob:oauth-redirect-webchannel";

impl FirefoxAccount {
    /// Fetch a short-lived access token using the saved refresh token.
    /// If there is no refresh token held or if it is not authorized for some of the requested
    /// scopes, this method will error-out and a login flow will need to be initiated
    /// using `begin_oauth_flow`.
    ///
    /// * `scopes` - Space-separated list of requested scopes.
    /// * `ttl` - the ttl in seconds of the token requested from the server.
    ///
    /// **💾 This method may alter the persisted account state.**
    pub fn get_access_token(&mut self, scope: &str, ttl: Option<u64>) -> Result<AccessTokenInfo> {
        if scope.contains(' ') {
            return Err(ErrorKind::MultipleScopesRequested.into());
        }
        if let Some(oauth_info) = self.state.access_token_cache.get(scope) {
            if oauth_info.expires_at > util::now_secs() + OAUTH_MIN_TIME_LEFT {
                return Ok(oauth_info.clone());
            }
        }
        let resp = match self.state.refresh_token {
            Some(ref refresh_token) => {
                if refresh_token.scopes.contains(scope) {
                    self.client.access_token_with_refresh_token(
                        &self.state.config,
                        &refresh_token.token,
                        ttl,
                        &[scope],
                    )?
                } else {
                    return Err(ErrorKind::NoCachedToken(scope.to_string()).into());
                }
            }
            None => match self.state.session_token {
                Some(ref session_token) => self.client.access_token_with_session_token(
                    &self.state.config,
                    &session_token,
                    &[scope],
                )?,
                None => return Err(ErrorKind::NoCachedToken(scope.to_string()).into()),
            },
        };
        let since_epoch = SystemTime::now()
            .duration_since(UNIX_EPOCH)
            .map_err(|_| ErrorKind::IllegalState("Current date before Unix Epoch."))?;
        let expires_at = since_epoch.as_secs() + resp.expires_in;
        let token_info = AccessTokenInfo {
            scope: resp.scope,
            token: resp.access_token,
            key: self.state.scoped_keys.get(scope).cloned(),
            expires_at,
        };
        self.state
            .access_token_cache
            .insert(scope.to_string(), token_info.clone());
        Ok(token_info)
    }

    /// Retrieve the current session token from state
    pub fn get_session_token(&self) -> Result<String> {
        match self.state.session_token {
            Some(ref session_token) => Ok(session_token.to_string()),
            None => Err(ErrorKind::NoSessionToken.into()),
        }
    }

    /// Check whether user is authorized using our refresh token.
    pub fn check_authorization_status(&mut self) -> Result<IntrospectInfo> {
        let resp = match self.state.refresh_token {
            Some(ref refresh_token) => {
                self.auth_circuit_breaker.check()?;
                self.client
                    .oauth_introspect_refresh_token(&self.state.config, &refresh_token.token)?
            }
            None => return Err(ErrorKind::NoRefreshToken.into()),
        };
        Ok(IntrospectInfo {
            active: resp.active,
        })
    }

    /// Initiate a pairing flow and return a URL that should be navigated to.
    ///
    /// * `pairing_url` - A pairing URL obtained by scanning a QR code produced by
    /// the pairing authority.
    /// * `scopes` - Space-separated list of requested scopes by the pairing supplicant.
    /// * `entrypoint` - The entrypoint to be used for data collection
    /// * `metrics` - Optional parameters for metrics
    pub fn begin_pairing_flow(
        &mut self,
        pairing_url: &str,
        scopes: &[&str],
        entrypoint: &str,
        metrics: Option<MetricsParams>,
    ) -> Result<String> {
        let mut url = self.state.config.pair_supp_url()?;
        url.query_pairs_mut().append_pair("entrypoint", entrypoint);
        if let Some(metrics) = metrics {
            metrics.append_params_to_url(&mut url);
        }
        let pairing_url = Url::parse(pairing_url)?;
        if url.host_str() != pairing_url.host_str() {
            return Err(ErrorKind::OriginMismatch.into());
        }
        url.set_fragment(pairing_url.fragment());
        self.oauth_flow(url, scopes)
    }

    /// Initiate an OAuth login flow and return a URL that should be navigated to.
    ///
    /// * `scopes` - Space-separated list of requested scopes.
    /// * `entrypoint` - The entrypoint to be used for metrics
    /// * `metrics` - Optional metrics parameters
    pub fn begin_oauth_flow(
        &mut self,
        scopes: &[&str],
        entrypoint: &str,
        metrics: Option<MetricsParams>,
    ) -> Result<String> {
        let mut url = if self.state.last_seen_profile.is_some() {
            self.state.config.oauth_force_auth_url()?
        } else {
            self.state.config.authorization_endpoint()?
        };

        url.query_pairs_mut()
            .append_pair("action", "email")
            .append_pair("response_type", "code")
            .append_pair("entrypoint", entrypoint);
        if let Some(metrics) = metrics {
            metrics.append_params_to_url(&mut url);
        }

        if let Some(ref cached_profile) = self.state.last_seen_profile {
            url.query_pairs_mut()
                .append_pair("email", &cached_profile.response.email);
        }

        let scopes: Vec<String> = match self.state.refresh_token {
            Some(ref refresh_token) => {
                // Union of the already held scopes and the one requested.
                let mut all_scopes: Vec<String> = vec![];
                all_scopes.extend(scopes.iter().map(ToString::to_string));
                let existing_scopes = refresh_token.scopes.clone();
                all_scopes.extend(existing_scopes);
                HashSet::<String>::from_iter(all_scopes)
                    .into_iter()
                    .collect()
            }
            None => scopes.iter().map(ToString::to_string).collect(),
        };
        let scopes: Vec<&str> = scopes.iter().map(<_>::as_ref).collect();
        self.oauth_flow(url, &scopes)
    }

    /// Fetch an OAuth code for a particular client using a session token from the account state.
    ///
    /// * `auth_params` Authorization parameters  which includes:
    ///     *  `client_id` - OAuth client id.
    ///     *  `scope` - list of requested scopes.
    ///     *  `state` - OAuth state.
    ///     *  `access_type` - Type of OAuth access, can be "offline" and "online"
    ///     *  `pkce_params` - Optional PKCE parameters for public clients (`code_challenge` and `code_challenge_method`)
    ///     *  `keys_jwk` - Optional JWK used to encrypt scoped keys
    pub fn authorize_code_using_session_token(
        &self,
        auth_params: AuthorizationParameters,
    ) -> Result<String> {
        let session_token = self.get_session_token()?;

        // Validate request to ensure that the client is actually allowed to request
        // the scopes they requested
        let allowed_scopes = self.client.scoped_key_data(
            &self.state.config,
            &session_token,
            &auth_params.client_id,
            &auth_params.scope.join(" "),
        )?;

        if let Some(not_allowed_scope) = auth_params
            .scope
            .iter()
            .find(|scope| !allowed_scopes.contains_key(*scope))
        {
            return Err(ErrorKind::ScopeNotAllowed(
                auth_params.client_id.clone(),
                not_allowed_scope.clone(),
            )
            .into());
        }

        let keys_jwe = if let Some(keys_jwk) = auth_params.keys_jwk {
            let mut scoped_keys = HashMap::new();
            allowed_scopes
                .iter()
                .try_for_each(|(scope, _)| -> Result<()> {
                    scoped_keys.insert(
                        scope,
                        self.state
                            .scoped_keys
                            .get(scope)
                            .ok_or_else(|| ErrorKind::NoScopedKey(scope.clone()))?,
                    );
                    Ok(())
                })?;
            let scoped_keys = serde_json::to_string(&scoped_keys)?;
            let keys_jwk = base64::decode_config(keys_jwk, base64::URL_SAFE_NO_PAD)?;
            let jwk = serde_json::from_slice(&keys_jwk)?;
            Some(jwcrypto::encrypt_to_jwe(
                scoped_keys.as_bytes(),
                EncryptionParameters::ECDH_ES {
                    enc: EncryptionAlgorithm::A256GCM,
                    peer_jwk: &jwk,
                },
            )?)
        } else {
            None
        };
        let auth_request_params = AuthorizationRequestParameters {
            client_id: auth_params.client_id,
            scope: auth_params.scope.join(" "),
            state: auth_params.state,
            access_type: auth_params.access_type,
            code_challenge: auth_params
                .pkce_params
                .as_ref()
                .map(|param| param.code_challenge.clone()),
            code_challenge_method: auth_params
                .pkce_params
                .map(|param| param.code_challenge_method),
            keys_jwe,
        };

        let resp = self.client.authorization_code_using_session_token(
            &self.state.config,
            &session_token,
            auth_request_params,
        )?;

        Ok(resp.code)
    }

    fn oauth_flow(&mut self, mut url: Url, scopes: &[&str]) -> Result<String> {
        self.clear_access_token_cache();
        let state = util::random_base64_url_string(16)?;
        let code_verifier = util::random_base64_url_string(43)?;
        let code_challenge = digest::digest(&digest::SHA256, &code_verifier.as_bytes())?;
        let code_challenge = base64::encode_config(&code_challenge, base64::URL_SAFE_NO_PAD);
        let scoped_keys_flow = ScopedKeysFlow::with_random_key()?;
        let jwk = scoped_keys_flow.get_public_key_jwk()?;
        let jwk_json = serde_json::to_string(&jwk)?;
        let keys_jwk = base64::encode_config(&jwk_json, base64::URL_SAFE_NO_PAD);
        url.query_pairs_mut()
            .append_pair("client_id", &self.state.config.client_id)
            .append_pair("scope", &scopes.join(" "))
            .append_pair("state", &state)
            .append_pair("code_challenge_method", "S256")
            .append_pair("code_challenge", &code_challenge)
            .append_pair("access_type", "offline")
            .append_pair("keys_jwk", &keys_jwk);

        if self.state.config.redirect_uri == OAUTH_WEBCHANNEL_REDIRECT {
            url.query_pairs_mut()
                .append_pair("context", "oauth_webchannel_v1");
        } else {
            url.query_pairs_mut()
                .append_pair("redirect_uri", &self.state.config.redirect_uri);
        }

        self.flow_store.insert(
            state, // Since state is supposed to be unique, we use it to key our flows.
            OAuthFlow {
                scoped_keys_flow: Some(scoped_keys_flow),
                code_verifier,
            },
        );
        Ok(url.to_string())
    }

    /// Complete an OAuth flow initiated in `begin_oauth_flow` or `begin_pairing_flow`.
    /// The `code` and `state` parameters can be obtained by parsing out the
    /// redirect URL after a successful login.
    ///
    /// **💾 This method alters the persisted account state.**
    pub fn complete_oauth_flow(&mut self, code: &str, state: &str) -> Result<()> {
        self.clear_access_token_cache();
        let oauth_flow = match self.flow_store.remove(state) {
            Some(oauth_flow) => oauth_flow,
            None => return Err(ErrorKind::UnknownOAuthState.into()),
        };
        let resp = self.client.refresh_token_with_code(
            &self.state.config,
            &code,
            &oauth_flow.code_verifier,
        )?;
        self.handle_oauth_response(resp, oauth_flow.scoped_keys_flow)
    }

    pub(crate) fn handle_oauth_response(
        &mut self,
        resp: OAuthTokenResponse,
        scoped_keys_flow: Option<ScopedKeysFlow>,
    ) -> Result<()> {
        if let Some(ref jwe) = resp.keys_jwe {
            let scoped_keys_flow = scoped_keys_flow.ok_or_else(|| {
                ErrorKind::UnrecoverableServerError("Got a JWE but have no JWK to decrypt it.")
            })?;
            let decrypted_keys = scoped_keys_flow.decrypt_keys_jwe(jwe)?;
            let scoped_keys: serde_json::Map<String, serde_json::Value> =
                serde_json::from_str(&decrypted_keys)?;
            for (scope, key) in scoped_keys {
                let scoped_key: ScopedKey = serde_json::from_value(key)?;
                self.state.scoped_keys.insert(scope, scoped_key);
            }
        }

        // If the client requested a 'tokens/session' OAuth scope then as part of the code
        // exchange this will get a session_token in the response.
        if resp.session_token.is_some() {
            self.state.session_token = resp.session_token;
        }

        // We are only interested in the refresh token at this time because we
        // don't want to return an over-scoped access token.
        // Let's be good citizens and destroy this access token.
        if let Err(err) = self
            .client
            .destroy_access_token(&self.state.config, &resp.access_token)
        {
            log::warn!("Access token destruction failure: {:?}", err);
        }
        let old_refresh_token = self.state.refresh_token.clone();
        let new_refresh_token = resp
            .refresh_token
            .ok_or_else(|| ErrorKind::UnrecoverableServerError("No refresh token in response"))?;
        // Destroying a refresh token also destroys its associated device,
        // grab the device information for replication later.
        let old_device_info = match old_refresh_token {
            Some(_) => match self.get_current_device() {
                Ok(maybe_device) => maybe_device,
                Err(err) => {
                    log::warn!("Error while getting previous device information: {:?}", err);
                    None
                }
            },
            None => None,
        };
        self.state.refresh_token = Some(RefreshToken {
            token: new_refresh_token,
            scopes: HashSet::from_iter(resp.scope.split(' ').map(ToString::to_string)),
        });
        // In order to keep 1 and only 1 refresh token alive per client instance,
        // we also destroy the existing refresh token.
        if let Some(ref refresh_token) = old_refresh_token {
            if let Err(err) = self
                .client
                .destroy_refresh_token(&self.state.config, &refresh_token.token)
            {
                log::warn!("Refresh token destruction failure: {:?}", err);
            }
        }
        if let Some(ref device_info) = old_device_info {
            if let Err(err) = self.replace_device(
                &device_info.display_name,
                &device_info.device_type,
                &device_info.push_subscription,
                &device_info.available_commands,
            ) {
                log::warn!("Device information restoration failed: {:?}", err);
            }
        }
        // When our keys change, we might need to re-register device capabilities with the server.
        // Ensure that this happens on the next call to ensure_capabilities.
        self.state.device_capabilities.clear();
        Ok(())
    }

    /// Typically called during a password change flow.
    /// Invalidates all tokens and fetches a new refresh token.
    /// Because the old refresh token is not valid anymore, we can't do like `handle_oauth_response`
    /// and re-create the device, so it is the responsibility of the caller to do so after we're
    /// done.
    ///
    /// **💾 This method alters the persisted account state.**
    pub fn handle_session_token_change(&mut self, session_token: &str) -> Result<()> {
        let old_refresh_token = self
            .state
            .refresh_token
            .as_ref()
            .ok_or_else(|| ErrorKind::NoRefreshToken)?;
        let scopes: Vec<&str> = old_refresh_token.scopes.iter().map(AsRef::as_ref).collect();
        let resp = self.client.refresh_token_with_session_token(
            &self.state.config,
            &session_token,
            &scopes,
        )?;
        let new_refresh_token = resp
            .refresh_token
            .ok_or_else(|| ErrorKind::UnrecoverableServerError("No refresh token in response"))?;
        self.state.refresh_token = Some(RefreshToken {
            token: new_refresh_token,
            scopes: HashSet::from_iter(resp.scope.split(' ').map(ToString::to_string)),
        });
        self.state.session_token = Some(session_token.to_owned());
        self.clear_access_token_cache();
        self.clear_devices_and_attached_clients_cache();
        // When our keys change, we might need to re-register device capabilities with the server.
        // Ensure that this happens on the next call to ensure_capabilities.
        self.state.device_capabilities.clear();
        Ok(())
    }

    /// **💾 This method may alter the persisted account state.**
    pub fn clear_access_token_cache(&mut self) {
        self.state.access_token_cache.clear();
    }

    #[cfg(feature = "integration_test")]
    pub fn new_logged_in(
        config: crate::Config,
        session_token: &str,
        scoped_keys: HashMap<String, ScopedKey>,
    ) -> Self {
        let mut fxa = FirefoxAccount::with_config(config);
        fxa.state.session_token = Some(session_token.to_owned());
        scoped_keys.iter().for_each(|(key, val)| {
            fxa.state.scoped_keys.insert(key.to_string(), val.clone());
        });
        fxa
    }
}

const AUTH_CIRCUIT_BREAKER_CAPACITY: u8 = 5;
const AUTH_CIRCUIT_BREAKER_RENEWAL_RATE: f32 = 3.0 / 60.0 / 1000.0; // 3 tokens every minute.

// The auth circuit breaker rate-limits access to the `oauth_introspect_refresh_token`
// using a fairly naively implemented token bucket algorithm.
#[derive(Clone, Copy)]
pub(crate) struct AuthCircuitBreaker {
    tokens: u8,
    last_refill: u64, // in ms.
}

impl Default for AuthCircuitBreaker {
    fn default() -> Self {
        AuthCircuitBreaker {
            tokens: AUTH_CIRCUIT_BREAKER_CAPACITY,
            last_refill: Self::now(),
        }
    }
}

impl AuthCircuitBreaker {
    pub(crate) fn check(&mut self) -> Result<()> {
        self.refill();
        if self.tokens == 0 {
            return Err(ErrorKind::AuthCircuitBreakerError.into());
        }
        self.tokens -= 1;
        Ok(())
    }

    fn refill(&mut self) {
        let now = Self::now();
        let new_tokens =
            ((now - self.last_refill) as f64 * AUTH_CIRCUIT_BREAKER_RENEWAL_RATE as f64) as u8; // `as` is a truncating/saturing cast.
        if new_tokens > 0 {
            self.last_refill = now;
            self.tokens = std::cmp::min(
                AUTH_CIRCUIT_BREAKER_CAPACITY,
                self.tokens.saturating_add(new_tokens),
            );
        }
    }

    #[cfg(not(test))]
    #[inline]
    fn now() -> u64 {
        util::now()
    }

    #[cfg(test)]
    fn now() -> u64 {
        1600000000000
    }
}

#[derive(Clone)]
pub struct AuthorizationPKCEParams {
    pub code_challenge: String,
    pub code_challenge_method: String,
}

#[derive(Clone)]
pub struct AuthorizationParameters {
    pub client_id: String,
    pub scope: Vec<String>,
    pub state: String,
    pub access_type: String,
    pub pkce_params: Option<AuthorizationPKCEParams>,
    pub keys_jwk: Option<String>,
}

impl TryFrom<Url> for AuthorizationParameters {
    type Error = Error;

    fn try_from(url: Url) -> Result<Self> {
        let query_map: HashMap<String, String> = url.query_pairs().into_owned().collect();
        let scope = query_map
            .get("scope")
            .cloned()
            .ok_or_else(|| ErrorKind::MissingUrlParameter("scope"))?;
        let client_id = query_map
            .get("client_id")
            .cloned()
            .ok_or_else(|| ErrorKind::MissingUrlParameter("client_id"))?;
        let state = query_map
            .get("state")
            .cloned()
            .ok_or_else(|| ErrorKind::MissingUrlParameter("state"))?;
        let access_type = query_map
            .get("access_type")
            .cloned()
            .ok_or_else(|| ErrorKind::MissingUrlParameter("access_type"))?;
        let code_challenge = query_map.get("code_challenge").cloned();
        let code_challenge_method = query_map.get("code_challenge_method").cloned();
        let pkce_params = match (code_challenge, code_challenge_method) {
            (Some(code_challenge), Some(code_challenge_method)) => Some(AuthorizationPKCEParams {
                code_challenge,
                code_challenge_method,
            }),
            _ => None,
        };
        let keys_jwk = query_map.get("keys_jwk").cloned();
        Ok(Self {
            client_id,
            scope: scope.split_whitespace().map(|s| s.to_string()).collect(),
            state,
            access_type,
            pkce_params,
            keys_jwk,
        })
    }
}
pub struct MetricsParams {
    pub parameters: std::collections::HashMap<String, String>,
}

impl MetricsParams {
    fn append_params_to_url(&self, url: &mut Url) {
        self.parameters
            .iter()
            .for_each(|(parameter_name, parameter_value)| {
                url.query_pairs_mut()
                    .append_pair(parameter_name, parameter_value);
            });
    }
}

#[derive(Clone, Serialize, Deserialize)]
pub struct RefreshToken {
    pub token: String,
    pub scopes: HashSet<String>,
}

impl std::fmt::Debug for RefreshToken {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("RefreshToken")
            .field("scopes", &self.scopes)
            .finish()
    }
}

pub struct OAuthFlow {
    pub scoped_keys_flow: Option<ScopedKeysFlow>,
    pub code_verifier: String,
}

#[derive(Clone, Serialize, Deserialize)]
pub struct AccessTokenInfo {
    pub scope: String,
    pub token: String,
    pub key: Option<ScopedKey>,
    pub expires_at: u64, // seconds since epoch
}

impl std::fmt::Debug for AccessTokenInfo {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("AccessTokenInfo")
            .field("scope", &self.scope)
            .field("key", &self.key)
            .field("expires_at", &self.expires_at)
            .finish()
    }
}

#[derive(Clone, Serialize, Deserialize, Debug)]
pub struct IntrospectInfo {
    pub active: bool,
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{http_client::*, Config};
    use std::borrow::Cow;
    use std::collections::HashMap;
    use std::sync::Arc;

    impl FirefoxAccount {
        pub fn add_cached_token(&mut self, scope: &str, token_info: AccessTokenInfo) {
            self.state
                .access_token_cache
                .insert(scope.to_string(), token_info);
        }

        pub fn set_session_token(&mut self, session_token: &str) {
            self.state.session_token = Some(session_token.to_owned());
        }
    }

    #[test]
    fn test_oauth_flow_url() {
        // FIXME: this test shouldn't make network requests.
        viaduct_reqwest::use_reqwest_backend();
        let config = Config::new(
            "https://accounts.firefox.com",
            "12345678",
            "https://foo.bar",
        );
        let mut params = HashMap::new();
        params.insert("flow_id".to_string(), "87654321".to_string());
        let metrics_params = MetricsParams { parameters: params };
        let mut fxa = FirefoxAccount::with_config(config);
        let url = fxa
            .begin_oauth_flow(&["profile"], "test_oauth_flow_url", Some(metrics_params))
            .unwrap();
        let flow_url = Url::parse(&url).unwrap();

        assert_eq!(flow_url.host_str(), Some("accounts.firefox.com"));
        assert_eq!(flow_url.path(), "/authorization");

        let mut pairs = flow_url.query_pairs();
        assert_eq!(pairs.count(), 12);
        assert_eq!(
            pairs.next(),
            Some((Cow::Borrowed("action"), Cow::Borrowed("email")))
        );
        assert_eq!(
            pairs.next(),
            Some((Cow::Borrowed("response_type"), Cow::Borrowed("code")))
        );
        assert_eq!(
            pairs.next(),
            Some((
                Cow::Borrowed("entrypoint"),
                Cow::Borrowed("test_oauth_flow_url")
            ))
        );
        assert_eq!(
            pairs.next(),
            Some((Cow::Borrowed("flow_id"), Cow::Borrowed("87654321")))
        );
        assert_eq!(
            pairs.next(),
            Some((Cow::Borrowed("client_id"), Cow::Borrowed("12345678")))
        );

        assert_eq!(
            pairs.next(),
            Some((Cow::Borrowed("scope"), Cow::Borrowed("profile")))
        );
        let state_param = pairs.next().unwrap();
        assert_eq!(state_param.0, Cow::Borrowed("state"));
        assert_eq!(state_param.1.len(), 22);
        assert_eq!(
            pairs.next(),
            Some((
                Cow::Borrowed("code_challenge_method"),
                Cow::Borrowed("S256")
            ))
        );
        let code_challenge_param = pairs.next().unwrap();
        assert_eq!(code_challenge_param.0, Cow::Borrowed("code_challenge"));
        assert_eq!(code_challenge_param.1.len(), 43);
        assert_eq!(
            pairs.next(),
            Some((Cow::Borrowed("access_type"), Cow::Borrowed("offline")))
        );
        let keys_jwk = pairs.next().unwrap();
        assert_eq!(keys_jwk.0, Cow::Borrowed("keys_jwk"));
        assert_eq!(keys_jwk.1.len(), 168);

        assert_eq!(
            pairs.next(),
            Some((
                Cow::Borrowed("redirect_uri"),
                Cow::Borrowed("https://foo.bar")
            ))
        );
    }

    #[test]
    fn test_force_auth_url() {
        let config = Config::stable_dev("12345678", "https://foo.bar");
        let mut fxa = FirefoxAccount::with_config(config);
        let email = "test@example.com";
        fxa.add_cached_profile("123", email);
        let url = fxa
            .begin_oauth_flow(&["profile"], "test_force_auth_url", None)
            .unwrap();
        let url = Url::parse(&url).unwrap();
        assert_eq!(url.path(), "/oauth/force_auth");
        let mut pairs = url.query_pairs();
        assert_eq!(
            pairs.find(|e| e.0 == "email"),
            Some((Cow::Borrowed("email"), Cow::Borrowed(email),))
        );
    }

    #[test]
    fn test_webchannel_context_url() {
        // FIXME: this test shouldn't make network requests.
        viaduct_reqwest::use_reqwest_backend();
        const SCOPES: &[&str] = &["https://identity.mozilla.com/apps/oldsync"];
        let config = Config::new(
            "https://accounts.firefox.com",
            "12345678",
            "urn:ietf:wg:oauth:2.0:oob:oauth-redirect-webchannel",
        );
        let mut fxa = FirefoxAccount::with_config(config);
        let url = fxa
            .begin_oauth_flow(&SCOPES, "test_webchannel_context_url", None)
            .unwrap();
        let url = Url::parse(&url).unwrap();
        let query_params: HashMap<_, _> = url.query_pairs().into_owned().collect();
        let context = &query_params["context"];
        assert_eq!(context, "oauth_webchannel_v1");
        assert_eq!(query_params.get("redirect_uri"), None);
    }

    #[test]
    fn test_webchannel_pairing_context_url() {
        const SCOPES: &[&str] = &["https://identity.mozilla.com/apps/oldsync"];
        const PAIRING_URL: &str = "https://accounts.firefox.com/pair#channel_id=658db7fe98b249a5897b884f98fb31b7&channel_key=1hIDzTj5oY2HDeSg_jA2DhcOcAn5Uqq0cAYlZRNUIo4";

        let config = Config::new(
            "https://accounts.firefox.com",
            "12345678",
            "urn:ietf:wg:oauth:2.0:oob:oauth-redirect-webchannel",
        );
        let mut fxa = FirefoxAccount::with_config(config);
        let url = fxa
            .begin_pairing_flow(
                &PAIRING_URL,
                &SCOPES,
                "test_webchannel_pairing_context_url",
                None,
            )
            .unwrap();
        let url = Url::parse(&url).unwrap();
        let query_params: HashMap<_, _> = url.query_pairs().into_owned().collect();
        let context = &query_params["context"];
        assert_eq!(context, "oauth_webchannel_v1");
        assert_eq!(query_params.get("redirect_uri"), None);
    }

    #[test]
    fn test_pairing_flow_url() {
        const SCOPES: &[&str] = &["https://identity.mozilla.com/apps/oldsync"];
        const PAIRING_URL: &str = "https://accounts.firefox.com/pair#channel_id=658db7fe98b249a5897b884f98fb31b7&channel_key=1hIDzTj5oY2HDeSg_jA2DhcOcAn5Uqq0cAYlZRNUIo4";
        const EXPECTED_URL: &str = "https://accounts.firefox.com/pair/supp?client_id=12345678&redirect_uri=https%3A%2F%2Ffoo.bar&scope=https%3A%2F%2Fidentity.mozilla.com%2Fapps%2Foldsync&state=SmbAA_9EA5v1R2bgIPeWWw&code_challenge_method=S256&code_challenge=ZgHLPPJ8XYbXpo7VIb7wFw0yXlTa6MUOVfGiADt0JSM&access_type=offline&keys_jwk=eyJjcnYiOiJQLTI1NiIsImt0eSI6IkVDIiwieCI6Ing5LUltQjJveDM0LTV6c1VmbW5sNEp0Ti14elV2eFZlZXJHTFRXRV9BT0kiLCJ5IjoiNXBKbTB3WGQ4YXdHcm0zREl4T1pWMl9qdl9tZEx1TWlMb1RkZ1RucWJDZyJ9#channel_id=658db7fe98b249a5897b884f98fb31b7&channel_key=1hIDzTj5oY2HDeSg_jA2DhcOcAn5Uqq0cAYlZRNUIo4";

        let config = Config::new(
            "https://accounts.firefox.com",
            "12345678",
            "https://foo.bar",
        );
        let mut params = HashMap::new();
        params.insert("flow_id".to_string(), "87654321".to_string());
        let metrics_params = MetricsParams { parameters: params };

        let mut fxa = FirefoxAccount::with_config(config);
        let url = fxa
            .begin_pairing_flow(
                &PAIRING_URL,
                &SCOPES,
                "test_pairing_flow_url",
                Some(metrics_params),
            )
            .unwrap();
        let flow_url = Url::parse(&url).unwrap();
        let expected_parsed_url = Url::parse(EXPECTED_URL).unwrap();

        assert_eq!(flow_url.host_str(), Some("accounts.firefox.com"));
        assert_eq!(flow_url.path(), "/pair/supp");
        assert_eq!(flow_url.fragment(), expected_parsed_url.fragment());

        let mut pairs = flow_url.query_pairs();
        assert_eq!(pairs.count(), 10);
        assert_eq!(
            pairs.next(),
            Some((
                Cow::Borrowed("entrypoint"),
                Cow::Borrowed("test_pairing_flow_url")
            ))
        );
        assert_eq!(
            pairs.next(),
            Some((Cow::Borrowed("flow_id"), Cow::Borrowed("87654321")))
        );
        assert_eq!(
            pairs.next(),
            Some((Cow::Borrowed("client_id"), Cow::Borrowed("12345678")))
        );
        assert_eq!(
            pairs.next(),
            Some((
                Cow::Borrowed("scope"),
                Cow::Borrowed("https://identity.mozilla.com/apps/oldsync")
            ))
        );

        let state_param = pairs.next().unwrap();
        assert_eq!(state_param.0, Cow::Borrowed("state"));
        assert_eq!(state_param.1.len(), 22);
        assert_eq!(
            pairs.next(),
            Some((
                Cow::Borrowed("code_challenge_method"),
                Cow::Borrowed("S256")
            ))
        );
        let code_challenge_param = pairs.next().unwrap();
        assert_eq!(code_challenge_param.0, Cow::Borrowed("code_challenge"));
        assert_eq!(code_challenge_param.1.len(), 43);
        assert_eq!(
            pairs.next(),
            Some((Cow::Borrowed("access_type"), Cow::Borrowed("offline")))
        );
        let keys_jwk = pairs.next().unwrap();
        assert_eq!(keys_jwk.0, Cow::Borrowed("keys_jwk"));
        assert_eq!(keys_jwk.1.len(), 168);

        assert_eq!(
            pairs.next(),
            Some((
                Cow::Borrowed("redirect_uri"),
                Cow::Borrowed("https://foo.bar")
            ))
        );
    }

    #[test]
    fn test_pairing_flow_origin_mismatch() {
        static PAIRING_URL: &str = "https://bad.origin.com/pair#channel_id=foo&channel_key=bar";
        let config = Config::stable_dev("12345678", "https://foo.bar");
        let mut fxa = FirefoxAccount::with_config(config);
        let url = fxa.begin_pairing_flow(
            &PAIRING_URL,
            &["https://identity.mozilla.com/apps/oldsync"],
            "test_pairiong_flow_origin_mismatch",
            None,
        );

        assert!(url.is_err());

        match url {
            Ok(_) => {
                panic!("should have error");
            }
            Err(err) => match err.kind() {
                ErrorKind::OriginMismatch { .. } => {}
                _ => panic!("error not OriginMismatch"),
            },
        }
    }

    #[test]
    fn test_check_authorization_status() {
        let config = Config::stable_dev("12345678", "https://foo.bar");
        let mut fxa = FirefoxAccount::with_config(config);

        let refresh_token_scopes = std::collections::HashSet::new();
        fxa.state.refresh_token = Some(RefreshToken {
            token: "refresh_token".to_owned(),
            scopes: refresh_token_scopes,
        });

        let mut client = FxAClientMock::new();
        client
            .expect_oauth_introspect_refresh_token(mockiato::Argument::any, |token| {
                token.partial_eq("refresh_token")
            })
            .times(1)
            .returns_once(Ok(IntrospectResponse { active: true }));
        fxa.set_client(Arc::new(client));

        let auth_status = fxa.check_authorization_status().unwrap();
        assert_eq!(auth_status.active, true);
    }

    #[test]
    fn test_check_authorization_status_circuit_breaker() {
        let config = Config::stable_dev("12345678", "https://foo.bar");
        let mut fxa = FirefoxAccount::with_config(config);

        let refresh_token_scopes = std::collections::HashSet::new();
        fxa.state.refresh_token = Some(RefreshToken {
            token: "refresh_token".to_owned(),
            scopes: refresh_token_scopes,
        });

        let mut client = FxAClientMock::new();
        // This copy-pasta (equivalent to `.returns(..).times(5)`) is there
        // because `Error` is not cloneable :/
        client
            .expect_oauth_introspect_refresh_token(mockiato::Argument::any, |token| {
                token.partial_eq("refresh_token")
            })
            .returns_once(Ok(IntrospectResponse { active: true }));
        client
            .expect_oauth_introspect_refresh_token(mockiato::Argument::any, |token| {
                token.partial_eq("refresh_token")
            })
            .returns_once(Ok(IntrospectResponse { active: true }));
        client
            .expect_oauth_introspect_refresh_token(mockiato::Argument::any, |token| {
                token.partial_eq("refresh_token")
            })
            .returns_once(Ok(IntrospectResponse { active: true }));
        client
            .expect_oauth_introspect_refresh_token(mockiato::Argument::any, |token| {
                token.partial_eq("refresh_token")
            })
            .returns_once(Ok(IntrospectResponse { active: true }));
        client
            .expect_oauth_introspect_refresh_token(mockiato::Argument::any, |token| {
                token.partial_eq("refresh_token")
            })
            .returns_once(Ok(IntrospectResponse { active: true }));
        client.expect_oauth_introspect_refresh_token_calls_in_order();
        fxa.set_client(Arc::new(client));

        for _ in 0..5 {
            assert!(fxa.check_authorization_status().is_ok());
        }
        match fxa.check_authorization_status() {
            Ok(_) => unreachable!("should not happen"),
            Err(err) => assert!(matches!(err.kind(), ErrorKind::AuthCircuitBreakerError)),
        }
    }

    #[test]
    fn test_auth_circuit_breaker_unit_recovery() {
        let mut breaker = AuthCircuitBreaker::default();
        // AuthCircuitBreaker::now is fixed for tests, let's assert that for sanity.
        assert_eq!(AuthCircuitBreaker::now(), 1600000000000);
        for _ in 0..AUTH_CIRCUIT_BREAKER_CAPACITY {
            assert!(breaker.check().is_ok());
        }
        assert!(breaker.check().is_err());
        // Jump back in time (1 min).
        breaker.last_refill -= 60 * 1000;
        let expected_tokens_before_check: u8 =
            (AUTH_CIRCUIT_BREAKER_RENEWAL_RATE * 60.0 * 1000.0) as u8;
        assert!(breaker.check().is_ok());
        assert_eq!(breaker.tokens, expected_tokens_before_check - 1);
    }

    use crate::scopes;

    #[test]
    fn test_auth_code_pair_valid_not_allowed_scope() {
        let config = Config::stable_dev("12345678", "https://foo.bar");
        let mut fxa = FirefoxAccount::with_config(config);
        fxa.set_session_token("session");
        let mut client = FxAClientMock::new();
        let not_allowed_scope = "https://identity.mozilla.com/apps/lockbox";
        let expected_scopes = scopes::OLD_SYNC
            .chars()
            .chain(std::iter::once(' '))
            .chain(not_allowed_scope.chars())
            .collect::<String>();
        client
            .expect_scoped_key_data(
                mockiato::Argument::any,
                |arg| arg.partial_eq("session"),
                |arg| arg.partial_eq("12345678"),
                |arg| arg.partial_eq(expected_scopes),
            )
            .returns_once(Err(ErrorKind::RemoteError {
                code: 400,
                errno: 163,
                error: "Invalid Scopes".to_string(),
                message: "Not allowed to request scopes".to_string(),
                info: "fyi, there was a server error".to_string(),
            }
            .into()));
        fxa.set_client(Arc::new(client));
        let auth_params = AuthorizationParameters {
            client_id: "12345678".to_string(),
            scope: vec![scopes::OLD_SYNC.to_string(), not_allowed_scope.to_string()],
            state: "somestate".to_string(),
            access_type: "offline".to_string(),
            pkce_params: None,
            keys_jwk: None,
        };
        let res = fxa.authorize_code_using_session_token(auth_params);
        assert!(res.is_err());
        let err = res.unwrap_err();
        if let ErrorKind::RemoteError {
            code,
            errno,
            error: _,
            message: _,
            info: _,
        } = err.kind()
        {
            assert_eq!(*code, 400);
            assert_eq!(*errno, 163); // Requested scopes not allowed
        } else {
            panic!("Should return an error from the server specifying that the requested scopes are not allowed");
        }
    }

    #[test]
    fn test_auth_code_pair_invalid_scope_not_allowed() {
        let config = Config::stable_dev("12345678", "https://foo.bar");
        let mut fxa = FirefoxAccount::with_config(config);
        fxa.set_session_token("session");
        let mut client = FxAClientMock::new();
        let invalid_scope = "IamAnInvalidScope";
        let expected_scopes = scopes::OLD_SYNC
            .chars()
            .chain(std::iter::once(' '))
            .chain(invalid_scope.chars())
            .collect::<String>();
        let mut server_ret = HashMap::new();
        server_ret.insert(
            scopes::OLD_SYNC.to_string(),
            ScopedKeyDataResponse {
                key_rotation_secret: "IamASecret".to_string(),
                key_rotation_timestamp: 100,
                identifier: "".to_string(),
            },
        );
        client
            .expect_scoped_key_data(
                mockiato::Argument::any,
                |arg| arg.partial_eq("session"),
                |arg| arg.partial_eq("12345678"),
                |arg| arg.partial_eq(expected_scopes),
            )
            .returns_once(Ok(server_ret));
        fxa.set_client(Arc::new(client));

        let auth_params = AuthorizationParameters {
            client_id: "12345678".to_string(),
            scope: vec![scopes::OLD_SYNC.to_string(), invalid_scope.to_string()],
            state: "somestate".to_string(),
            access_type: "offline".to_string(),
            pkce_params: None,
            keys_jwk: None,
        };
        let res = fxa.authorize_code_using_session_token(auth_params);
        assert!(res.is_err());
        let err = res.unwrap_err();
        if let ErrorKind::ScopeNotAllowed(client_id, scope) = err.kind() {
            assert_eq!(client_id.clone(), "12345678");
            assert_eq!(scope.clone(), "IamAnInvalidScope");
        } else {
            panic!("Should return an error that specifies the scope that is not allowed");
        }
    }

    #[test]
    fn test_auth_code_pair_scope_not_in_state() {
        let config = Config::stable_dev("12345678", "https://foo.bar");
        let mut fxa = FirefoxAccount::with_config(config);
        fxa.set_session_token("session");
        let mut client = FxAClientMock::new();
        let mut server_ret = HashMap::new();
        server_ret.insert(
            scopes::OLD_SYNC.to_string(),
            ScopedKeyDataResponse {
                key_rotation_secret: "IamASecret".to_string(),
                key_rotation_timestamp: 100,
                identifier: "".to_string(),
            },
        );
        client
            .expect_scoped_key_data(
                mockiato::Argument::any,
                |arg| arg.partial_eq("session"),
                |arg| arg.partial_eq("12345678"),
                |arg| arg.partial_eq(scopes::OLD_SYNC),
            )
            .returns_once(Ok(server_ret));
        fxa.set_client(Arc::new(client));
        let auth_params = AuthorizationParameters {
            client_id: "12345678".to_string(),
            scope: vec![scopes::OLD_SYNC.to_string()],
            state: "somestate".to_string(),
            access_type: "offline".to_string(),
            pkce_params: None,
            keys_jwk: Some("IAmAVerySecretKeysJWkInBase64".to_string()),
        };
        let res = fxa.authorize_code_using_session_token(auth_params);
        assert!(res.is_err());
        let err = res.unwrap_err();
        if let ErrorKind::NoScopedKey(scope) = err.kind() {
            assert_eq!(scope.clone(), scopes::OLD_SYNC.to_string());
        } else {
            panic!("Should return an error that specifies the scope that is not in the state");
        }
    }
}
