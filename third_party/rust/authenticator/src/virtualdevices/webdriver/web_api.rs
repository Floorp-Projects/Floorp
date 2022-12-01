/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use serde::{Deserialize, Serialize};
use std::net::SocketAddr;
use std::string;
use std::sync::{Arc, Mutex};
use warp::Filter;

use crate::virtualdevices::webdriver::{testtoken, virtualmanager::VirtualManagerState};

fn default_as_false() -> bool {
    false
}
fn default_as_true() -> bool {
    false
}

#[derive(Serialize, Deserialize, Clone, PartialEq)]
pub struct AuthenticatorConfiguration {
    protocol: string::String,
    transport: string::String,
    #[serde(rename = "hasResidentKey")]
    #[serde(default = "default_as_false")]
    has_resident_key: bool,
    #[serde(rename = "hasUserVerification")]
    #[serde(default = "default_as_false")]
    has_user_verification: bool,
    #[serde(rename = "isUserConsenting")]
    #[serde(default = "default_as_true")]
    is_user_consenting: bool,
    #[serde(rename = "isUserVerified")]
    #[serde(default = "default_as_false")]
    is_user_verified: bool,
}

#[derive(Serialize, Deserialize, Clone, PartialEq)]
pub struct CredentialParameters {
    #[serde(rename = "credentialId")]
    credential_id: String,
    #[serde(rename = "isResidentCredential")]
    is_resident_credential: bool,
    #[serde(rename = "rpId")]
    rp_id: String,
    #[serde(rename = "privateKey")]
    private_key: String,
    #[serde(rename = "userHandle")]
    #[serde(default)]
    user_handle: String,
    #[serde(rename = "signCount")]
    sign_count: u64,
}

#[derive(Serialize, Deserialize, Clone, PartialEq)]
pub struct UserVerificationParameters {
    #[serde(rename = "isUserVerified")]
    is_user_verified: bool,
}

impl CredentialParameters {
    fn new_from_test_token_credential(tc: &testtoken::TestTokenCredential) -> CredentialParameters {
        let credential_id = base64::encode_config(&tc.credential, base64::URL_SAFE);

        let private_key = base64::encode_config(&tc.privkey, base64::URL_SAFE);

        let user_handle = base64::encode_config(&tc.user_handle, base64::URL_SAFE);

        CredentialParameters {
            credential_id,
            is_resident_credential: tc.is_resident_credential,
            rp_id: tc.rp_id.clone(),
            private_key,
            user_handle,
            sign_count: tc.sign_count,
        }
    }
}

fn with_state(
    state: Arc<Mutex<VirtualManagerState>>,
) -> impl Filter<Extract = (Arc<Mutex<VirtualManagerState>>,), Error = std::convert::Infallible> + Clone
{
    warp::any().map(move || state.clone())
}

fn authenticator_add(
    state: Arc<Mutex<VirtualManagerState>>,
) -> impl Filter<Extract = impl warp::Reply, Error = warp::Rejection> + Clone {
    warp::path!("webauthn" / "authenticator")
        .and(warp::post())
        .and(warp::body::json())
        .and(with_state(state))
        .and_then(handlers::authenticator_add)
}

fn authenticator_delete(
    state: Arc<Mutex<VirtualManagerState>>,
) -> impl Filter<Extract = impl warp::Reply, Error = warp::Rejection> + Clone {
    warp::path!("webauthn" / "authenticator" / u64)
        .and(warp::delete())
        .and(with_state(state))
        .and_then(handlers::authenticator_delete)
}

fn authenticator_set_uv(
    state: Arc<Mutex<VirtualManagerState>>,
) -> impl Filter<Extract = impl warp::Reply, Error = warp::Rejection> + Clone {
    warp::path!("webauthn" / "authenticator" / u64 / "uv")
        .and(warp::post())
        .and(warp::body::json())
        .and(with_state(state))
        .and_then(handlers::authenticator_set_uv)
}

// This is not part of the specification, but it's useful for debugging
fn authenticator_get(
    state: Arc<Mutex<VirtualManagerState>>,
) -> impl Filter<Extract = impl warp::Reply, Error = warp::Rejection> + Clone {
    warp::path!("webauthn" / "authenticator" / u64)
        .and(warp::get())
        .and(with_state(state))
        .and_then(handlers::authenticator_get)
}

