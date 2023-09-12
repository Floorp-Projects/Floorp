/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::authenticatorservice::{RegisterArgs, SignArgs};

use crate::ctap2;

use crate::ctap2::commands::client_pin::Pin;

use crate::errors::AuthenticatorError;
use crate::statecallback::StateCallback;
use crate::status_update::{send_status, InteractiveUpdate};
use crate::transport::device_selector::{
    BlinkResult, Device, DeviceBuildParameters, DeviceCommand, DeviceSelectorEvent,
};
use crate::transport::platform::transaction::Transaction;
use crate::transport::{hid::HIDDevice, FidoDevice, FidoProtocol};
use crate::{InteractiveRequest, ManageResult};
use std::sync::mpsc::{channel, RecvTimeoutError, Sender};

use std::time::Duration;

#[derive(Default)]
pub struct StateMachine {
    transaction: Option<Transaction>,
}

impl StateMachine {
    pub fn new() -> Self {
        Default::default()
    }

    fn init_device(
        info: DeviceBuildParameters,
        selector: &Sender<DeviceSelectorEvent>,
    ) -> Option<Device> {
        // Create a new device.
        let mut dev = match Device::new(info) {
            Ok(dev) => dev,
            Err((e, id)) => {
                info!("error happened with device: {}", e);
                let _ = selector.send(DeviceSelectorEvent::NotAToken(id));
                return None;
            }
        };

        // Try initializing it.
        if let Err(e) = dev.init() {
            warn!("error while initializing device: {}", e);
            let _ = selector.send(DeviceSelectorEvent::NotAToken(dev.id()));
            return None;
        }

        Some(dev)
    }

    fn wait_for_device_selector(
        dev: &mut Device,
        selector: &Sender<DeviceSelectorEvent>,
        status: &Sender<crate::StatusUpdate>,
        keep_alive: &dyn Fn() -> bool,
    ) -> bool {
        let (tx, rx) = channel();
        if selector
            .send(DeviceSelectorEvent::ImAToken((dev.id(), tx)))
            .is_err()
        {
            // If we fail to register with the device selector, then we're not going
            // to be selected.
            return false;
        }

        // We can be cancelled from the user (through keep_alive()) or from the device selector
        // (through a DeviceCommand::Cancel on rx).  We'll combine those signals into a single
        // predicate to pass to Device::block_and_blink.
        let keep_blinking = || keep_alive() && !matches!(rx.try_recv(), Ok(DeviceCommand::Cancel));

        // Blocking recv. DeviceSelector will tell us what to do
        match rx.recv() {
            Ok(DeviceCommand::Blink) => {
                // The caller wants the user to choose a device. Send a status update and blink
                // this device. NOTE: We send one status update per device, so the recipient should be
                // prepared to receive the message multiple times.
                send_status(status, crate::StatusUpdate::SelectDeviceNotice);
                match dev.block_and_blink(&keep_blinking) {
                    BlinkResult::DeviceSelected => {
                        // User selected us. Let DeviceSelector know, so it can cancel all other
                        // outstanding open blink-requests. If we fail to send the SelectedToken
                        // message to the device selector, then don't consider this token as having
                        // been selected.
                        selector
                            .send(DeviceSelectorEvent::SelectedToken(dev.id()))
                            .is_ok()
                    }
                    BlinkResult::Cancelled => {
                        info!("Device {:?} was not selected", dev.id());
                        false
                    }
                }
            }
            Ok(DeviceCommand::Cancel) => {
                info!("Device {:?} was not selected", dev.id());
                false
            }
            Ok(DeviceCommand::Removed) => {
                info!("Device {:?} was removed", dev.id());
                false
            }
            Ok(DeviceCommand::Continue) => true,
            Err(_) => {
                warn!("Error when trying to receive messages from DeviceSelector! Exiting.");
                false
            }
        }
    }

