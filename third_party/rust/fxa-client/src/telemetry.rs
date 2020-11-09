/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::{error::*, scopes, FirefoxAccount};
use jwcrypto::{EncryptionAlgorithm, EncryptionParameters, Jwk};
use rand_rccrypto::rand::seq::SliceRandom;
use rc_crypto::rand;
use serde_derive::*;
use sync_guid::Guid;

impl FirefoxAccount {
    /// Get the ecosystem anon id, generating it if necessary.
    ///
    /// **💾 This method alters the persisted account state.**
    pub fn get_ecosystem_anon_id(&mut self) -> Result<String> {
        self.get_ecosystem_anon_id_helper(true)
    }

    fn get_ecosystem_anon_id_helper(&mut self, generate_placeholder: bool) -> Result<String> {
        let profile = self.get_profile(false)?;
        // Default case: the ecosystem anon ID was generated during login.
        if let Some(ecosystem_anon_id) = profile.ecosystem_anon_id {
            return Ok(ecosystem_anon_id);
        }
        if !generate_placeholder {
            return Err(ErrorKind::IllegalState("ecosystem_anon_id should be present").into());
        }
        // For older clients, we generate an ecosystem_user_id,
        // persist it and then return ecosystem_anon_id.
        let mut ecosystem_user_id = vec![0u8; 32];
        rand::fill(&mut ecosystem_user_id)?;
        // Will end up as a len 64 hex string.
        let ecosystem_user_id = hex::encode(ecosystem_user_id);

        let anon_id_key = self.fetch_random_ecosystem_anon_id_key()?;
        let ecosystem_anon_id = jwcrypto::encrypt_to_jwe(
            &ecosystem_user_id.as_bytes(),
            EncryptionParameters::ECDH_ES {
                enc: EncryptionAlgorithm::A256GCM,
                peer_jwk: &anon_id_key,
            },
        )?;

        let token = self.get_access_token(scopes::PROFILE_WRITE, None)?.token;
        if let Err(err) =
            self.client
                .set_ecosystem_anon_id(&self.state.config, &token, &ecosystem_anon_id)
        {
            if let ErrorKind::RemoteError { code: 412, .. } = err.kind() {
                // Another client beat us, fetch the new ecosystem_anon_id.
                return self.get_ecosystem_anon_id_helper(false);
            }
        }

        // Persist the unencrypted ecosystem_user_id for possible future use.
        self.state.ecosystem_user_id = Some(ecosystem_user_id);
        Ok(ecosystem_anon_id)
    }

    fn fetch_random_ecosystem_anon_id_key(&self) -> Result<Jwk> {
        let config = self.client.fxa_client_configuration(&self.state.config)?;
        let keys = config
            .ecosystem_anon_id_keys
            .ok_or_else(|| ErrorKind::NoAnonIdKey)?;
        let mut rng = rand_rccrypto::RcCryptoRng;
        Ok(keys
            .choose(&mut rng)
            .ok_or_else(|| ErrorKind::NoAnonIdKey)?
            .clone())
    }