fn authenticator_credential_add(
    state: Arc<Mutex<VirtualManagerState>>,
) -> impl Filter<Extract = impl warp::Reply, Error = warp::Rejection> + Clone {
    warp::path!("webauthn" / "authenticator" / u64 / "credential")
        .and(warp::post())
        .and(warp::body::json())
        .and(with_state(state))
        .and_then(handlers::authenticator_credential_add)
}

fn authenticator_credential_delete(
    state: Arc<Mutex<VirtualManagerState>>,
) -> impl Filter<Extract = impl warp::Reply, Error = warp::Rejection> + Clone {
    warp::path!("webauthn" / "authenticator" / u64 / "credentials" / String)
        .and(warp::delete())
        .and(with_state(state))
        .and_then(handlers::authenticator_credential_delete)
}

fn authenticator_credentials_get(
    state: Arc<Mutex<VirtualManagerState>>,
) -> impl Filter<Extract = impl warp::Reply, Error = warp::Rejection> + Clone {
    warp::path!("webauthn" / "authenticator" / u64 / "credentials")
        .and(warp::get())
        .and(with_state(state))
        .and_then(handlers::authenticator_credentials_get)
}

fn authenticator_credentials_clear(
    state: Arc<Mutex<VirtualManagerState>>,
) -> impl Filter<Extract = impl warp::Reply, Error = warp::Rejection> + Clone {
    warp::path!("webauthn" / "authenticator" / u64 / "credentials")
        .and(warp::delete())
        .and(with_state(state))
        .and_then(handlers::authenticator_credentials_clear)
}

mod handlers {
    use super::{CredentialParameters, UserVerificationParameters};
    use crate::virtualdevices::webdriver::{
        testtoken, virtualmanager::VirtualManagerState, web_api::AuthenticatorConfiguration,
    };
    use serde::Serialize;
    use std::convert::Infallible;
    use std::ops::DerefMut;
    use std::sync::{Arc, Mutex};
    use std::vec;
    use warp::http::{uri, StatusCode};

    #[derive(Serialize)]
    struct JsonSuccess {}

    impl JsonSuccess {
        pub fn blank() -> JsonSuccess {
            JsonSuccess {}
        }
    }

    #[derive(Serialize)]
    struct JsonError {
        #[serde(skip_serializing_if = "Option::is_none")]
        line: Option<u32>,
        error: String,
        details: String,
    }

    impl JsonError {
        pub fn new(error: &str, line: u32, details: &str) -> JsonError {
            JsonError {
                details: details.to_string(),
                error: error.to_string(),
                line: Some(line),
            }
        }
        pub fn from_status_code(code: StatusCode) -> JsonError {
            JsonError {
                details: code.canonical_reason().unwrap().to_string(),
                line: None,
                error: "".to_string(),
            }
        }
        pub fn from_error(error: &str) -> JsonError {
            JsonError {
                details: "".to_string(),
                error: error.to_string(),
                line: None,
            }
        }
    }

    macro_rules! reply_error {
        ($status:expr) => {
            warp::reply::with_status(
                warp::reply::json(&JsonError::from_status_code($status)),
                $status,
            )
        };
    }

    macro_rules! try_json {
        ($val:expr, $status:expr) => {
            match $val {
                Ok(v) => v,
                Err(e) => {
                    return Ok(warp::reply::with_status(
                        warp::reply::json(&JsonError::new(
                            $status.canonical_reason().unwrap(),
                            line!(),
                            &e.to_string(),
                        )),
                        $status,
                    ));
                }
            }
        };
    }

    pub fn validate_rp_id(rp_id: &str) -> crate::Result<()> {
        if let Ok(uri) = rp_id.parse::<uri::Uri>().map_err(|_| {
            crate::errors::AuthenticatorError::U2FToken(crate::errors::U2FTokenError::Unknown)
        }) {
            if uri.scheme().is_none()
                && uri.path_and_query().is_none()
                && uri.port().is_none()
                && uri.host().is_some()
                && uri.authority().unwrap() == uri.host().unwrap()
                // Don't try too hard to ensure it's a valid domain, just
                // ensure there's a label delim in there somewhere
                && uri.host().unwrap().find('.').is_some()
            {
                return Ok(());
            }
        }
        Err(crate::errors::AuthenticatorError::U2FToken(
            crate::errors::U2FTokenError::Unknown,
        ))
    }

