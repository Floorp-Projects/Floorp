/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::errors;
use crate::virtualdevices::software_u2f::SoftwareU2FToken;
use crate::{RegisterFlags, RegisterResult, SignFlags, SignResult};

pub enum TestWireProtocol {
    CTAP1,
    CTAP2,
}

impl TestWireProtocol {
    pub fn to_webdriver_string(&self) -> String {
        match self {
            TestWireProtocol::CTAP1 => "ctap1/u2f".to_string(),
            TestWireProtocol::CTAP2 => "ctap2".to_string(),
        }
    }
}

pub struct TestTokenCredential {
    pub credential: Vec<u8>,
    pub privkey: Vec<u8>,
    pub user_handle: Vec<u8>,
    pub sign_count: u64,
    pub is_resident_credential: bool,
    pub rp_id: String,
}

pub struct TestToken {
    pub id: u64,
    pub protocol: TestWireProtocol,
    pub transport: String,
    pub is_user_consenting: bool,
    pub has_user_verification: bool,
    pub is_user_verified: bool,
    pub has_resident_key: bool,
    pub u2f_impl: Option<SoftwareU2FToken>,
    pub credentials: Vec<TestTokenCredential>,
}

impl TestToken {
    pub fn new(
        id: u64,
        protocol: TestWireProtocol,
        transport: String,
        is_user_consenting: bool,
        has_user_verification: bool,
        is_user_verified: bool,
        has_resident_key: bool,
    ) -> TestToken {
        match protocol {
            TestWireProtocol::CTAP1 => Self {
                id,
                protocol,
                transport,
                is_user_consenting,
                has_user_verification,
                is_user_verified,
                has_resident_key,
                u2f_impl: Some(SoftwareU2FToken::new()),
                credentials: Vec::new(),
            },
            _ => unreachable!(),
        }
    }

    pub fn insert_credential(
        &mut self,
        credential: &[u8],
        privkey: &[u8],
        rp_id: String,
        is_resident_credential: bool,
        user_handle: &[u8],
        sign_count: u64,
    ) {
        let c = TestTokenCredential {
            credential: credential.to_vec(),
            privkey: privkey.to_vec(),
            rp_id,
            is_resident_credential,
            user_handle: user_handle.to_vec(),
            sign_count,
        };

        match self
            .credentials
            .binary_search_by_key(&credential, |probe| &probe.credential)
        {
            Ok(_) => {}
            Err(idx) => self.credentials.insert(idx, c),
        }
    }

    pub fn delete_credential(&mut self, credential: &[u8]) -> bool {
        debug!("Asking to delete credential",);
        if let Ok(idx) = self
            .credentials
            .binary_search_by_key(&credential, |probe| &probe.credential)
        {
            debug!("Asking to delete credential from idx {}", idx);
            self.credentials.remove(idx);
            return true;
        }

        false
    }

    pub fn register(&self) -> crate::Result<RegisterResult> {
        if self.u2f_impl.is_some() {
            return self.u2f_impl.as_ref().unwrap().register(
                RegisterFlags::empty(),
                10_000,
                vec![0; 32],
                vec![0; 32],
                vec![],
            );
        }
        Err(errors::AuthenticatorError::U2FToken(
            errors::U2FTokenError::Unknown,
        ))
    }

    pub fn sign(&self) -> crate::Result<SignResult> {
        if self.u2f_impl.is_some() {
            return self.u2f_impl.as_ref().unwrap().sign(
                SignFlags::empty(),
                10_000,
                vec![0; 32],
                vec![vec![0; 32]],
                vec![],
            );
        }
        Err(errors::AuthenticatorError::U2FToken(
            errors::U2FTokenError::Unknown,
        ))
    }
}
