/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::authenticatorservice::AuthenticatorTransport;
use crate::authenticatorservice::{RegisterArgs, RegisterArgsCtap1, SignArgs};
use crate::consts::PARAMETER_SIZE;
use crate::crypto::COSEAlgorithm;
use crate::ctap2::client_data::{CollectedClientData, WebauthnType};
use crate::ctap2::commands::get_assertion::{GetAssertion, GetAssertionOptions};
use crate::ctap2::commands::make_credentials::MakeCredentials;
use crate::ctap2::commands::make_credentials::MakeCredentialsOptions;
use crate::ctap2::server::{
    PublicKeyCredentialParameters, RelyingParty, RelyingPartyWrapper, RpIdHash,
};
use crate::errors::*;
use crate::statecallback::StateCallback;
use crate::statemachine::{StateMachine, StateMachineCtap2};
use crate::{Pin, SignFlags};
use runloop::RunLoop;
use std::io;
use std::sync::mpsc::{channel, RecvTimeoutError, Sender};
use std::time::Duration;

enum QueueAction {
    RegisterCtap1 {
        timeout: u64,
        ctap_args: RegisterArgsCtap1,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    },
    RegisterCtap2 {
        timeout: u64,
        make_credentials: MakeCredentials,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    },
    SignCtap1 {
        flags: crate::SignFlags,
        timeout: u64,
        challenge: Vec<u8>,
        app_ids: Vec<crate::AppId>,
        key_handles: Vec<crate::KeyHandle>,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    },
    SignCtap2 {
        timeout: u64,
        get_assertion: GetAssertion,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    },
    Cancel,
    Reset {
        timeout: u64,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ResetResult>>,
    },
    SetPin {
        timeout: u64,
        new_pin: Pin,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ResetResult>>,
    },
}

pub struct U2FManager {
    queue: RunLoop,
    tx: Sender<QueueAction>,
}

impl U2FManager {
    pub fn new() -> io::Result<Self> {
        let (tx, rx) = channel();

        // Start a new work queue thread.
        let queue = RunLoop::new(move |alive| {
            let mut sm = StateMachine::new();

            while alive() {
                match rx.recv_timeout(Duration::from_millis(50)) {
                    Ok(QueueAction::RegisterCtap1 {
                        timeout,
                        ctap_args,
                        status,
                        callback,
                    }) => {
                        // This must not block, otherwise we can't cancel.
                        sm.register(
                            ctap_args.flags,
                            timeout,
                            ctap_args.challenge,
                            ctap_args.application,
                            ctap_args.key_handles,
                            status,
                            callback,
                        );
                    }
                    Ok(QueueAction::SignCtap1 {
                        flags,
                        timeout,
                        challenge,
                        app_ids,
                        key_handles,
                        status,
                        callback,
                    }) => {
                        // This must not block, otherwise we can't cancel.
                        sm.sign(
                            flags,
                            timeout,
                            challenge,
                            app_ids,
                            key_handles,
                            status,
                            callback,
                        );
                    }
                    Ok(QueueAction::Cancel) => {
                        // Cancelling must block so that we don't start a new
                        // polling thread before the old one has shut down.
                        sm.cancel();
                    }
                    Ok(QueueAction::RegisterCtap2 { .. }) => {
                        // TODO(MS): What to do here? Error out? Silently ignore?
                        unimplemented!();
                    }
                    Ok(QueueAction::SignCtap2 { .. }) => {
                        // TODO(MS): What to do here? Error out? Silently ignore?
                        unimplemented!();
                    }
                    Ok(QueueAction::Reset { .. }) | Ok(QueueAction::SetPin { .. }) => {
                        unimplemented!();
                    }
                    Err(RecvTimeoutError::Disconnected) => {
                        break;
                    }
                    _ => { /* continue */ }
                }
            }

            // Cancel any ongoing activity.
            sm.cancel();
        })?;

        Ok(Self { queue, tx })
    }
}

impl AuthenticatorTransport for U2FManager {
    fn register(
        &mut self,
        timeout: u64,
        ctap_args: RegisterArgs,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    ) -> crate::Result<()> {
        let args = match ctap_args {
            RegisterArgs::CTAP1(args) => args,
            RegisterArgs::CTAP2(_) => {
                return Err(AuthenticatorError::VersionMismatch("U2FManager", 1));
            }
        };
        if args.challenge.len() != PARAMETER_SIZE || args.application.len() != PARAMETER_SIZE {
            return Err(AuthenticatorError::InvalidRelyingPartyInput);
        }

        for key_handle in &args.key_handles {
            if key_handle.credential.len() >= 256 {
                return Err(AuthenticatorError::InvalidRelyingPartyInput);
            }
        }

        let action = QueueAction::RegisterCtap1 {
            timeout,
            ctap_args: args,
            status,
            callback,
        };
        Ok(self.tx.send(action)?)
    }

