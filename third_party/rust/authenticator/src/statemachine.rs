/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::authenticatorservice::{RegisterArgs, SignArgs};
use crate::consts::PARAMETER_SIZE;

use crate::ctap2;
use crate::ctap2::client_data::ClientDataHash;
use crate::ctap2::commands::client_pin::Pin;
use crate::ctap2::commands::get_assertion::GetAssertionResult;
use crate::ctap2::commands::make_credentials::MakeCredentialsResult;

use crate::ctap2::server::{
    PublicKeyCredentialDescriptor, RelyingParty, RelyingPartyWrapper, ResidentKeyRequirement,
    RpIdHash, UserVerificationRequirement,
};
use crate::errors::{self, AuthenticatorError};
use crate::statecallback::StateCallback;
use crate::status_update::send_status;
use crate::transport::device_selector::{
    BlinkResult, Device, DeviceBuildParameters, DeviceCommand, DeviceSelectorEvent,
};
use crate::transport::platform::transaction::Transaction;
use crate::transport::{hid::HIDDevice, FidoDevice, FidoProtocol};
use crate::u2fprotocol::{u2f_init_device, u2f_is_keyhandle_valid, u2f_register, u2f_sign};
use crate::{
    AuthenticatorTransports, InteractiveRequest, KeyHandle, RegisterFlags, RegisterResult,
    SignFlags, SignResult,
};
use std::sync::mpsc::{channel, RecvTimeoutError, Sender};
use std::thread;
use std::time::Duration;

fn is_valid_transport(transports: crate::AuthenticatorTransports) -> bool {
    transports.is_empty() || transports.contains(crate::AuthenticatorTransports::USB)
}

fn find_valid_key_handles<'a, F>(
    app_ids: &'a [crate::AppId],
    key_handles: &'a [crate::KeyHandle],
    mut is_valid: F,
) -> (&'a crate::AppId, Vec<&'a crate::KeyHandle>)
where
    F: FnMut(&Vec<u8>, &crate::KeyHandle) -> bool,
{
    // Try all given app_ids in order.
    for app_id in app_ids {
        // Find all valid key handles for the current app_id.
        let valid_handles = key_handles
            .iter()
            .filter(|key_handle| is_valid(app_id, key_handle))
            .collect::<Vec<_>>();

        // If there's at least one, stop.
        if !valid_handles.is_empty() {
            return (app_id, valid_handles);
        }
    }

    (&app_ids[0], vec![])
}

#[derive(Default)]
pub struct StateMachine {
    transaction: Option<Transaction>,
}

impl StateMachine {
    pub fn new() -> Self {
        Default::default()
    }