    pub async fn authenticator_add(
        auth: AuthenticatorConfiguration,
        state: Arc<Mutex<VirtualManagerState>>,
    ) -> Result<impl warp::Reply, Infallible> {
        let protocol = match auth.protocol.as_str() {
            "ctap1/u2f" => testtoken::TestWireProtocol::CTAP1,
            "ctap2" => testtoken::TestWireProtocol::CTAP2,
            _ => {
                return Ok(warp::reply::with_status(
                    warp::reply::json(&JsonError::from_error(
                        format!("unknown protocol: {}", auth.protocol).as_str(),
                    )),
                    StatusCode::BAD_REQUEST,
                ))
            }
        };

        let mut state_lock = try_json!(state.lock(), StatusCode::INTERNAL_SERVER_ERROR);
        let mut state_obj = state_lock.deref_mut();
        state_obj.authenticator_counter += 1;

        let tt = testtoken::TestToken::new(
            state_obj.authenticator_counter,
            protocol,
            auth.transport,
            auth.is_user_consenting,
            auth.has_user_verification,
            auth.is_user_verified,
            auth.has_resident_key,
        );

        match state_obj
            .tokens
            .binary_search_by_key(&state_obj.authenticator_counter, |probe| probe.id)
        {
            Ok(_) => panic!("unexpected repeat of authenticator_id"),
            Err(idx) => state_obj.tokens.insert(idx, tt),
        }

        #[derive(Serialize)]
        struct AddResult {
            #[serde(rename = "authenticatorId")]
            authenticator_id: u64,
        }

        Ok(warp::reply::with_status(
            warp::reply::json(&AddResult {
                authenticator_id: state_obj.authenticator_counter,
            }),
            StatusCode::CREATED,
        ))
    }

    pub async fn authenticator_delete(
        id: u64,
        state: Arc<Mutex<VirtualManagerState>>,
    ) -> Result<impl warp::Reply, Infallible> {
        let mut state_obj = try_json!(state.lock(), StatusCode::INTERNAL_SERVER_ERROR);
        match state_obj.tokens.binary_search_by_key(&id, |probe| probe.id) {
            Ok(idx) => state_obj.tokens.remove(idx),
            Err(_) => {
                return Ok(reply_error!(StatusCode::NOT_FOUND));
            }
        };

        Ok(warp::reply::with_status(
            warp::reply::json(&JsonSuccess::blank()),
            StatusCode::OK,
        ))
    }

    pub async fn authenticator_get(
        id: u64,
        state: Arc<Mutex<VirtualManagerState>>,
    ) -> Result<impl warp::Reply, Infallible> {
        let mut state_obj = try_json!(state.lock(), StatusCode::INTERNAL_SERVER_ERROR);
        if let Ok(idx) = state_obj.tokens.binary_search_by_key(&id, |probe| probe.id) {
            let tt = &mut state_obj.tokens[idx];

            let data = AuthenticatorConfiguration {
                protocol: tt.protocol.to_webdriver_string(),
                transport: tt.transport.clone(),
                has_resident_key: tt.has_resident_key,
                has_user_verification: tt.has_user_verification,
                is_user_consenting: tt.is_user_consenting,
                is_user_verified: tt.is_user_verified,
            };

            return Ok(warp::reply::with_status(
                warp::reply::json(&data),
                StatusCode::OK,
            ));
        }

        Ok(reply_error!(StatusCode::NOT_FOUND))
    }

    pub async fn authenticator_set_uv(
        id: u64,
        uv: UserVerificationParameters,
        state: Arc<Mutex<VirtualManagerState>>,
    ) -> Result<impl warp::Reply, Infallible> {
        let mut state_obj = try_json!(state.lock(), StatusCode::INTERNAL_SERVER_ERROR);

        if let Ok(idx) = state_obj.tokens.binary_search_by_key(&id, |probe| probe.id) {
            let tt = &mut state_obj.tokens[idx];
            tt.is_user_verified = uv.is_user_verified;
            return Ok(warp::reply::with_status(
                warp::reply::json(&JsonSuccess::blank()),
                StatusCode::OK,
            ));
        }

        Ok(reply_error!(StatusCode::NOT_FOUND))
    }