    fn sign(
        &mut self,
        timeout: u64,
        ctap_args: SignArgs,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    ) -> crate::Result<()> {
        let args = match ctap_args {
            SignArgs::CTAP1(args) => args,
            SignArgs::CTAP2(_) => {
                return Err(AuthenticatorError::VersionMismatch("U2FManager", 1));
            }
        };

        if args.challenge.len() != PARAMETER_SIZE {
            return Err(AuthenticatorError::InvalidRelyingPartyInput);
        }

        if args.app_ids.is_empty() {
            return Err(AuthenticatorError::InvalidRelyingPartyInput);
        }

        for app_id in &args.app_ids {
            if app_id.len() != PARAMETER_SIZE {
                return Err(AuthenticatorError::InvalidRelyingPartyInput);
            }
        }

        for key_handle in &args.key_handles {
            if key_handle.credential.len() >= 256 {
                return Err(AuthenticatorError::InvalidRelyingPartyInput);
            }
        }

        let action = QueueAction::SignCtap1 {
            flags: args.flags,
            timeout,
            challenge: args.challenge,
            app_ids: args.app_ids,
            key_handles: args.key_handles,
            status,
            callback,
        };
        Ok(self.tx.send(action)?)
    }

    fn cancel(&mut self) -> crate::Result<()> {
        Ok(self.tx.send(QueueAction::Cancel)?)
    }

    fn reset(
        &mut self,
        timeout: u64,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ResetResult>>,
    ) -> crate::Result<()> {
        Ok(self.tx.send(QueueAction::Reset {
            timeout,
            status,
            callback,
        })?)
    }

    fn set_pin(
        &mut self,
        timeout: u64,
        new_pin: Pin,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ResetResult>>,
    ) -> crate::Result<()> {
        Ok(self.tx.send(QueueAction::SetPin {
            timeout,
            new_pin,
            status,
            callback,
        })?)
    }
}

impl Drop for U2FManager {
    fn drop(&mut self) {
        self.queue.cancel();
    }
}

pub struct Manager {
    queue: RunLoop,
    tx: Sender<QueueAction>,
}

impl Manager {
    pub fn new() -> io::Result<Self> {
        let (tx, rx) = channel();

        // Start a new work queue thread.
        let queue = RunLoop::new(move |alive| {
            let mut sm = StateMachineCtap2::new();

            while alive() {
                match rx.recv_timeout(Duration::from_millis(50)) {
                    Ok(QueueAction::RegisterCtap2 {
                        timeout,
                        make_credentials,
                        status,
                        callback,
                    }) => {
                        // This must not block, otherwise we can't cancel.
                        sm.register(timeout, make_credentials, status, callback);
                    }

                    Ok(QueueAction::SignCtap2 {
                        timeout,
                        get_assertion,
                        status,
                        callback,
                    }) => {
                        // This must not block, otherwise we can't cancel.
                        sm.sign(timeout, get_assertion, status, callback);
                    }

                    Ok(QueueAction::Cancel) => {
                        // Cancelling must block so that we don't start a new
                        // polling thread before the old one has shut down.
                        sm.cancel();
                    }

                    Ok(QueueAction::Reset {
                        timeout,
                        status,
                        callback,
                    }) => {
                        // Reset the token: Delete all keypairs, reset PIN
                        sm.reset(timeout, status, callback);
                    }

                    Ok(QueueAction::SetPin {
                        timeout,
                        new_pin,
                        status,
                        callback,
                    }) => {
                        // This must not block, otherwise we can't cancel.
                        sm.set_pin(timeout, new_pin, status, callback);
                    }

                    Ok(QueueAction::RegisterCtap1 {
                        timeout: _,
                        ctap_args: _,
                        status: _,
                        callback: _,
                    }) => {
                        // TODO(MS): Remove QueueAction::RegisterCtap1 once U2FManager is deleted.
                        //           The repackaging from CTAP1 to CTAP2 happens in self.register()
                        unimplemented!();
                    }

                    Ok(QueueAction::SignCtap1 {
                        timeout: _,
                        callback: _,
                        flags: _,
                        challenge: _,
                        app_ids: _,
                        key_handles: _,
                        status: _,
                    }) => {
                        // TODO(MS): Remove QueueAction::SignCtap1 once U2FManager is deleted.
                        //           The repackaging from CTAP1 to CTAP2 happens in self.sign()
                        unimplemented!()
                    }

                    Err(RecvTimeoutError::Disconnected) => {
                        break;
                    }

                    _ => { /* continue */ }
                }
            }

            // Cancel any ongoing activity.
            sm.cancel();
        })?;

        Ok(Self { queue, tx })
    }
}

impl Drop for Manager {
    fn drop(&mut self) {
        self.queue.cancel();
    }
}