    fn init_and_select(
        info: DeviceBuildParameters,
        selector: &Sender<DeviceSelectorEvent>,
        status: &Sender<crate::StatusUpdate>,
        ctap2_only: bool,
        keep_alive: &dyn Fn() -> bool,
    ) -> Option<Device> {
        // Create a new device.
        let mut dev = match Device::new(info) {
            Ok(dev) => dev,
            Err((e, id)) => {
                info!("error happened with device: {}", e);
                selector.send(DeviceSelectorEvent::NotAToken(id)).ok()?;
                return None;
            }
        };

        // Try initializing it.
        if let Err(e) = dev.init() {
            warn!("error while initializing device: {}", e);
            selector.send(DeviceSelectorEvent::NotAToken(dev.id())).ok();
            return None;
        }

        if ctap2_only && dev.get_protocol() != FidoProtocol::CTAP2 {
            info!("Device does not support CTAP2");
            selector.send(DeviceSelectorEvent::NotAToken(dev.id())).ok();
            return None;
        }

        let (tx, rx) = channel();
        selector
            .send(DeviceSelectorEvent::ImAToken((dev.id(), tx)))
            .ok()?;

        // We can be cancelled from the user (through keep_alive()) or from the device selector
        // (through a DeviceCommand::Cancel on rx).  We'll combine those signals into a single
        // predicate to pass to Device::block_and_blink.
        let keep_blinking = || keep_alive() && !matches!(rx.try_recv(), Ok(DeviceCommand::Cancel));

        // Blocking recv. DeviceSelector will tell us what to do
        match rx.recv() {
            Ok(DeviceCommand::Blink) => {
                // Inform the user that there are multiple devices available.
                // NOTE: We'll send this once per device, so the recipient should be prepared
                // to receive this message multiple times.
                send_status(status, crate::StatusUpdate::SelectDeviceNotice);
                match dev.block_and_blink(&keep_blinking) {
                    BlinkResult::DeviceSelected => {
                        // User selected us. Let DeviceSelector know, so it can cancel all other
                        // outstanding open blink-requests.
                        selector
                            .send(DeviceSelectorEvent::SelectedToken(dev.id()))
                            .ok()?;
                    }
                    BlinkResult::Cancelled => {
                        info!("Device {:?} was not selected", dev.id());
                        return None;
                    }
                }
            }
            Ok(DeviceCommand::Cancel) => {
                info!("Device {:?} was not selected", dev.id());
                return None;
            }
            Ok(DeviceCommand::Removed) => {
                info!("Device {:?} was removed", dev.id());
                return None;
            }
            Ok(DeviceCommand::Continue) => {
                // Just continue
            }
            Err(_) => {
                warn!("Error when trying to receive messages from DeviceSelector! Exiting.");
                return None;
            }
        }
        Some(dev)
    }