    /// Gathers and resets telemetry for this account instance.
    /// This should be considered a short-term solution to telemetry gathering
    /// and should called whenever consumers expect there might be telemetry,
    /// and it should submit the telemetry to whatever telemetry system is in
    /// use (probably glean).
    ///
    /// The data is returned as a JSON string, which consumers should parse
    /// forgivingly (eg, be tolerant of things not existing) to try and avoid
    /// too many changes as telemetry comes and goes.
    pub fn gather_telemetry(&mut self) -> Result<String> {
        let telem = self.telemetry.replace(FxaTelemetry::new());
        Ok(serde_json::to_string(&telem)?)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{http_client::*, oauth::AccessTokenInfo, Config};
    use jwcrypto::{ec::ECKeysParameters, JwkKeyParameters};
    use std::sync::Arc;

    fn fxa_setup() -> FirefoxAccount {
        let config = Config::stable_dev("12345678", "https://foo.bar");
        let mut fxa = FirefoxAccount::with_config(config);
        fxa.add_cached_token(
            "profile",
            AccessTokenInfo {
                scope: "profile".to_string(),
                token: "profiletok".to_string(),
                key: None,
                expires_at: u64::max_value(),
            },
        );
        fxa.add_cached_token(
            "profile:write",
            AccessTokenInfo {
                scope: "profile".to_string(),
                token: "profilewritetok".to_string(),
                key: None,
                expires_at: u64::max_value(),
            },
        );
        fxa
    }

    #[test]
    fn get_ecosystem_anon_id_in_profile() {
        let mut fxa = fxa_setup();

        let ecosystem_anon_id = "bobo".to_owned();

        let mut client = FxAClientMock::new();
        client
            .expect_profile(
                mockiato::Argument::any,
                |token| token.partial_eq("profiletok"),
                mockiato::Argument::any,
            )
            .times(1)
            .returns_once(Ok(Some(ResponseAndETag {
                response: ProfileResponse {
                    uid: "12345ab".to_string(),
                    email: "foo@bar.com".to_string(),
                    display_name: None,
                    avatar: "https://foo.avatar".to_string(),
                    avatar_default: true,
                    ecosystem_anon_id: Some(ecosystem_anon_id.to_owned()),
                },
                etag: None,
            })));
        fxa.set_client(Arc::new(client));

        assert_eq!(fxa.get_ecosystem_anon_id().unwrap(), ecosystem_anon_id);
    }

    #[test]
    fn get_ecosystem_anon_id_generate_anon_id() {
        let mut fxa = fxa_setup();

        let mut client = FxAClientMock::new();
        client
            .expect_profile(
                mockiato::Argument::any,
                |token| token.partial_eq("profiletok"),
                mockiato::Argument::any,
            )
            .times(1)
            .returns_once(Ok(Some(ResponseAndETag {
                response: ProfileResponse {
                    uid: "12345ab".to_string(),
                    email: "foo@bar.com".to_string(),
                    display_name: None,
                    avatar: "https://foo.avatar".to_string(),
                    avatar_default: true,
                    ecosystem_anon_id: None,
                },
                etag: None,
            })));
        client
            .expect_fxa_client_configuration(mockiato::Argument::any)
            .times(1)
            .returns_once(Ok(ClientConfigurationResponse {
                auth_server_base_url: "https://foo.bar".to_owned(),
                oauth_server_base_url: "https://foo.bar".to_owned(),
                profile_server_base_url: "https://foo.bar".to_owned(),
                sync_tokenserver_base_url: "https://foo.bar".to_owned(),
                ecosystem_anon_id_keys: Some(vec![Jwk {
                    kid: Some("LlU4keOmhTuq9fCNnpIldYGT9vT9dIDwnu_SBtTgeEQ".to_owned()),
                    key_parameters: JwkKeyParameters::EC(ECKeysParameters {
                        crv: "P-256".to_owned(),
                        x: "i3FM3OFSCZEoqu-jtelXwKt6AL4ODQ75NUdHbcLWQSo".to_owned(),
                        y: "nW-S3QiHDo-9hwfBhKnGKarkt_PVqVyIPUytjutTunY".to_owned(),
                    }),
                }]),
            }));
        client
            .expect_set_ecosystem_anon_id(
                mockiato::Argument::any,
                |token| token.partial_eq("profilewritetok"),
                mockiato::Argument::any,
            )
            .times(1)
            .returns_once(Ok(()));
        fxa.set_client(Arc::new(client));

        let ecosystem_anon_id = fxa.get_ecosystem_anon_id().unwrap();
        // Well, it looks like a jwe folks.
        assert!(ecosystem_anon_id.chars().filter(|c| c == &'.').count() == 4);
        assert!(fxa.state.ecosystem_user_id.unwrap().len() == 64);
    }

    #[test]
    fn get_ecosystem_anon_id_generate_anon_id_412() {
        let mut fxa = fxa_setup();

        let ecosystem_anon_id = "bobo".to_owned();

        let mut client = FxAClientMock::new();
        client
            .expect_profile(
                mockiato::Argument::any,
                |token| token.partial_eq("profiletok"),
                mockiato::Argument::any,
            )
            .returns_once(Ok(Some(ResponseAndETag {
                response: ProfileResponse {
                    uid: "12345ab".to_string(),
                    email: "foo@bar.com".to_string(),
                    display_name: None,
                    avatar: "https://foo.avatar".to_string(),
                    avatar_default: true,
                    ecosystem_anon_id: None,
                },
                etag: None,
            })));
        // 2nd profile call after we get the 412.
        client
            .expect_profile(
                mockiato::Argument::any,
                |token| token.partial_eq("profiletok"),
                mockiato::Argument::any,
            )
            .returns_once(Ok(Some(ResponseAndETag {
                response: ProfileResponse {
                    uid: "12345ab".to_string(),
                    email: "foo@bar.com".to_string(),
                    display_name: None,
                    avatar: "https://foo.avatar".to_string(),
                    avatar_default: true,
                    ecosystem_anon_id: Some(ecosystem_anon_id.clone()),
                },
                etag: None,
            })));
        client.expect_profile_calls_in_order();
        client
            .expect_fxa_client_configuration(mockiato::Argument::any)
            .times(1)
            .returns_once(Ok(ClientConfigurationResponse {
                auth_server_base_url: "https://foo.bar".to_owned(),
                oauth_server_base_url: "https://foo.bar".to_owned(),
                profile_server_base_url: "https://foo.bar".to_owned(),
                sync_tokenserver_base_url: "https://foo.bar".to_owned(),
                ecosystem_anon_id_keys: Some(vec![Jwk {
                    kid: Some("LlU4keOmhTuq9fCNnpIldYGT9vT9dIDwnu_SBtTgeEQ".to_owned()),
                    key_parameters: JwkKeyParameters::EC(ECKeysParameters {
                        crv: "P-256".to_owned(),
                        x: "i3FM3OFSCZEoqu-jtelXwKt6AL4ODQ75NUdHbcLWQSo".to_owned(),
                        y: "nW-S3QiHDo-9hwfBhKnGKarkt_PVqVyIPUytjutTunY".to_owned(),
                    }),
                }]),
            }));
        client
            .expect_set_ecosystem_anon_id(
                mockiato::Argument::any,
                |token| token.partial_eq("profilewritetok"),
                mockiato::Argument::any,
            )
            .times(1)
            .returns_once(Err(ErrorKind::RemoteError {
                code: 412,
                errno: 500,
                error: "precondition failed".to_string(),
                message: "another user did it".to_string(),
                info: "".to_string(),
            }
            .into()));
        fxa.set_client(Arc::new(client));

        assert_eq!(fxa.get_ecosystem_anon_id().unwrap(), ecosystem_anon_id);
        assert!(fxa.state.ecosystem_user_id.is_none());
    }
}

// A somewhat mixed-bag of all telemetry we want to collect. The idea is that
// the app will "pull" telemetry via a new API whenever it thinks there might
// be something to record.
// It's considered a temporary solution until either we can record it directly
// (eg, via glean) or we come up with something better.
// Note that this means we'll lose telemetry if we crash between gathering it
// here and the app submitting it, but that should be rare (in practice,
// apps will submit it directly after an operation that generated telememtry)

/// The reason a tab/command was received.
#[derive(Debug, Serialize)]
#[serde(rename_all = "kebab-case")]
pub enum ReceivedReason {
    /// A push notification for the command was received.
    Push,
    /// Discovered while handling a push notification for a later message.
    PushMissed,
    /// Explicit polling for missed commands.
    Poll,
}

#[derive(Debug, Serialize)]
pub struct SentCommand {
    pub flow_id: String,
    pub stream_id: String,
}

impl Default for SentCommand {
    fn default() -> Self {
        Self {
            flow_id: Guid::random().to_string(),
            stream_id: Guid::random().to_string(),
        }
    }
}

#[derive(Debug, Serialize)]
pub struct ReceivedCommand {
    pub flow_id: String,
    pub stream_id: String,
    pub reason: ReceivedReason,
}

// We have a naive strategy to avoid unbounded memory growth - the intention
// is that if any platform lets things grow to hit these limits, it's probably
// never going to consume anything - so it doesn't matter what we discard (ie,
// there's no good reason to have a smarter circular buffer etc)
const MAX_TAB_EVENTS: usize = 200;

#[derive(Debug, Default, Serialize)]
pub struct FxaTelemetry {
    commands_sent: Vec<SentCommand>,
    commands_received: Vec<ReceivedCommand>,
}

impl FxaTelemetry {
    pub fn new() -> Self {
        FxaTelemetry {
            ..Default::default()
        }
    }

    pub fn record_tab_sent(&mut self, sent: SentCommand) {
        if self.commands_sent.len() < MAX_TAB_EVENTS {
            self.commands_sent.push(sent);
        }
    }

    pub fn record_tab_received(&mut self, recd: ReceivedCommand) {
        if self.commands_received.len() < MAX_TAB_EVENTS {
            self.commands_received.push(recd);
        }
    }
}