impl AuthenticatorTransport for Manager {
    fn register(
        &mut self,
        timeout: u64,
        ctap_args: RegisterArgs,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    ) -> Result<(), AuthenticatorError> {
        let make_credentials = match ctap_args {
            RegisterArgs::CTAP2(args) => {
                let client_data = CollectedClientData {
                    webauthn_type: WebauthnType::Create,
                    challenge: args.challenge.into(),
                    origin: args.origin,
                    cross_origin: false,
                    token_binding: None,
                };

                MakeCredentials::new(
                    client_data,
                    RelyingPartyWrapper::Data(args.relying_party),
                    Some(args.user),
                    args.pub_cred_params,
                    args.exclude_list,
                    args.options,
                    args.extensions,
                    args.pin,
                    // pin_auth will be filled in Statemachine, once we have a device
                )
            }
            RegisterArgs::CTAP1(args) => {
                let client_data = CollectedClientData {
                    webauthn_type: WebauthnType::Create,
                    challenge: args.challenge.into(),
                    origin: String::new(),
                    cross_origin: false,
                    token_binding: None,
                };

                MakeCredentials::new(
                    client_data,
                    RelyingPartyWrapper::Hash(RpIdHash::from(&args.application)?),
                    None,
                    vec![PublicKeyCredentialParameters {
                        alg: COSEAlgorithm::ES256,
                    }],
                    args.key_handles
                        .iter()
                        .map(|k| k.into())
                        .collect::<Vec<_>>(),
                    MakeCredentialsOptions {
                        resident_key: None,
                        user_verification: None,
                    },
                    Default::default(),
                    None,
                )
            }
        }?;

        let action = QueueAction::RegisterCtap2 {
            timeout,
            make_credentials,
            status,
            callback,
        };
        Ok(self.tx.send(action)?)
    }

    fn sign(
        &mut self,
        timeout: u64,
        ctap_args: SignArgs,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    ) -> crate::Result<()> {
        match ctap_args {
            SignArgs::CTAP1(args) => {
                if args.challenge.len() != PARAMETER_SIZE {
                    return Err(AuthenticatorError::InvalidRelyingPartyInput);
                }

                if args.app_ids.is_empty() {
                    return Err(AuthenticatorError::InvalidRelyingPartyInput);
                }

                let client_data = CollectedClientData {
                    webauthn_type: WebauthnType::Get,
                    challenge: args.challenge.into(),
                    origin: String::new(),
                    cross_origin: false,
                    token_binding: None,
                };
                let options = if args.flags == SignFlags::empty() {
                    GetAssertionOptions::default()
                } else {
                    GetAssertionOptions {
                        user_verification: Some(
                            args.flags.contains(SignFlags::REQUIRE_USER_VERIFICATION),
                        ),
                        ..GetAssertionOptions::default()
                    }
                };

                for app_id in &args.app_ids {
                    if app_id.len() != PARAMETER_SIZE {
                        return Err(AuthenticatorError::InvalidRelyingPartyInput);
                    }
                    for key_handle in &args.key_handles {
                        if key_handle.credential.len() >= 256 {
                            return Err(AuthenticatorError::InvalidRelyingPartyInput);
                        }
                        let rp = RelyingPartyWrapper::Hash(RpIdHash::from(&app_id)?);

                        let allow_list = vec![key_handle.into()];

                        let get_assertion = GetAssertion::new(
                            client_data.clone(),
                            rp,
                            allow_list,
                            options,
                            Default::default(),
                            None,
                        )?;

                        let action = QueueAction::SignCtap2 {
                            timeout,
                            get_assertion,
                            status: status.clone(),
                            callback: callback.clone(),
                        };
                        self.tx.send(action)?;
                    }
                }
            }

            SignArgs::CTAP2(args) => {
                let client_data = CollectedClientData {
                    webauthn_type: WebauthnType::Get,
                    challenge: args.challenge.into(),
                    origin: args.origin,
                    cross_origin: false,
                    token_binding: None,
                };

                let get_assertion = GetAssertion::new(
                    client_data.clone(),
                    RelyingPartyWrapper::Data(RelyingParty {
                        id: args.relying_party_id,
                        name: None,
                        icon: None,
                    }),
                    args.allow_list,
                    args.options,
                    args.extensions,
                    args.pin,
                )?;

                let action = QueueAction::SignCtap2 {
                    timeout,
                    get_assertion,
                    status,
                    callback,
                };
                self.tx.send(action)?;
            }
        };
        Ok(())
    }

    fn cancel(&mut self) -> Result<(), AuthenticatorError> {
        Ok(self.tx.send(QueueAction::Cancel)?)
    }

    fn reset(
        &mut self,
        timeout: u64,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ResetResult>>,
    ) -> Result<(), AuthenticatorError> {
        Ok(self.tx.send(QueueAction::Reset {
            timeout,
            status,
            callback,
        })?)
    }

    fn set_pin(
        &mut self,
        timeout: u64,
        new_pin: Pin,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ResetResult>>,
    ) -> crate::Result<()> {
        Ok(self.tx.send(QueueAction::SetPin {
            timeout,
            new_pin,
            status,
            callback,
        })?)
    }
}
