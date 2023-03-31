/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::consts::PARAMETER_SIZE;
use crate::ctap2::commands::client_pin::{ChangeExistingPin, Pin, PinError, SetNewPin};
use crate::ctap2::commands::get_assertion::{GetAssertion, GetAssertionResult};
use crate::ctap2::commands::make_credentials::{MakeCredentials, MakeCredentialsResult};
use crate::ctap2::commands::reset::Reset;
use crate::ctap2::commands::{
    repackage_pin_errors, CommandError, PinAuthCommand, Request, StatusCode,
};
use crate::ctap2::server::{RelyingParty, RelyingPartyWrapper};
use crate::errors::{self, AuthenticatorError, UnsupportedOption};
use crate::statecallback::StateCallback;
use crate::transport::device_selector::{
    BlinkResult, Device, DeviceBuildParameters, DeviceCommand, DeviceSelectorEvent,
};
use crate::transport::platform::transaction::Transaction;
use crate::transport::{errors::HIDError, hid::HIDDevice, FidoDevice, Nonce};
use crate::u2fprotocol::{u2f_init_device, u2f_is_keyhandle_valid, u2f_register, u2f_sign};
use crate::u2ftypes::U2FDevice;
use crate::{send_status, RegisterResult, SignResult, StatusUpdate};
use std::sync::mpsc::{channel, Sender};
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

    pub fn register(
        &mut self,
        flags: crate::RegisterFlags,
        timeout: u64,
        challenge: Vec<u8>,
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

                send_status(
                    &status,
                    crate::StatusUpdate::DeviceAvailable {
                        dev_info: dev.get_device_info(),
                    },
                );

                // Iterate the exclude list and see if there are any matches.
                // If so, we'll keep polling the device anyway to test for user
                // consent, to be consistent with CTAP2 device behavior.
                let excluded = key_handles.iter().any(|key_handle| {
                    is_valid_transport(key_handle.transports)
                        && u2f_is_keyhandle_valid(
                            dev,
                            &challenge,
                            &application,
                            &key_handle.credential,
                        )
                        .unwrap_or(false) /* no match on failure */
                });

                while alive() {
                    if excluded {
                        let blank = vec![0u8; PARAMETER_SIZE];
                        if u2f_register(dev, &blank, &blank).is_ok() {
                            callback.call(Err(errors::AuthenticatorError::U2FToken(
                                errors::U2FTokenError::InvalidState,
                            )));
                            break;
                        }
                    } else if let Ok(bytes) = u2f_register(dev, &challenge, &application) {
                        let dev_info = dev.get_device_info();
                        send_status(
                            &status,
                            crate::StatusUpdate::Success {
                                dev_info: dev.get_device_info(),
                            },
                        );
                        callback.call(Ok(RegisterResult::CTAP1(bytes, dev_info)));
                        break;
                    }

                    // Sleep a bit before trying again.
                    thread::sleep(Duration::from_millis(100));
                }

                send_status(
                    &status,
                    crate::StatusUpdate::DeviceUnavailable {
                        dev_info: dev.get_device_info(),
                    },
                );
            },
        );

        self.transaction = Some(try_or!(transaction, |e| cbc.call(Err(e))));
    }

    pub fn sign(
        &mut self,
        flags: crate::SignFlags,
        timeout: u64,
        challenge: Vec<u8>,
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
                        u2f_is_keyhandle_valid(dev, &challenge, app_id, &key_handle.credential)
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

                send_status(
                    &status,
                    crate::StatusUpdate::DeviceAvailable {
                        dev_info: dev.get_device_info(),
                    },
                );

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
                                u2f_sign(dev, &challenge, app_id, &key_handle.credential)
                            {
                                let dev_info = dev.get_device_info();
                                send_status(
                                    &status,
                                    crate::StatusUpdate::Success {
                                        dev_info: dev.get_device_info(),
                                    },
                                );
                                callback.call(Ok(SignResult::CTAP1(
                                    app_id.clone(),
                                    key_handle.credential.clone(),
                                    bytes,
                                    dev_info,
                                )));
                                break 'outer;
                            }
                        }
                    }

                    // Sleep a bit before trying again.
                    thread::sleep(Duration::from_millis(100));
                }

                send_status(
                    &status,
                    crate::StatusUpdate::DeviceUnavailable {
                        dev_info: dev.get_device_info(),
                    },
                );
            },
        );

        self.transaction = Some(try_or!(transaction, |e| cbc.call(Err(e))));
    }

    // This blocks.
    pub fn cancel(&mut self) {
        if let Some(mut transaction) = self.transaction.take() {
            transaction.cancel();
        }
    }
}