    pub fn register(
        &mut self,
        timeout: u64,
        args: RegisterArgs,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    ) {
        // Abort any prior register/sign calls.
        self.cancel();
        let cbc = callback.clone();
        let transaction = Transaction::new(
            timeout,
            cbc.clone(),
            status,
            move |info, selector, status, alive| {
                let mut dev = match Self::init_device(info, &selector) {
                    Some(dev) => dev,
                    None => return,
                };
                if !Self::wait_for_device_selector(&mut dev, &selector, &status, alive) {
                    return;
                };

                if args.use_ctap1_fallback {
                    dev.downgrade_to_ctap1();
                }

                info!("Device {:?} continues with the register process", dev.id());
                if ctap2::register(&mut dev, args.clone(), status, callback.clone(), alive) {
                    // ctap2::register returns true if it called the callback with Ok(..).  Cancel
                    // all other tokens in case we skipped the "blinking"-action and went straight
                    // for the actual request.
                    let _ = selector.send(DeviceSelectorEvent::SelectedToken(dev.id()));
                }
            },
        );

        self.transaction = Some(try_or!(transaction, |e| cbc.call(Err(e))));
    }

    pub fn sign(
        &mut self,
        timeout: u64,
        args: SignArgs,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    ) {
        // Abort any prior register/sign calls.
        self.cancel();
        let cbc = callback.clone();

        let transaction = Transaction::new(
            timeout,
            callback.clone(),
            status,
            move |info, selector, status, alive| {
                let mut dev = match Self::init_device(info, &selector) {
                    Some(dev) => dev,
                    None => return,
                };
                if !Self::wait_for_device_selector(&mut dev, &selector, &status, alive) {
                    return;
                };

                if args.use_ctap1_fallback {
                    dev.downgrade_to_ctap1();
                }

                info!("Device {:?} continues with the signing process", dev.id());
                if ctap2::sign(&mut dev, args.clone(), status, callback.clone(), alive) {
                    // ctap2::sign returns true if it called the callback with Ok(..).  Cancel all
                    // other tokens in case we skipped the "blinking"-action and went straight for
                    // the actual request.
                    let _ = selector.send(DeviceSelectorEvent::SelectedToken(dev.id()));
                }
            },
        );

        self.transaction = Some(try_or!(transaction, move |e| cbc.call(Err(e))));
    }

    // This blocks.
    pub fn cancel(&mut self) {
        if let Some(mut transaction) = self.transaction.take() {
            info!("Statemachine was cancelled. Cancelling transaction now.");
            transaction.cancel();
        }
    }

    pub fn reset(
        &mut self,
        timeout: u64,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ResetResult>>,
    ) {
        // Abort any prior register/sign calls.
        self.cancel();
        let cbc = callback.clone();

        let transaction = Transaction::new(
            timeout,
            callback.clone(),
            status,
            move |info, selector, status, alive| {
                let mut dev = match Self::init_device(info, &selector) {
                    Some(dev) => dev,
                    None => return,
                };

                if dev.get_protocol() != FidoProtocol::CTAP2 {
                    info!("Device does not support CTAP2");
                    let _ = selector.send(DeviceSelectorEvent::NotAToken(dev.id()));
                    return;
                }

                if !Self::wait_for_device_selector(&mut dev, &selector, &status, alive) {
                    return;
                };
                ctap2::reset_helper(&mut dev, selector, status, callback.clone(), alive);
            },
        );

        self.transaction = Some(try_or!(transaction, move |e| cbc.call(Err(e))));
    }

    pub fn set_pin(
        &mut self,
        timeout: u64,
        new_pin: Pin,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ResetResult>>,
    ) {
        // Abort any prior register/sign calls.
        self.cancel();

        let cbc = callback.clone();

        let transaction = Transaction::new(
            timeout,
            callback.clone(),
            status,
            move |info, selector, status, alive| {
                let mut dev = match Self::init_device(info, &selector) {
                    Some(dev) => dev,
                    None => return,
                };

                if dev.get_protocol() != FidoProtocol::CTAP2 {
                    info!("Device does not support CTAP2");
                    let _ = selector.send(DeviceSelectorEvent::NotAToken(dev.id()));
                    return;
                }

                if !Self::wait_for_device_selector(&mut dev, &selector, &status, alive) {
                    return;
                };

                ctap2::set_or_change_pin_helper(
                    &mut dev,
                    None,
                    new_pin.clone(),
                    status,
                    callback.clone(),
                    alive,
                );
            },
        );
        self.transaction = Some(try_or!(transaction, move |e| cbc.call(Err(e))));
    }