    pub async fn authenticator_credential_add(
        id: u64,
        auth: CredentialParameters,
        state: Arc<Mutex<VirtualManagerState>>,
    ) -> Result<impl warp::Reply, Infallible> {
        let credential = try_json!(
            base64::decode_config(&auth.credential_id, base64::URL_SAFE),
            StatusCode::BAD_REQUEST
        );

        let privkey = try_json!(
            base64::decode_config(&auth.private_key, base64::URL_SAFE),
            StatusCode::BAD_REQUEST
        );

        let userhandle = try_json!(
            base64::decode_config(&auth.user_handle, base64::URL_SAFE),
            StatusCode::BAD_REQUEST
        );

        try_json!(validate_rp_id(&auth.rp_id), StatusCode::BAD_REQUEST);

        let mut state_obj = try_json!(state.lock(), StatusCode::INTERNAL_SERVER_ERROR);
        if let Ok(idx) = state_obj.tokens.binary_search_by_key(&id, |probe| probe.id) {
            let tt = &mut state_obj.tokens[idx];

            tt.insert_credential(
                &credential,
                &privkey,
                auth.rp_id,
                auth.is_resident_credential,
                &userhandle,
                auth.sign_count,
            );

            return Ok(warp::reply::with_status(
                warp::reply::json(&JsonSuccess::blank()),
                StatusCode::CREATED,
            ));
        }

        Ok(reply_error!(StatusCode::NOT_FOUND))
    }

    pub async fn authenticator_credential_delete(
        id: u64,
        credential_id: String,
        state: Arc<Mutex<VirtualManagerState>>,
    ) -> Result<impl warp::Reply, Infallible> {
        let credential = try_json!(
            base64::decode_config(&credential_id, base64::URL_SAFE),
            StatusCode::BAD_REQUEST
        );

        let mut state_obj = try_json!(state.lock(), StatusCode::INTERNAL_SERVER_ERROR);

        debug!("Asking to delete  {}", &credential_id);

        if let Ok(idx) = state_obj.tokens.binary_search_by_key(&id, |probe| probe.id) {
            let tt = &mut state_obj.tokens[idx];
            debug!("Asking to delete from token {}", tt.id);
            if tt.delete_credential(&credential) {
                return Ok(warp::reply::with_status(
                    warp::reply::json(&JsonSuccess::blank()),
                    StatusCode::OK,
                ));
            }
        }

        Ok(reply_error!(StatusCode::NOT_FOUND))
    }

    pub async fn authenticator_credentials_get(
        id: u64,
        state: Arc<Mutex<VirtualManagerState>>,
    ) -> Result<impl warp::Reply, Infallible> {
        let mut state_obj = try_json!(state.lock(), StatusCode::INTERNAL_SERVER_ERROR);

        if let Ok(idx) = state_obj.tokens.binary_search_by_key(&id, |probe| probe.id) {
            let tt = &mut state_obj.tokens[idx];
            let mut creds: vec::Vec<CredentialParameters> = vec![];
            for ttc in &tt.credentials {
                creds.push(CredentialParameters::new_from_test_token_credential(ttc));
            }

            return Ok(warp::reply::with_status(
                warp::reply::json(&creds),
                StatusCode::OK,
            ));
        }

        Ok(reply_error!(StatusCode::NOT_FOUND))
    }

    pub async fn authenticator_credentials_clear(
        id: u64,
        state: Arc<Mutex<VirtualManagerState>>,
    ) -> Result<impl warp::Reply, Infallible> {
        let mut state_obj = try_json!(state.lock(), StatusCode::INTERNAL_SERVER_ERROR);
        if let Ok(idx) = state_obj.tokens.binary_search_by_key(&id, |probe| probe.id) {
            let tt = &mut state_obj.tokens[idx];

            tt.credentials.clear();

            return Ok(warp::reply::with_status(
                warp::reply::json(&JsonSuccess::blank()),
                StatusCode::OK,
            ));
        }

        Ok(reply_error!(StatusCode::NOT_FOUND))
    }
}

#[tokio::main]
pub async fn serve(state: Arc<Mutex<VirtualManagerState>>, addr: SocketAddr) {
    let routes = authenticator_add(state.clone())
        .or(authenticator_delete(state.clone()))
        .or(authenticator_get(state.clone()))
        .or(authenticator_set_uv(state.clone()))
        .or(authenticator_credential_add(state.clone()))
        .or(authenticator_credential_delete(state.clone()))
        .or(authenticator_credentials_get(state.clone()))
        .or(authenticator_credentials_clear(state.clone()));

    warp::serve(routes).run(addr).await;
}