#[derive(Default)]
// TODO(MS): To be renamed to `StateMachine` once U2FManager and the original StateMachine can be removed.
pub struct StateMachineCtap2 {
    transaction: Option<Transaction>,
}

impl StateMachineCtap2 {
    pub fn new() -> Self {
        Default::default()
    }

    fn init_and_select(
        info: DeviceBuildParameters,
        selector: &Sender<DeviceSelectorEvent>,
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
        if let Err(e) = dev.init(Nonce::CreateRandom) {
            warn!("error while initializing device: {}", e);
            selector.send(DeviceSelectorEvent::NotAToken(dev.id())).ok();
            return None;
        }

        if ctap2_only && dev.get_authenticator_info().is_none() {
            info!("Device does not support CTAP2");
            selector.send(DeviceSelectorEvent::NotAToken(dev.id())).ok();
            return None;
        }

        let write_only_clone = match dev.clone_device_as_write_only() {
            Ok(x) => x,
            Err(_) => {
                // There is probably something seriously wrong here, if this happens.
                // So `NotAToken()` is probably too weak a response here.
                warn!("error while cloning device: {:?}", dev.id());
                selector
                    .send(DeviceSelectorEvent::NotAToken(dev.id()))
                    .ok()?;
                return None;
            }
        };
        let (tx, rx) = channel();
        selector
            .send(DeviceSelectorEvent::ImAToken((write_only_clone, tx)))
            .ok()?;

        // We can be cancelled from the user (through keep_alive()) or from the device selector
        // (through a DeviceCommand::Cancel on rx).  We'll combine those signals into a single
        // predicate to pass to Device::block_and_blink.
        let keep_blinking = || {
            keep_alive() && !matches!(rx.try_recv(), Ok(DeviceCommand::Cancel))
        };

        // Blocking recv. DeviceSelector will tell us what to do
        match rx.recv() {
            Ok(DeviceCommand::Blink) => match dev.block_and_blink(&keep_blinking) {
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
            },
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

    fn ask_user_for_pin<U>(
        error: PinError,
        status: &Sender<StatusUpdate>,
        callback: &StateCallback<crate::Result<U>>,
    ) -> Result<Pin, ()> {
        info!("PIN Error that requires user interaction detected. Sending it back and waiting for a reply");
        let (tx, rx) = channel();
        send_status(status, crate::StatusUpdate::PinError(error.clone(), tx));
        match rx.recv() {
            Ok(pin) => Ok(pin),
            Err(_) => {
                // recv() can only fail, if the other side is dropping the Sender. We are using this as a trick
                // to let the callback decide if this PinError is recoverable (e.g. with User input) or not (e.g.
                // locked token). If it is deemed unrecoverable, we error out the 'normal' way with the same error.
                error!("Callback dropped the channel, so we forward the error to the results-callback: {:?}", error);
                callback.call(Err(AuthenticatorError::PinError(error)));
                Err(())
            }
        }
    }

    fn determine_pin_auth<T: PinAuthCommand, U>(
        cmd: &mut T,
        dev: &mut Device,
        status: &Sender<StatusUpdate>,
        callback: &StateCallback<crate::Result<U>>,
    ) -> Result<(), ()> {
        loop {
            match cmd.determine_pin_auth(dev) {
                Ok(_) => {
                    break;
                }
                Err(AuthenticatorError::PinError(e)) => {
                    let pin = Self::ask_user_for_pin(e, status, callback)?;
                    cmd.set_pin(Some(pin));
                    continue;
                }
                Err(e) => {
                    error!("Error when determining pinAuth: {:?}", e);
                    callback.call(Err(e));
                    return Err(());
                }
            };
        }

        // CTAP 2.0 spec is a bit vague here, but CTAP 2.1 is very specific, that the request
        // should either include pinAuth OR uv=true, but not both at the same time.
        // Do not set user_verification, if pinAuth is provided
        if cmd.pin_auth().is_some() {
            cmd.unset_uv_option();
        }

        Ok(())
    }

    pub fn register(
        &mut self,
        timeout: u64,
        params: MakeCredentials,
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
                let mut dev = match Self::init_and_select(info, &selector, false, alive) {
                    None => {
                        return;
                    }
                    Some(dev) => dev,
                };

                info!("Device {:?} continues with the register process", dev.id());
                // TODO(baloo): not sure about this, have to ask
                // We currently support none of the authenticator selection
                // criteria because we can't ask tokens whether they do support
                // those features. If flags are set, ignore all tokens for now.
                //
                // Technically, this is a ConstraintError because we shouldn't talk
                // to this authenticator in the first place. But the result is the
                // same anyway.
                //if !flags.is_empty() {
                //    return;
                //}

                // TODO(baloo): not sure about this, have to ask
                // Iterate the exclude list and see if there are any matches.
                // If so, we'll keep polling the device anyway to test for user
                // consent, to be consistent with CTAP2 device behavior.
                //let excluded = key_handles.iter().any(|key_handle| {
                //    is_valid_transport(key_handle.transports)
                //        && u2f_is_keyhandle_valid(dev, &challenge, &application, &key_handle.credential)
                //            .unwrap_or(false) /* no match on failure */
                //});

                // TODO(MS): This is wasteful, but the current setup with read only-functions doesn't allow me
                //           to modify "params" directly.
                let mut makecred = params.clone();
                if params.is_ctap2_request() {
                    // First check if extensions have been requested that are not supported by the device
                    if let Some(true) = params.extensions.hmac_secret {
                        if let Some(auth) = dev.get_authenticator_info() {
                            if !auth.supports_hmac_secret() {
                                callback.call(Err(AuthenticatorError::UnsupportedOption(
                                    UnsupportedOption::HmacSecret,
                                )));
                                return;
                            }
                        }
                    }

                    // Second, ask for PIN and get the shared secret
                    if Self::determine_pin_auth(&mut makecred, &mut dev, &status, &callback)
                        .is_err()
                    {
                        return;
                    }
                }
                debug!("------------------------------------------------------------------");
                debug!("{:?}", makecred);
                debug!("------------------------------------------------------------------");
                let resp = dev.send_msg_cancellable(&makecred, alive);
                if resp.is_ok() {
                    send_status(
                        &status,
                        crate::StatusUpdate::Success {
                            dev_info: dev.get_device_info(),
                        },
                    );
                    // The DeviceSelector could already be dead, but it might also wait
                    // for us to respond, in order to cancel all other tokens in case
                    // we skipped the "blinking"-action and went straight for the actual
                    // request.
                    let _ = selector.send(DeviceSelectorEvent::SelectedToken(dev.id()));
                }
                match resp {
                    Ok(MakeCredentialsResult::CTAP2(attestation, client_data)) => {
                        callback.call(Ok(RegisterResult::CTAP2(attestation, client_data)))
                    }
                    Ok(MakeCredentialsResult::CTAP1(data)) => {
                        callback.call(Ok(RegisterResult::CTAP1(data, dev.get_device_info())))
                    }

                    Err(HIDError::Command(CommandError::StatusCode(
                        StatusCode::ChannelBusy,
                        _,
                    ))) => {}
                    Err(e) => {
                        warn!("error happened: {}", e);
                        callback.call(Err(AuthenticatorError::HIDError(e)));
                    }
                }
            },
        );

        self.transaction = Some(try_or!(transaction, |e| cbc.call(Err(e))));
    }