    // Function to interactively manage a specific token.
    // Difference to register/sign: These want to do something and don't care
    // with which token they do it.
    // This function wants to manipulate a specific token. For this, we first
    // have to select one and then do something with it, based on what it
    // supports (Set PIN, Change PIN, Reset, etc.).
    // Hence, we first go through the discovery-phase, then provide the user
    // with the AuthenticatorInfo and then let them interactively decide what to do
    pub fn manage(
        &mut self,
        timeout: u64,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ManageResult>>,
    ) {
        // Abort any prior register/sign calls.
        self.cancel();
        let cbc = callback.clone();

        let transaction = Transaction::new(
            timeout,
            callback.clone(),
            status,
            move |info, selector, status, alive| {
                let mut dev = match Self::init_device(info, &selector) {
                    Some(dev) => dev,
                    None => return,
                };

                if dev.get_protocol() != FidoProtocol::CTAP2 {
                    info!("Device does not support CTAP2");
                    let _ = selector.send(DeviceSelectorEvent::NotAToken(dev.id()));
                    return;
                }

                if !Self::wait_for_device_selector(&mut dev, &selector, &status, alive) {
                    return;
                };

                info!("Device {:?} selected for interactive management.", dev.id());

                // Sending the user the info about the token
                let (tx, rx) = channel();
                send_status(
                    &status,
                    crate::StatusUpdate::InteractiveManagement(InteractiveUpdate::StartManagement(
                        (tx, dev.get_authenticator_info().cloned()),
                    )),
                );
                while alive() {
                    match rx.recv_timeout(Duration::from_millis(400)) {
                        Ok(InteractiveRequest::Quit) => {
                            callback.call(Ok(ManageResult::Success));
                            break;
                        }
                        Ok(InteractiveRequest::Reset) => {
                            ctap2::reset_helper(
                                &mut dev,
                                selector,
                                status,
                                callback.clone(),
                                alive,
                            );
                        }
                        Ok(InteractiveRequest::ChangePIN(curr_pin, new_pin)) => {
                            ctap2::set_or_change_pin_helper(
                                &mut dev,
                                Some(curr_pin),
                                new_pin,
                                status,
                                callback.clone(),
                                alive,
                            );
                        }
                        Ok(InteractiveRequest::SetPIN(pin)) => {
                            ctap2::set_or_change_pin_helper(
                                &mut dev,
                                None,
                                pin,
                                status,
                                callback.clone(),
                                alive,
                            );
                        }
                        Ok(InteractiveRequest::ChangeConfig(authcfg, puat)) => {
                            ctap2::configure_authenticator(
                                &mut dev,
                                puat,
                                authcfg,
                                status.clone(),
                                callback.clone(),
                                alive,
                            );
                            continue;
                        }
                        Ok(InteractiveRequest::CredentialManagement(cred_management, puat)) => {
                            ctap2::credential_management(
                                &mut dev,
                                puat,
                                cred_management,
                                status.clone(),
                                callback.clone(),
                                alive,
                            );
                            continue;
                        }
                        Ok(InteractiveRequest::BioEnrollment(bio_enrollment, puat)) => {
                            ctap2::bio_enrollment(
                                &mut dev,
                                puat,
                                bio_enrollment,
                                status.clone(),
                                callback.clone(),
                                alive,
                            );
                            continue;
                        }
                        Err(RecvTimeoutError::Timeout) => {
                            if !alive() {
                                // We got stopped at some point
                                callback.call(Err(AuthenticatorError::CancelledByUser));
                                break;
                            }
                            continue;
                        }
                        Err(RecvTimeoutError::Disconnected) => {
                            // recv() failed, because the other side is dropping the Sender.
                            info!(
                                "Callback dropped the channel, so we abort the interactive session"
                            );
                            callback.call(Err(AuthenticatorError::CancelledByUser));
                        }
                    }
                    break;
                }
            },
        );

        self.transaction = Some(try_or!(transaction, move |e| cbc.call(Err(e))));
    }
}
