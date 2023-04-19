/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::authenticatorservice::AuthenticatorTransport;
use crate::authenticatorservice::{RegisterArgs, SignArgs};

use crate::ctap2::client_data::ClientDataHash;
use crate::ctap2::commands::get_assertion::GetAssertion;
use crate::ctap2::commands::make_credentials::MakeCredentials;
use crate::ctap2::server::{RelyingParty, RelyingPartyWrapper};
use crate::errors::*;
use crate::statecallback::StateCallback;
use crate::statemachine::StateMachine;
use crate::Pin;
use runloop::RunLoop;
use std::io;
use std::sync::mpsc::{channel, RecvTimeoutError, Sender};
use std::time::Duration;

enum QueueAction {
    Register {
        timeout: u64,
        make_credentials: MakeCredentials,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    },
    Sign {
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

pub struct Manager {
    queue: RunLoop,
    tx: Sender<QueueAction>,
}

impl Manager {
    pub fn new() -> io::Result<Self> {
        let (tx, rx) = channel();

        // Start a new work queue thread.
        let queue = RunLoop::new(move |alive| {
            let mut sm = StateMachine::new();

            while alive() {
                match rx.recv_timeout(Duration::from_millis(50)) {
                    Ok(QueueAction::Register {
                        timeout,
                        make_credentials,
                        status,
                        callback,
                    }) => {
                        // This must not block, otherwise we can't cancel.
                        sm.register(timeout, make_credentials, status, callback);
                    }

                    Ok(QueueAction::Sign {
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
        args: RegisterArgs,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    ) -> Result<(), AuthenticatorError> {
        let make_credentials = MakeCredentials::new(
            ClientDataHash(args.client_data_hash),
            RelyingPartyWrapper::Data(args.relying_party),
            Some(args.user),
            args.pub_cred_params,
            args.exclude_list,
            args.options,
            args.extensions,
            args.pin,
            args.use_ctap1_fallback,
            // pin_auth will be filled in Statemachine, once we have a device
        )?;

        let action = QueueAction::Register {
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
        args: SignArgs,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    ) -> crate::Result<()> {
        let get_assertion = GetAssertion::new(
            ClientDataHash(args.client_data_hash),
            RelyingPartyWrapper::Data(RelyingParty {
                id: args.relying_party_id,
                name: None,
                icon: None,
            }),
            args.allow_list,
            args.options,
            args.extensions,
            args.pin,
            args.alternate_rp_id,
            args.use_ctap1_fallback,
        )?;

        let action = QueueAction::Sign {
            timeout,
            get_assertion,
            status,
            callback,
        };

        self.tx.send(action)?;
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