    pub fn sign(
        &mut self,
        timeout: u64,
        params: GetAssertion,
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
                let mut dev = match Self::init_and_select(info, &selector, false, alive) {
                    None => {
                        return;
                    }
                    Some(dev) => dev,
                };

                info!("Device {:?} continues with the signing process", dev.id());
                // TODO(MS): This is wasteful, but the current setup with read only-functions doesn't allow me
                //           to modify "params" directly.
                let mut getassertion = params.clone();
                if params.is_ctap2_request() {
                    // First check if extensions have been requested that are not supported by the device
                    if params.extensions.hmac_secret.is_some() {
                        if let Some(auth) = dev.get_authenticator_info() {
                            if !auth.supports_hmac_secret() {
                                callback.call(Err(AuthenticatorError::UnsupportedOption(
                                    UnsupportedOption::HmacSecret,
                                )));
                                return;
                            }
                        }
                    }

                    // Second, ask for PIN and get the shared secret
                    if Self::determine_pin_auth(&mut getassertion, &mut dev, &status, &callback)
                        .is_err()
                    {
                        return;
                    }

                    // Third, use the shared secret in the extensions, if requested
                    if let Some(extension) = getassertion.extensions.hmac_secret.as_mut() {
                        if let Some(secret) = dev.get_shared_secret() {
                            match extension.calculate(secret) {
                                Ok(x) => x,
                                Err(e) => {
                                    callback.call(Err(e));
                                    return;
                                }
                            }
                        }
                    }
                }

                debug!("------------------------------------------------------------------");
                debug!("{:?}", getassertion);
                debug!("------------------------------------------------------------------");

                let mut resp = dev.send_msg_cancellable(&getassertion, alive);
                if resp.is_err() {
                    // Retry with a different RP ID if one was supplied. This is intended to be
                    // used with the AppID provided in the WebAuthn FIDO AppID extension.
                    if let Some(alternate_rp_id) = getassertion.alternate_rp_id {
                        getassertion.rp = RelyingPartyWrapper::Data(RelyingParty {
                            id: alternate_rp_id,
                            ..Default::default()
                        });
                        getassertion.alternate_rp_id = None;
                        resp = dev.send_msg_cancellable(&getassertion, alive);
                    }
                }
                if resp.is_ok() {
                    send_status(
                        &status,
                        crate::StatusUpdate::Success {
                            dev_info: dev.get_device_info(),
                        },
                    );
                    // The DeviceSelector could already be dead, but it might also wait
                    // for us to respond, in order to cancel all other tokens in case
                    // we skipped the "blinking"-action and went straight for the actual
                    // request.
                    let _ = selector.send(DeviceSelectorEvent::SelectedToken(dev.id()));
                }
                match resp {
                    Ok(GetAssertionResult::CTAP1(resp)) => {
                        let app_id = getassertion.rp.hash().as_ref().to_vec();
                        let key_handle = getassertion.allow_list[0].id.clone();

                        callback.call(Ok(SignResult::CTAP1(
                            app_id,
                            key_handle,
                            resp,
                            dev.get_device_info(),
                        )))
                    }
                    Ok(GetAssertionResult::CTAP2(assertion, client_data)) => {
                        callback.call(Ok(SignResult::CTAP2(assertion, client_data)))
                    }
                    Err(HIDError::Command(CommandError::StatusCode(
                        StatusCode::ChannelBusy,
                        _,
                    ))) => {}
                    Err(e) => {
                        warn!("error happened: {}", e);
                        callback.call(Err(AuthenticatorError::HIDError(e)));
                    }
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
                let reset = Reset {};
                let mut dev = match Self::init_and_select(info, &selector, true, alive) {
                    None => {
                        return;
                    }
                    Some(dev) => dev,
                };

                info!("Device {:?} continues with the reset process", dev.id());
                debug!("------------------------------------------------------------------");
                debug!("{:?}", reset);
                debug!("------------------------------------------------------------------");

                let resp = dev.send_cbor_cancellable(&reset, alive);
                if resp.is_ok() {
                    send_status(
                        &status,
                        crate::StatusUpdate::Success {
                            dev_info: dev.get_device_info(),
                        },
                    );
                    // The DeviceSelector could already be dead, but it might also wait
                    // for us to respond, in order to cancel all other tokens in case
                    // we skipped the "blinking"-action and went straight for the actual
                    // request.
                    let _ = selector.send(DeviceSelectorEvent::SelectedToken(dev.id()));
                }

                match resp {
                    Ok(()) => callback.call(Ok(())),
                    Err(HIDError::DeviceNotSupported) | Err(HIDError::UnsupportedCommand) => {}
                    Err(HIDError::Command(CommandError::StatusCode(
                        StatusCode::ChannelBusy,
                        _,
                    ))) => {}
                    Err(e) => {
                        warn!("error happened: {}", e);
                        callback.call(Err(AuthenticatorError::HIDError(e)));
                    }
                }
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
                let mut dev = match Self::init_and_select(info, &selector, true, alive) {
                    None => {
                        return;
                    }
                    Some(dev) => dev,
                };

                let (mut shared_secret, authinfo) = match dev.establish_shared_secret() {
                    Ok(s) => s,
                    Err(e) => {
                        callback.call(Err(AuthenticatorError::HIDError(e)));
                        return;
                    }
                };

                // With CTAP2.1 we will have an adjustable required length for PINs
                if new_pin.as_bytes().len() < 4 {
                    callback.call(Err(AuthenticatorError::PinError(PinError::PinIsTooShort)));
                    return;
                }

                if new_pin.as_bytes().len() > 64 {
                    callback.call(Err(AuthenticatorError::PinError(PinError::PinIsTooLong(
                        new_pin.as_bytes().len(),
                    ))));
                    return;
                }

                // Check if a client-pin is already set, or if a new one should be created
                let res = if authinfo.options.client_pin.unwrap_or_default() {
                    let mut res;
                    let mut error = PinError::PinRequired;
                    loop {
                        let current_pin = match Self::ask_user_for_pin(error, &status, &callback) {
                            Ok(pin) => pin,
                            _ => {
                                return;
                            }
                        };

                        res = ChangeExistingPin::new(
                            &authinfo,
                            &shared_secret,
                            &current_pin,
                            &new_pin,
                        )
                        .map_err(HIDError::Command)
                        .and_then(|msg| dev.send_cbor_cancellable(&msg, alive))
                        .map_err(AuthenticatorError::HIDError)
                        .map_err(|e| repackage_pin_errors(&mut dev, e));

                        if let Err(AuthenticatorError::PinError(e)) = res {
                            error = e;
                            // We need to re-establish the shared secret for the next round.
                            match dev.establish_shared_secret() {
                                Ok((s, _)) => {
                                    shared_secret = s;
                                }
                                Err(e) => {
                                    callback.call(Err(AuthenticatorError::HIDError(e)));
                                    return;
                                }
                            };

                            continue;
                        } else {
                            break;
                        }
                    }
                    res
                } else {
                    dev.send_cbor_cancellable(&SetNewPin::new(&shared_secret, &new_pin), alive)
                        .map_err(AuthenticatorError::HIDError)
                };
                callback.call(res);
            },
        );
        self.transaction = Some(try_or!(transaction, move |e| cbc.call(Err(e))));
    }
}
