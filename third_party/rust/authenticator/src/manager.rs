/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::io;
use std::sync::mpsc::{channel, RecvTimeoutError, Sender};
use std::time::Duration;

use crate::authenticatorservice::AuthenticatorTransport;
use crate::consts::PARAMETER_SIZE;
use crate::errors::*;
use crate::statecallback::StateCallback;
use crate::statemachine::StateMachine;
use runloop::RunLoop;

enum QueueAction {
    Register {
        flags: crate::RegisterFlags,
        timeout: u64,
        challenge: Vec<u8>,
        application: crate::AppId,
        key_handles: Vec<crate::KeyHandle>,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    },
    Sign {
        flags: crate::SignFlags,
        timeout: u64,
        challenge: Vec<u8>,
        app_ids: Vec<crate::AppId>,
        key_handles: Vec<crate::KeyHandle>,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    },
    Cancel,
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
                    Ok(QueueAction::Register {
                        flags,
                        timeout,
                        challenge,
                        application,
                        key_handles,
                        status,
                        callback,
                    }) => {
                        // This must not block, otherwise we can't cancel.
                        sm.register(
                            flags,
                            timeout,
                            challenge,
                            application,
                            key_handles,
                            status,
                            callback,
                        );
                    }
                    Ok(QueueAction::Sign {
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
        flags: crate::RegisterFlags,
        timeout: u64,
        challenge: Vec<u8>,
        application: crate::AppId,
        key_handles: Vec<crate::KeyHandle>,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    ) -> crate::Result<()> {
        if challenge.len() != PARAMETER_SIZE || application.len() != PARAMETER_SIZE {
            return Err(AuthenticatorError::InvalidRelyingPartyInput);
        }

        for key_handle in &key_handles {
            if key_handle.credential.len() > 256 {
                return Err(AuthenticatorError::InvalidRelyingPartyInput);
            }
        }

        let action = QueueAction::Register {
            flags,
            timeout,
            challenge,
            application,
            key_handles,
            status,
            callback,
        };
        Ok(self.tx.send(action)?)
    }

    fn sign(
        &mut self,
        flags: crate::SignFlags,
        timeout: u64,
        challenge: Vec<u8>,
        app_ids: Vec<crate::AppId>,
        key_handles: Vec<crate::KeyHandle>,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    ) -> crate::Result<()> {
        if challenge.len() != PARAMETER_SIZE {
            return Err(AuthenticatorError::InvalidRelyingPartyInput);
        }

        if app_ids.is_empty() {
            return Err(AuthenticatorError::InvalidRelyingPartyInput);
        }

        for app_id in &app_ids {
            if app_id.len() != PARAMETER_SIZE {
                return Err(AuthenticatorError::InvalidRelyingPartyInput);
            }
        }

        for key_handle in &key_handles {
            if key_handle.credential.len() > 256 {
                return Err(AuthenticatorError::InvalidRelyingPartyInput);
            }
        }

        let action = QueueAction::Sign {
            flags,
            timeout,
            challenge,
            app_ids,
            key_handles,
            status,
            callback,
        };
        Ok(self.tx.send(action)?)
    }

    fn cancel(&mut self) -> crate::Result<()> {
        Ok(self.tx.send(QueueAction::Cancel)?)
    }
}

impl Drop for U2FManager {
    fn drop(&mut self) {
        self.queue.cancel();
    }
}