    pub fn register(
        &mut self,
        timeout: u64,
        args: RegisterArgs,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    ) {
        if args.use_ctap1_fallback {
            /* Firefox uses this when security.webauthn.ctap2 is false. */
            let mut flags = RegisterFlags::empty();
            if args.resident_key_req == ResidentKeyRequirement::Required {
                flags |= RegisterFlags::REQUIRE_RESIDENT_KEY;
            }
            if args.user_verification_req == UserVerificationRequirement::Required {
                flags |= RegisterFlags::REQUIRE_USER_VERIFICATION;
            }

            let rp = RelyingPartyWrapper::Data(args.relying_party);
            let application = rp.hash().as_ref().to_vec();
            let key_handles = args
                .exclude_list
                .iter()
                .map(|cred_desc| KeyHandle {
                    credential: cred_desc.id.clone(),
                    transports: AuthenticatorTransports::empty(),
                })
                .collect();
            let challenge = ClientDataHash(args.client_data_hash);

            self.legacy_register(
                flags,
                timeout,
                challenge,
                application,
                key_handles,
                status,
                callback,
            );
            return;
        }

        // Abort any prior register/sign calls.
        self.cancel();
        let cbc = callback.clone();
        let transaction = Transaction::new(
            timeout,
            cbc.clone(),
            status,
            move |info, selector, status, alive| {
                let mut dev = match Self::init_and_select(info, &selector, &status, false, alive) {
                    None => {
                        return;
                    }
                    Some(dev) => dev,
                };

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
        if args.use_ctap1_fallback {
            /* Firefox uses this when security.webauthn.ctap2 is false. */
            let mut flags = SignFlags::empty();
            if args.user_verification_req == UserVerificationRequirement::Required {
                flags |= SignFlags::REQUIRE_USER_VERIFICATION;
            }
            let mut app_ids = vec![];
            let rp_id = RelyingPartyWrapper::Data(RelyingParty {
                id: args.relying_party_id,
                ..Default::default()
            });
            app_ids.push(rp_id.hash().as_ref().to_vec());
            if let Some(app_id) = args.alternate_rp_id {
                let app_id = RelyingPartyWrapper::Data(RelyingParty {
                    id: app_id,
                    ..Default::default()
                });
                app_ids.push(app_id.hash().as_ref().to_vec());
            }
            let key_handles = args
                .allow_list
                .iter()
                .map(|cred_desc| KeyHandle {
                    credential: cred_desc.id.clone(),
                    transports: AuthenticatorTransports::empty(),
                })
                .collect();
            let challenge = ClientDataHash(args.client_data_hash);

            self.legacy_sign(
                flags,
                timeout,
                challenge,
                app_ids,
                key_handles,
                status,
                callback,
            );
            return;
        }

        // Abort any prior register/sign calls.
        self.cancel();
        let cbc = callback.clone();

        let transaction = Transaction::new(
            timeout,
            callback.clone(),
            status,
            move |info, selector, status, alive| {
                let mut dev = match Self::init_and_select(info, &selector, &status, false, alive) {
                    None => {
                        return;
                    }
                    Some(dev) => dev,
                };

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
                let mut dev = match Self::init_and_select(info, &selector, &status, true, alive) {
                    None => {
                        return;
                    }
                    Some(dev) => dev,
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
                let mut dev = match Self::init_and_select(info, &selector, &status, true, alive) {
                    None => {
                        return;
                    }
                    Some(dev) => dev,
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

    pub fn legacy_register(
        &mut self,
        flags: crate::RegisterFlags,
        timeout: u64,
        challenge: ClientDataHash,
        application: crate::AppId,
        key_handles: Vec<crate::KeyHandle>,
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
            move |info, _, status, alive| {
                // Create a new device.
                let dev = &mut match Device::new(info) {
                    Ok(dev) => dev,
                    _ => return,
                };

                // Try initializing it.
                if !dev.is_u2f() || !u2f_init_device(dev) {
                    return;
                }

                // This legacy code doesn't use `FidoDevice::get_protocol()`, but let's ensure the
                // protocol is set to CTAP1 as a precaution.
                dev.downgrade_to_ctap1();

                // We currently support none of the authenticator selection
                // criteria because we can't ask tokens whether they do support
                // those features. If flags are set, ignore all tokens for now.
                //
                // Technically, this is a ConstraintError because we shouldn't talk
                // to this authenticator in the first place. But the result is the
                // same anyway.
                if !flags.is_empty() {
                    return;
                }

                // Iterate the exclude list and see if there are any matches.
                // If so, we'll keep polling the device anyway to test for user
                // consent, to be consistent with CTAP2 device behavior.
                let excluded = key_handles.iter().any(|key_handle| {
                    is_valid_transport(key_handle.transports)
                        && u2f_is_keyhandle_valid(
                            dev,
                            challenge.as_ref(),
                            &application,
                            &key_handle.credential,
                        )
                        .unwrap_or(false) /* no match on failure */
                });

                send_status(&status, crate::StatusUpdate::PresenceRequired);

                while alive() {
                    if excluded {
                        let blank = vec![0u8; PARAMETER_SIZE];
                        if u2f_register(dev, &blank, &blank).is_ok() {
                            callback.call(Err(errors::AuthenticatorError::U2FToken(
                                errors::U2FTokenError::InvalidState,
                            )));
                            break;
                        }
                    } else if let Ok(bytes) = u2f_register(dev, challenge.as_ref(), &application) {
                        let mut rp_id_hash: RpIdHash = RpIdHash([0u8; 32]);
                        rp_id_hash.0.copy_from_slice(&application);
                        let result = match MakeCredentialsResult::from_ctap1(&bytes, &rp_id_hash) {
                            Ok(MakeCredentialsResult(att_obj)) => att_obj,
                            Err(_) => {
                                callback.call(Err(errors::AuthenticatorError::U2FToken(
                                    errors::U2FTokenError::Unknown,
                                )));
                                break;
                            }
                        };
                        callback.call(Ok(RegisterResult::CTAP2(result)));
                        break;
                    }

                    // Sleep a bit before trying again.
                    thread::sleep(Duration::from_millis(100));
                }
            },
        );

        self.transaction = Some(try_or!(transaction, |e| cbc.call(Err(e))));
    }

    pub fn legacy_sign(
        &mut self,
        flags: crate::SignFlags,
        timeout: u64,
        challenge: ClientDataHash,
        app_ids: Vec<crate::AppId>,
        key_handles: Vec<crate::KeyHandle>,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    ) {
        // Abort any prior register/sign calls.
        self.cancel();

        let cbc = callback.clone();

        let transaction = Transaction::new(
            timeout,
            cbc.clone(),
            status,
            move |info, _, status, alive| {
                // Create a new device.
                let dev = &mut match Device::new(info) {
                    Ok(dev) => dev,
                    _ => return,
                };

                // Try initializing it.
                if !dev.is_u2f() || !u2f_init_device(dev) {
                    return;
                }

                // This legacy code doesn't use `FidoDevice::get_protocol()`, but let's ensure the
                // protocol is set to CTAP1 as a precaution.
                dev.downgrade_to_ctap1();

                // We currently don't support user verification because we can't
                // ask tokens whether they do support that. If the flag is set,
                // ignore all tokens for now.
                //
                // Technically, this is a ConstraintError because we shouldn't talk
                // to this authenticator in the first place. But the result is the
                // same anyway.
                if !flags.is_empty() {
                    return;
                }

                // For each appId, try all key handles. If there's at least one
                // valid key handle for an appId, we'll use that appId below.
                let (app_id, valid_handles) =
                    find_valid_key_handles(&app_ids, &key_handles, |app_id, key_handle| {
                        u2f_is_keyhandle_valid(
                            dev,
                            challenge.as_ref(),
                            app_id,
                            &key_handle.credential,
                        )
                        .unwrap_or(false) /* no match on failure */
                    });

                // Aggregate distinct transports from all given credentials.
                let transports = key_handles
                    .iter()
                    .fold(crate::AuthenticatorTransports::empty(), |t, k| {
                        t | k.transports
                    });

                // We currently only support USB. If the RP specifies transports
                // and doesn't include USB it's probably lying.
                if !is_valid_transport(transports) {
                    return;
                }

                send_status(&status, crate::StatusUpdate::PresenceRequired);

                'outer: while alive() {
                    // If the device matches none of the given key handles
                    // then just make it blink with bogus data.
                    if valid_handles.is_empty() {
                        let blank = vec![0u8; PARAMETER_SIZE];
                        if u2f_register(dev, &blank, &blank).is_ok() {
                            callback.call(Err(errors::AuthenticatorError::U2FToken(
                                errors::U2FTokenError::InvalidState,
                            )));
                            break;
                        }
                    } else {
                        // Otherwise, try to sign.
                        for key_handle in &valid_handles {
                            if let Ok(bytes) =
                                u2f_sign(dev, challenge.as_ref(), app_id, &key_handle.credential)
                            {
                                let pkcd = PublicKeyCredentialDescriptor {
                                    id: key_handle.credential.clone(),
                                    transports: vec![],
                                };
                                let mut rp_id_hash: RpIdHash = RpIdHash([0u8; 32]);
                                rp_id_hash.0.copy_from_slice(app_id);
                                let result = match GetAssertionResult::from_ctap1(
                                    &bytes,
                                    &rp_id_hash,
                                    &pkcd,
                                ) {
                                    Ok(assertions) => assertions,
                                    Err(_) => {
                                        callback.call(Err(errors::AuthenticatorError::U2FToken(
                                            errors::U2FTokenError::Unknown,
                                        )));
                                        break 'outer;
                                    }
                                };
                                callback.call(Ok(SignResult::CTAP2(result)));
                                break 'outer;
                            }
                        }
                    }

                    // Sleep a bit before trying again.
                    thread::sleep(Duration::from_millis(100));
                }
            },
        );

        self.transaction = Some(try_or!(transaction, |e| cbc.call(Err(e))));
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
                let mut dev = match Self::init_and_select(info, &selector, &status, true, alive) {
                    None => {
                        return;
                    }
                    Some(dev) => dev,
                };

                info!("Device {:?} selected for interactive management.", dev.id());

                // Sending the user the info about the token
                let (tx, rx) = channel();
                send_status(
                    &status,
                    crate::StatusUpdate::InteractiveManagement((
                        tx,
                        dev.get_authenticator_info().cloned(),
                    )),
                );
                while alive() {
                    match rx.recv_timeout(Duration::from_millis(400)) {
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