#[cfg(test)]
mod tests {
    use super::handlers::validate_rp_id;
    use super::testtoken::*;
    use super::*;
    use crate::virtualdevices::webdriver::virtualmanager::VirtualManagerState;
    use std::sync::{Arc, Mutex};
    use warp::http::StatusCode;

    fn init() {
        let _ = env_logger::builder().is_test(true).try_init();
    }

    #[test]
    fn test_validate_rp_id() {
        init();

        assert_matches!(
            validate_rp_id(&String::from("http://example.com")),
            Err(crate::errors::AuthenticatorError::U2FToken(
                crate::errors::U2FTokenError::Unknown,
            ))
        );
        assert_matches!(
            validate_rp_id(&String::from("https://example.com")),
            Err(crate::errors::AuthenticatorError::U2FToken(
                crate::errors::U2FTokenError::Unknown,
            ))
        );
        assert_matches!(
            validate_rp_id(&String::from("example.com:443")),
            Err(crate::errors::AuthenticatorError::U2FToken(
                crate::errors::U2FTokenError::Unknown,
            ))
        );
        assert_matches!(
            validate_rp_id(&String::from("example.com/path")),
            Err(crate::errors::AuthenticatorError::U2FToken(
                crate::errors::U2FTokenError::Unknown,
            ))
        );
        assert_matches!(
            validate_rp_id(&String::from("example.com:443/path")),
            Err(crate::errors::AuthenticatorError::U2FToken(
                crate::errors::U2FTokenError::Unknown,
            ))
        );
        assert_matches!(
            validate_rp_id(&String::from("user:pass@example.com")),
            Err(crate::errors::AuthenticatorError::U2FToken(
                crate::errors::U2FTokenError::Unknown,
            ))
        );
        assert_matches!(
            validate_rp_id(&String::from("com")),
            Err(crate::errors::AuthenticatorError::U2FToken(
                crate::errors::U2FTokenError::Unknown,
            ))
        );
        assert_matches!(validate_rp_id(&String::from("example.com")), Ok(()));
    }

    fn mk_state_with_token_list(ids: &[u64]) -> Arc<Mutex<VirtualManagerState>> {
        let state = VirtualManagerState::new();

        {
            let mut state_obj = state.lock().unwrap();
            for id in ids {
                state_obj.tokens.push(TestToken::new(
                    *id,
                    TestWireProtocol::CTAP1,
                    "internal".to_string(),
                    true,
                    true,
                    true,
                    true,
                ));
            }

            state_obj.tokens.sort_by_key(|probe| probe.id)
        }

        state
    }

