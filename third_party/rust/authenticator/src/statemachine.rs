/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::consts::PARAMETER_SIZE;
use crate::ctap2::client_data::ClientDataHash;
use crate::ctap2::commands::client_pin::{
    ChangeExistingPin, Pin, PinError, PinUvAuthTokenPermission, SetNewPin,
};
use crate::ctap2::commands::get_assertion::{GetAssertion, GetAssertionResult};
use crate::ctap2::commands::make_credentials::{MakeCredentials, MakeCredentialsResult};
use crate::ctap2::commands::reset::Reset;
use crate::ctap2::commands::{
    repackage_pin_errors, CommandError, PinUvAuthCommand, PinUvAuthResult, Request, StatusCode,
};
use crate::ctap2::server::{
    PublicKeyCredentialDescriptor, RelyingParty, RelyingPartyWrapper, RpIdHash,
};
use crate::errors::{self, AuthenticatorError, UnsupportedOption};
use crate::statecallback::StateCallback;
use crate::transport::device_selector::{
    BlinkResult, Device, DeviceBuildParameters, DeviceCommand, DeviceSelectorEvent,
};
use crate::transport::platform::transaction::Transaction;
use crate::transport::{errors::HIDError, hid::HIDDevice, FidoDevice, Nonce};
use crate::u2fprotocol::{u2f_init_device, u2f_is_keyhandle_valid, u2f_register, u2f_sign};
use crate::u2ftypes::U2FDevice;
use crate::{
    send_status, AuthenticatorTransports, KeyHandle, RegisterFlags, RegisterResult, SignFlags,
    SignResult, StatusPinUv, StatusUpdate,
};
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
        let keep_blinking = || keep_alive() && !matches!(rx.try_recv(), Ok(DeviceCommand::Cancel));

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
        was_invalid: bool,
        retries: Option<u8>,
        status: &Sender<StatusUpdate>,
        callback: &StateCallback<crate::Result<U>>,
    ) -> Result<Pin, ()> {
        info!("PIN Error that requires user interaction detected. Sending it back and waiting for a reply");
        let (tx, rx) = channel();
        if was_invalid {
            send_status(
                status,
                crate::StatusUpdate::PinUvError(StatusPinUv::InvalidPin(tx, retries)),
            );
        } else {
            send_status(
                status,
                crate::StatusUpdate::PinUvError(StatusPinUv::PinRequired(tx)),
            );
        }
        match rx.recv() {
            Ok(pin) => Ok(pin),
            Err(_) => {
                // recv() can only fail, if the other side is dropping the Sender.
                error!("Callback dropped the channel. Aborting.");
                callback.call(Err(AuthenticatorError::CancelledByUser));
                Err(())
            }
        }
    }

    /// Try to fetch PinUvAuthToken from the device and derive from it PinUvAuthParam.
    /// Prefer UV, fallback to PIN.
    /// Prefer newer pinUvAuth-methods, if supported by the device.
    fn get_pin_uv_auth_param<T: PinUvAuthCommand + Request<V>, V>(
        cmd: &mut T,
        dev: &mut Device,
        permission: PinUvAuthTokenPermission,
        skip_uv: bool,
    ) -> Result<PinUvAuthResult, AuthenticatorError> {
        let info = dev
            .get_authenticator_info()
            .ok_or(AuthenticatorError::HIDError(HIDError::DeviceNotInitialized))?;

        // Only use UV, if the device supports it and we don't skip it
        // which happens as a fallback, if UV-usage failed too many times
        // Note: In theory, we could also repeatedly query GetInfo here and check
        //       if uv is set to Some(true), as tokens should set it to Some(false)
        //       if UV is blocked (too many failed attempts). But the CTAP2.0-spec is
        //       vague and I don't trust all tokens to implement it that way. So we
        //       keep track of it ourselves, using `skip_uv`.
        let supports_uv = info.options.user_verification == Some(true);
        let supports_pin = info.options.client_pin.is_some();
        let pin_configured = info.options.client_pin == Some(true);

        // Check if the combination of device-protection and request-options
        // are allowing for 'discouraged', meaning no auth required.
        if cmd.can_skip_user_verification(info) {
            return Ok(PinUvAuthResult::NoAuthRequired);
        }

        // Device does not support any auth-method
        if !pin_configured && !supports_uv {
            // We'll send it to the device anyways, and let it error out (or magically work)
            return Ok(PinUvAuthResult::NoAuthTypeSupported);
        }

        let (res, pin_auth_token) = if info.options.pin_uv_auth_token == Some(true) {
            if !skip_uv && supports_uv {
                // CTAP 2.1 - UV
                let pin_auth_token = dev
                    .get_pin_uv_auth_token_using_uv_with_permissions(permission, cmd.get_rp_id());
                (
                    PinUvAuthResult::SuccessGetPinUvAuthTokenUsingUvWithPermissions,
                    pin_auth_token,
                )
            } else if supports_pin && pin_configured {
                // CTAP 2.1 - PIN
                let pin_auth_token = dev.get_pin_uv_auth_token_using_pin_with_permissions(
                    cmd.pin(),
                    permission,
                    cmd.get_rp_id(),
                );
                (
                    PinUvAuthResult::SuccessGetPinUvAuthTokenUsingPinWithPermissions,
                    pin_auth_token,
                )
            } else {
                return Ok(PinUvAuthResult::NoAuthTypeSupported);
            }
        } else {
            // CTAP 2.0 fallback
            if !skip_uv && supports_uv && cmd.pin().is_none() {
                // If the device supports internal user-verification (e.g. fingerprints),
                // skip PIN-stuff

                // We may need the shared secret for HMAC-extension, so we
                // have to establish one
                if info.supports_hmac_secret() {
                    let _shared_secret = dev.establish_shared_secret()?;
                }
                return Ok(PinUvAuthResult::UsingInternalUv);
            }

            let pin_auth_token = dev.get_pin_token(cmd.pin());
            (PinUvAuthResult::SuccessGetPinToken, pin_auth_token)
        };

        let pin_auth_token = pin_auth_token.map_err(|e| repackage_pin_errors(dev, e))?;
        cmd.set_pin_uv_auth_param(Some(pin_auth_token))?;
        // CTAP 2.0 spec is a bit vague here, but CTAP 2.1 is very specific, that the request
        // should either include pinAuth OR uv=true, but not both at the same time.
        // Do not set user_verification, if pinAuth is provided
        cmd.set_uv_option(None);
        Ok(res)
    }

    /// PUAP, as per spec: PinUvAuthParam
    /// Determines, if we need to establish a PinUvAuthParam, based on the
    /// capabilities of the device and the incoming request.
    /// If it is needed, tries to establish one and save it inside the Request.
    /// Returns Ok() if we can proceed with sending the actual Request to
    /// the device, Err() otherwise.
    /// Handles asking the user for a PIN, if needed and sending StatusUpdates
    /// regarding PIN and UV usage.
    fn determine_puap_if_needed<T: PinUvAuthCommand + Request<V>, U, V>(
        cmd: &mut T,
        dev: &mut Device,
        mut skip_uv: bool,
        permission: PinUvAuthTokenPermission,
        status: &Sender<StatusUpdate>,
        callback: &StateCallback<crate::Result<U>>,
        alive: &dyn Fn() -> bool,
    ) -> Result<PinUvAuthResult, ()> {
        // Starting from a blank slate.
        cmd.set_pin_uv_auth_param(None).map_err(|_| ())?;

        // CTAP1/U2F-only devices do not support PinUvAuth, so we skip it
        if !dev.supports_ctap2() {
            return Ok(PinUvAuthResult::DeviceIsCtap1);
        }

        while alive() {
            debug!("-----------------------------------------------------------------");
            debug!("Getting pinUvAuthParam");
            match Self::get_pin_uv_auth_param(cmd, dev, permission, skip_uv) {
                Ok(r) => {
                    return Ok(r);
                }

                Err(AuthenticatorError::PinError(PinError::PinRequired)) => {
                    if let Ok(pin) = Self::ask_user_for_pin(false, None, status, callback) {
                        cmd.set_pin(Some(pin));
                        skip_uv = true;
                        continue;
                    } else {
                        return Err(());
                    }
                }
                Err(AuthenticatorError::PinError(PinError::InvalidPin(retries))) => {
                    if let Ok(pin) = Self::ask_user_for_pin(true, retries, status, callback) {
                        cmd.set_pin(Some(pin));
                        continue;
                    } else {
                        return Err(());
                    }
                }
                Err(AuthenticatorError::PinError(PinError::InvalidUv(retries))) => {
                    if retries == Some(0) {
                        skip_uv = true;
                    }
                    send_status(
                        status,
                        StatusUpdate::PinUvError(StatusPinUv::InvalidUv(retries)),
                    )
                }
                Err(e @ AuthenticatorError::PinError(PinError::PinAuthBlocked)) => {
                    send_status(
                        status,
                        StatusUpdate::PinUvError(StatusPinUv::PinAuthBlocked),
                    );
                    error!("Error when determining pinAuth: {:?}", e);
                    callback.call(Err(e));
                    return Err(());
                }
                Err(e @ AuthenticatorError::PinError(PinError::PinBlocked)) => {
                    send_status(status, StatusUpdate::PinUvError(StatusPinUv::PinBlocked));
                    error!("Error when determining pinAuth: {:?}", e);
                    callback.call(Err(e));
                    return Err(());
                }
                Err(e @ AuthenticatorError::PinError(PinError::PinNotSet)) => {
                    send_status(status, StatusUpdate::PinUvError(StatusPinUv::PinNotSet));
                    error!("Error when determining pinAuth: {:?}", e);
                    callback.call(Err(e));
                    return Err(());
                }
                Err(AuthenticatorError::PinError(PinError::UvBlocked)) => {
                    skip_uv = true;
                    send_status(status, StatusUpdate::PinUvError(StatusPinUv::UvBlocked))
                }
                // Used for CTAP2.0 UV (fingerprints)
                Err(AuthenticatorError::PinError(PinError::PinAuthInvalid)) => {
                    skip_uv = true;
                    send_status(
                        status,
                        StatusUpdate::PinUvError(StatusPinUv::InvalidUv(None)),
                    )
                }
                Err(e) => {
                    error!("Error when determining pinAuth: {:?}", e);
                    callback.call(Err(e));
                    return Err(());
                }
            }
        }
        Err(())
    }

    pub fn register(
        &mut self,
        timeout: u64,
        params: MakeCredentials,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    ) {
        if params.use_ctap1_fallback {
            /* Firefox uses this when security.webauthn.ctap2 is false. */
            let mut flags = RegisterFlags::empty();
            if params.options.resident_key == Some(true) {
                flags |= RegisterFlags::REQUIRE_RESIDENT_KEY;
            }
            if params.options.user_verification == Some(true) {
                flags |= RegisterFlags::REQUIRE_USER_VERIFICATION;
            }
            let application = params.rp.hash().0.to_vec();
            let key_handles = params
                .exclude_list
                .iter()
                .map(|cred_desc| KeyHandle {
                    credential: cred_desc.id.clone(),
                    transports: AuthenticatorTransports::empty(),
                })
                .collect();
            let challenge = params.client_data_hash;

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

                // TODO(MS): This is wasteful, but the current setup with read only-functions doesn't allow me
                //           to modify "params" directly.
                let mut makecred = params.clone();
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

                let mut skip_uv = false;
                while alive() {
                    let pin_uv_auth_result = match Self::determine_puap_if_needed(
                        &mut makecred,
                        &mut dev,
                        skip_uv,
                        PinUvAuthTokenPermission::MakeCredential,
                        &status,
                        &callback,
                        alive,
                    ) {
                        Ok(r) => r,
                        Err(()) => {
                            return;
                        }
                    };

                    debug!("------------------------------------------------------------------");
                    debug!("{makecred:?} using {pin_uv_auth_result:?}");
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
                        Ok(MakeCredentialsResult(attestation)) => {
                            callback.call(Ok(RegisterResult::CTAP2(attestation)));
                            break;
                        }
                        Err(HIDError::Command(CommandError::StatusCode(
                            StatusCode::ChannelBusy,
                            _,
                        ))) => {
                            // Channel busy. Client SHOULD retry the request after a short delay.
                            thread::sleep(Duration::from_millis(100));
                            continue;
                        }
                        Err(HIDError::Command(CommandError::StatusCode(
                            StatusCode::PinAuthInvalid,
                            _,
                        ))) if pin_uv_auth_result == PinUvAuthResult::UsingInternalUv => {
                            // This should only happen for CTAP2.0 tokens that use internal UV and
                            // failed (e.g. wrong fingerprint used), while doing MakeCredentials
                            send_status(
                                &status,
                                StatusUpdate::PinUvError(StatusPinUv::InvalidUv(None)),
                            );
                            continue;
                        }
                        Err(HIDError::Command(CommandError::StatusCode(
                            StatusCode::PinRequired,
                            _,
                        ))) if pin_uv_auth_result == PinUvAuthResult::UsingInternalUv => {
                            // This should only happen for CTAP2.0 tokens that use internal UV and failed
                            // repeatedly, so that we have to fall back to PINs
                            skip_uv = true;
                            continue;
                        }
                        Err(HIDError::Command(CommandError::StatusCode(
                            StatusCode::UvBlocked,
                            _,
                        ))) if pin_uv_auth_result
                            == PinUvAuthResult::SuccessGetPinUvAuthTokenUsingUvWithPermissions =>
                        {
                            // This should only happen for CTAP2.1 tokens that use internal UV and failed
                            // repeatedly, so that we have to fall back to PINs
                            skip_uv = true;
                            continue;
                        }
                        Err(e) => {
                            warn!("error happened: {e}");
                            callback.call(Err(AuthenticatorError::HIDError(e)));
                            break;
                        }
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
        if params.use_ctap1_fallback {
            /* Firefox uses this when security.webauthn.ctap2 is false. */
            let flags = match params.options.user_verification {
                Some(true) => SignFlags::REQUIRE_USER_VERIFICATION,
                _ => SignFlags::empty(),
            };
            let mut app_ids = vec![params.rp.hash().0.to_vec()];
            if let Some(app_id) = params.alternate_rp_id {
                app_ids.push(
                    RelyingPartyWrapper::Data(RelyingParty {
                        id: app_id,
                        ..Default::default()
                    })
                    .hash()
                    .0
                    .to_vec(),
                );
            }
            let key_handles = params
                .allow_list
                .iter()
                .map(|cred_desc| KeyHandle {
                    credential: cred_desc.id.clone(),
                    transports: AuthenticatorTransports::empty(),
                })
                .collect();
            let challenge = params.client_data_hash;

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
                let mut skip_uv = false;
                while alive() {
                    let pin_uv_auth_result = match Self::determine_puap_if_needed(
                        &mut getassertion,
                        &mut dev,
                        skip_uv,
                        PinUvAuthTokenPermission::GetAssertion,
                        &status,
                        &callback,
                        alive,
                    ) {
                        Ok(r) => r,
                        Err(()) => {
                            return;
                        }
                    };

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

                    debug!("------------------------------------------------------------------");
                    debug!("{getassertion:?} using {pin_uv_auth_result:?}");
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
                        Ok(GetAssertionResult(assertion)) => {
                            callback.call(Ok(SignResult::CTAP2(assertion)));
                            break;
                        }
                        Err(HIDError::Command(CommandError::StatusCode(
                            StatusCode::ChannelBusy,
                            _,
                        ))) => {
                            // Channel busy. Client SHOULD retry the request after a short delay.
                            thread::sleep(Duration::from_millis(100));
                            continue;
                        }
                        Err(HIDError::Command(CommandError::StatusCode(
                            StatusCode::OperationDenied,
                            _,
                        ))) if pin_uv_auth_result == PinUvAuthResult::UsingInternalUv => {
                            // This should only happen for CTAP2.0 tokens that use internal UV and failed
                            // (e.g. wrong fingerprint used), while doing GetAssertion
                            // Yes, this is a different error code than for MakeCredential.
                            send_status(
                                &status,
                                StatusUpdate::PinUvError(StatusPinUv::InvalidUv(None)),
                            );
                            continue;
                        }
                        Err(HIDError::Command(CommandError::StatusCode(
                            StatusCode::PinRequired,
                            _,
                        ))) if pin_uv_auth_result == PinUvAuthResult::UsingInternalUv => {
                            // This should only happen for CTAP2.0 tokens that use internal UV and failed
                            // repeatedly, so that we have to fall back to PINs
                            skip_uv = true;
                            continue;
                        }
                        Err(HIDError::Command(CommandError::StatusCode(
                            StatusCode::UvBlocked,
                            _,
                        ))) if pin_uv_auth_result
                            == PinUvAuthResult::SuccessGetPinUvAuthTokenUsingUvWithPermissions =>
                        {
                            // This should only happen for CTAP2.1 tokens that use internal UV and failed
                            // repeatedly, so that we have to fall back to PINs
                            skip_uv = true;
                            continue;
                        }
                        Err(e) => {
                            warn!("error happened: {e}");
                            callback.call(Err(AuthenticatorError::HIDError(e)));
                            break;
                        }
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

                let mut shared_secret = match dev.establish_shared_secret() {
                    Ok(s) => s,
                    Err(e) => {
                        callback.call(Err(AuthenticatorError::HIDError(e)));
                        return;
                    }
                };

                let authinfo = match dev.get_authenticator_info() {
                    Some(i) => i.clone(),
                    None => {
                        callback.call(Err(HIDError::DeviceNotInitialized.into()));
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
                    let mut was_invalid = false;
                    let mut retries = None;
                    loop {
                        let current_pin = match Self::ask_user_for_pin(
                            was_invalid,
                            retries,
                            &status,
                            &callback,
                        ) {
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
                        .map_err(|e| repackage_pin_errors(&mut dev, e));

                        if let Err(AuthenticatorError::PinError(PinError::InvalidPin(r))) = res {
                            was_invalid = true;
                            retries = r;
                            // We need to re-establish the shared secret for the next round.
                            match dev.establish_shared_secret() {
                                Ok(s) => {
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
                            challenge.as_ref(),
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
                        let dev_info = dev.get_device_info();
                        send_status(&status, crate::StatusUpdate::Success { dev_info });
                        callback.call(Ok(RegisterResult::CTAP2(result)));
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
                                    Ok(GetAssertionResult(assertion)) => assertion,
                                    Err(_) => {
                                        callback.call(Err(errors::AuthenticatorError::U2FToken(
                                            errors::U2FTokenError::Unknown,
                                        )));
                                        break 'outer;
                                    }
                                };
                                let dev_info = dev.get_device_info();
                                send_status(&status, crate::StatusUpdate::Success { dev_info });
                                callback.call(Ok(SignResult::CTAP2(result)));
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
}
