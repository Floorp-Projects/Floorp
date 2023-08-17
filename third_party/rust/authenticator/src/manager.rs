/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::authenticatorservice::AuthenticatorTransport;
use crate::authenticatorservice::{RegisterArgs, SignArgs};
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
        register_args: RegisterArgs,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    },
    Sign {
        timeout: u64,
        sign_args: SignArgs,
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
    InteractiveManagement {
        timeout: u64,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ManageResult>>,
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
                        register_args,
                        status,
                        callback,
                    }) => {
                        // This must not block, otherwise we can't cancel.
                        sm.register(timeout, register_args, status, callback);
                    }

                    Ok(QueueAction::Sign {
                        timeout,
                        sign_args,
                        status,
                        callback,
                    }) => {
                        // This must not block, otherwise we can't cancel.
                        sm.sign(timeout, sign_args, status, callback);
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

                    Ok(QueueAction::InteractiveManagement {
                        timeout,
                        status,
                        callback,
                    }) => {
                        // Manage token interactively
                        sm.manage(timeout, status, callback);
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
        register_args: RegisterArgs,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    ) -> Result<(), AuthenticatorError> {
        let action = QueueAction::Register {
            timeout,
            register_args,
            status,
            callback,
        };
        Ok(self.tx.send(action)?)
    }

    fn sign(
        &mut self,
        timeout: u64,
        sign_args: SignArgs,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    ) -> crate::Result<()> {
        let action = QueueAction::Sign {
            timeout,
            sign_args,
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

    fn manage(
        &mut self,
        timeout: u64,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ManageResult>>,
    ) -> Result<(), AuthenticatorError> {
        Ok(self.tx.send(QueueAction::InteractiveManagement {
            timeout,
            status,
            callback,
        })?)
    }
}