    fn assert_success_rsp_blank(body: &warp::hyper::body::Bytes) {
        assert_eq!(String::from_utf8_lossy(&body), r#"{}"#)
    }

    fn assert_creds_equals_test_token_params(
        a: &[CredentialParameters],
        b: &[TestTokenCredential],
    ) {
        assert_eq!(a.len(), b.len());

        for (i, j) in a.iter().zip(b.iter()) {
            assert_eq!(
                i.credential_id,
                base64::encode_config(&j.credential, base64::URL_SAFE)
            );
            assert_eq!(
                i.user_handle,
                base64::encode_config(&j.user_handle, base64::URL_SAFE)
            );
            assert_eq!(
                i.private_key,
                base64::encode_config(&j.privkey, base64::URL_SAFE)
            );
            assert_eq!(i.rp_id, j.rp_id);
            assert_eq!(i.sign_count, j.sign_count);
            assert_eq!(i.is_resident_credential, j.is_resident_credential);
        }
    }

    #[tokio::test]
    async fn test_authenticator_add() {
        init();
        let filter = authenticator_add(mk_state_with_token_list(&[]));

        {
            let res = warp::test::request()
                .method("POST")
                .path("/webauthn/authenticator")
                .reply(&filter)
                .await;
            assert!(res.status().is_client_error());
        }

        let valid_add = AuthenticatorConfiguration {
            protocol: "ctap1/u2f".to_string(),
            transport: "usb".to_string(),
            has_resident_key: false,
            has_user_verification: false,
            is_user_consenting: false,
            is_user_verified: false,
        };

        {
            let mut invalid = valid_add.clone();
            invalid.protocol = "unknown".to_string();
            let res = warp::test::request()
                .method("POST")
                .path("/webauthn/authenticator")
                .json(&invalid)
                .reply(&filter)
                .await;
            assert!(res.status().is_client_error());
            assert!(String::from_utf8_lossy(&res.body())
                .contains(&String::from("unknown protocol: unknown")));
        }

        {
            let mut unknown = valid_add.clone();
            unknown.transport = "unknown".to_string();
            let res = warp::test::request()
                .method("POST")
                .path("/webauthn/authenticator")
                .json(&unknown)
                .reply(&filter)
                .await;
            assert!(res.status().is_success());
            assert_eq!(
                String::from_utf8_lossy(&res.body()),
                r#"{"authenticatorId":1}"#
            )
        }

        {
            let res = warp::test::request()
                .method("POST")
                .path("/webauthn/authenticator")
                .json(&valid_add)
                .reply(&filter)
                .await;
            assert!(res.status().is_success());
            assert_eq!(
                String::from_utf8_lossy(&res.body()),
                r#"{"authenticatorId":2}"#
            )
        }
    }

    #[tokio::test]
    async fn test_authenticator_delete() {
        init();
        let filter = authenticator_delete(mk_state_with_token_list(&[32]));

        {
            let res = warp::test::request()
                .method("DELETE")
                .path("/webauthn/authenticator/3")
                .reply(&filter)
                .await;
            assert!(res.status().is_client_error());
        }

        {
            let res = warp::test::request()
                .method("DELETE")
                .path("/webauthn/authenticator/32")
                .reply(&filter)
                .await;
            assert!(res.status().is_success());
            assert_success_rsp_blank(res.body());
        }

        {
            let res = warp::test::request()
                .method("DELETE")
                .path("/webauthn/authenticator/42")
                .reply(&filter)
                .await;
            assert!(res.status().is_client_error());
        }
    }

    #[tokio::test]
    async fn test_authenticator_change_uv() {
        init();
        let state = mk_state_with_token_list(&[1]);
        let filter = authenticator_set_uv(state.clone());

        {
            let state_obj = state.lock().unwrap();
            assert_eq!(true, state_obj.tokens[0].is_user_verified);
        }

        {
            // Empty POST is bad
            let res = warp::test::request()
                .method("POST")
                .path("/webauthn/authenticator/1/uv")
                .reply(&filter)
                .await;
            assert!(res.status().is_client_error());
        }

        {
            // Unexpected POST structure is bad
            #[derive(Serialize)]
            struct Unexpected {
                id: u64,
            }
            let unexpected = Unexpected { id: 4 };

            let res = warp::test::request()
                .method("POST")
                .path("/webauthn/authenticator/1/uv")
                .json(&unexpected)
                .reply(&filter)
                .await;
            assert!(res.status().is_client_error());
        }

        {
            let param_false = UserVerificationParameters {
                is_user_verified: false,
            };

            let res = warp::test::request()
                .method("POST")
                .path("/webauthn/authenticator/1/uv")
                .json(&param_false)
                .reply(&filter)
                .await;
            assert_eq!(res.status(), 200);
            assert_success_rsp_blank(res.body());

            let state_obj = state.lock().unwrap();
            assert_eq!(false, state_obj.tokens[0].is_user_verified);
        }

        {
            let param_false = UserVerificationParameters {
                is_user_verified: true,
            };

            let res = warp::test::request()
                .method("POST")
                .path("/webauthn/authenticator/1/uv")
                .json(&param_false)
                .reply(&filter)
                .await;
            assert_eq!(res.status(), 200);
            assert_success_rsp_blank(res.body());

            let state_obj = state.lock().unwrap();
            assert_eq!(true, state_obj.tokens[0].is_user_verified);
        }
    }

    #[tokio::test]
    async fn test_authenticator_credentials() {
        init();
        let state = mk_state_with_token_list(&[1]);
        let filter = authenticator_credential_add(state.clone())
            .or(authenticator_credential_delete(state.clone()))
            .or(authenticator_credentials_get(state.clone()))
            .or(authenticator_credentials_clear(state.clone()));

        let valid_add_credential = CredentialParameters {
            credential_id: r"c3VwZXIgcmVhZGVy".to_string(),
            is_resident_credential: true,
            rp_id: "valid.rpid".to_string(),
            private_key: base64::encode_config(b"hello internet~", base64::URL_SAFE),
            user_handle: base64::encode_config(b"hello internet~", base64::URL_SAFE),
            sign_count: 0,
        };

        {
            let res = warp::test::request()
                .method("POST")
                .path("/webauthn/authenticator/1/credential")
                .reply(&filter)
                .await;
            assert!(res.status().is_client_error());
        }

        {
            let mut invalid = valid_add_credential.clone();
            invalid.credential_id = "!@#$ invalid base64".to_string();
            let res = warp::test::request()
                .method("POST")
                .path("/webauthn/authenticator/1/credential")
                .json(&invalid)
                .reply(&filter)
                .await;
            assert!(res.status().is_client_error());
        }

        {
            let mut invalid = valid_add_credential.clone();
            invalid.rp_id = "example".to_string();
            let res = warp::test::request()
                .method("POST")
                .path("/webauthn/authenticator/1/credential")
                .json(&invalid)
                .reply(&filter)
                .await;
            assert!(res.status().is_client_error());
        }

        {
            let mut invalid = valid_add_credential.clone();
            invalid.rp_id = "https://example.com".to_string();
            let res = warp::test::request()
                .method("POST")
                .path("/webauthn/authenticator/1/credential")
                .json(&invalid)
                .reply(&filter)
                .await;
            assert!(res.status().is_client_error());

            let state_obj = state.lock().unwrap();
            assert_eq!(0, state_obj.tokens[0].credentials.len());
        }

        {
            let mut no_user_handle = valid_add_credential.clone();
            no_user_handle.user_handle = "".to_string();
            no_user_handle.credential_id = "YQo=".to_string();
            let res = warp::test::request()
                .method("POST")
                .path("/webauthn/authenticator/1/credential")
                .json(&no_user_handle)
                .reply(&filter)
                .await;
            assert!(res.status().is_success());
            assert_success_rsp_blank(res.body());

            let state_obj = state.lock().unwrap();
            assert_eq!(1, state_obj.tokens[0].credentials.len());
            let c = &state_obj.tokens[0].credentials[0];
            assert!(c.user_handle.is_empty());
        }

        {
            let res = warp::test::request()
                .method("POST")
                .path("/webauthn/authenticator/1/credential")
                .json(&valid_add_credential)
                .reply(&filter)
                .await;
            assert_eq!(res.status(), StatusCode::CREATED);
            assert_success_rsp_blank(res.body());

            let state_obj = state.lock().unwrap();
            assert_eq!(2, state_obj.tokens[0].credentials.len());
            let c = &state_obj.tokens[0].credentials[1];
            assert!(!c.user_handle.is_empty());
        }

        {
            // Duplicate, should still be two credentials
            let res = warp::test::request()
                .method("POST")
                .path("/webauthn/authenticator/1/credential")
                .json(&valid_add_credential)
                .reply(&filter)
                .await;
            assert_eq!(res.status(), StatusCode::CREATED);
            assert_success_rsp_blank(res.body());

            let state_obj = state.lock().unwrap();
            assert_eq!(2, state_obj.tokens[0].credentials.len());
        }

        {
            let res = warp::test::request()
                .method("GET")
                .path("/webauthn/authenticator/1/credentials")
                .reply(&filter)
                .await;
            assert_eq!(res.status(), 200);
            let (_, body) = res.into_parts();
            let cred = serde_json::de::from_slice::<Vec<CredentialParameters>>(&body).unwrap();

            let state_obj = state.lock().unwrap();
            assert_creds_equals_test_token_params(&cred, &state_obj.tokens[0].credentials);
        }

        {
            let res = warp::test::request()
                .method("DELETE")
                .path("/webauthn/authenticator/1/credentials/YmxhbmsK")
                .reply(&filter)
                .await;
            assert_eq!(res.status(), 404);
        }

        {
            let res = warp::test::request()
                .method("DELETE")
                .path("/webauthn/authenticator/1/credentials/c3VwZXIgcmVhZGVy")
                .reply(&filter)
                .await;
            assert_eq!(res.status(), 200);
            assert_success_rsp_blank(res.body());

            let state_obj = state.lock().unwrap();
            assert_eq!(1, state_obj.tokens[0].credentials.len());
        }

        {
            let res = warp::test::request()
                .method("DELETE")
                .path("/webauthn/authenticator/1/credentials")
                .reply(&filter)
                .await;
            assert!(res.status().is_success());
            assert_success_rsp_blank(res.body());

            let state_obj = state.lock().unwrap();
            assert_eq!(0, state_obj.tokens[0].credentials.len());
        }
    }
}
