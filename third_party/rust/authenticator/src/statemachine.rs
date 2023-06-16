/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::authenticatorservice::{RegisterArgs, SignArgs};
use crate::consts::PARAMETER_SIZE;
use crate::crypto::COSEAlgorithm;
use crate::ctap2::client_data::ClientDataHash;
use crate::ctap2::commands::client_pin::{
    ChangeExistingPin, Pin, PinError, PinUvAuthTokenPermission, SetNewPin,
};
use crate::ctap2::commands::get_assertion::{
    GetAssertion, GetAssertionOptions, GetAssertionResult,
};
use crate::ctap2::commands::make_credentials::{
    dummy_make_credentials_cmd, MakeCredentials, MakeCredentialsOptions, MakeCredentialsResult,
};
use crate::ctap2::commands::reset::Reset;
use crate::ctap2::commands::{
    repackage_pin_errors, CommandError, PinUvAuthCommand, PinUvAuthResult, Request, StatusCode,
};
use crate::ctap2::preflight::{
    do_credential_list_filtering_ctap1, do_credential_list_filtering_ctap2,
};
use crate::ctap2::server::{
    PublicKeyCredentialDescriptor, RelyingParty, RelyingPartyWrapper, ResidentKeyRequirement,
    RpIdHash, UserVerificationRequirement,
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
    send_status, AuthenticatorTransports, InteractiveRequest, KeyHandle, RegisterFlags,
    RegisterResult, SignFlags, SignResult, StatusPinUv, StatusUpdate,
};
use std::sync::mpsc::{channel, RecvError, RecvTimeoutError, Sender};
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

macro_rules! unwrap_result {
    ($item: expr, $callback: expr) => {
        match $item {
            Ok(r) => r,
            Err(e) => {
                $callback.call(Err(e.into()));
                return;
            }
        }
    };
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

        let (tx, rx) = channel();
        selector
            .send(DeviceSelectorEvent::ImAToken((dev.id(), tx)))
            .ok()?;
        send_status(
            status,
            crate::StatusUpdate::DeviceAvailable {
                dev_info: dev.get_device_info(),
            },
        );

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

                        send_status(
                            status,
                            crate::StatusUpdate::DeviceSelected(dev.get_device_info()),
                        );
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
                send_status(
                    status,
                    crate::StatusUpdate::DeviceUnavailable {
                        dev_info: dev.get_device_info(),
                    },
                );
                return None;
            }
            Ok(DeviceCommand::Continue) => {
                // Just continue
                send_status(
                    status,
                    crate::StatusUpdate::DeviceSelected(dev.get_device_info()),
                );
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
            Err(RecvError) => {
                // recv() can only fail, if the other side is dropping the Sender.
                info!("Callback dropped the channel. Aborting.");
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
        uv_req: UserVerificationRequirement,
    ) -> Result<PinUvAuthResult, AuthenticatorError> {
        // CTAP 2.1 is very specific that the request should either include pinUvAuthParam
        // OR uv=true, but not both at the same time. We now have to decide which (if either)
        // to send. We may omit both values. Will never send an explicit uv=false, because
        //  a) this is the default, and
        //  b) some CTAP 2.0 authenticators return UnsupportedOption when uv=false.

        // We ensure both pinUvAuthParam and uv are not set to start.
        cmd.set_pin_uv_auth_param(None)?;
        cmd.set_uv_option(None);

        // CTAP1/U2F-only devices do not support user verification, so we skip it
        let info = match dev.get_authenticator_info() {
            Some(info) => info,
            None => return Ok(PinUvAuthResult::DeviceIsCtap1),
        };

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
        if cmd.can_skip_user_verification(info, uv_req) {
            return Ok(PinUvAuthResult::NoAuthRequired);
        }

        // Device does not support any (remaining) auth-method
        if (skip_uv || !supports_uv) && !supports_pin {
            if supports_uv && uv_req == UserVerificationRequirement::Required {
                // We should always set the uv option in the Required case, but the CTAP 2.1 spec
                // says 'Platforms MUST NOT include the "uv" option key if the authenticator does
                // not support built-in user verification.' This is to work around some CTAP 2.0
                // authenticators which incorrectly error out with CTAP2_ERR_UNSUPPORTED_OPTION
                // when the "uv" option is set. The RP that requested UV will (hopefully) reject our
                // response in the !supports_uv case.
                cmd.set_uv_option(Some(true));
            }
            return Ok(PinUvAuthResult::NoAuthTypeSupported);
        }

        // Device supports PINs, but a PIN is not configured. Signal that we
        // can complete the operation if the user sets a PIN first.
        if (skip_uv || !supports_uv) && !pin_configured {
            return Err(AuthenticatorError::PinError(PinError::PinNotSet));
        }

        if info.options.pin_uv_auth_token == Some(true) {
            if !skip_uv && supports_uv {
                // CTAP 2.1 - UV
                let pin_auth_token = dev
                    .get_pin_uv_auth_token_using_uv_with_permissions(permission, cmd.get_rp().id())
                    .map_err(|e| repackage_pin_errors(dev, e))?;
                cmd.set_pin_uv_auth_param(Some(pin_auth_token.clone()))?;
                Ok(PinUvAuthResult::SuccessGetPinUvAuthTokenUsingUvWithPermissions(pin_auth_token))
            } else {
                // CTAP 2.1 - PIN
                // We did not take the `!skip_uv && supports_uv` branch, so we have
                // `(skip_uv || !supports_uv)`. Moreover we did not exit early in the
                // `(skip_uv || !supports_uv) && !pin_configured` case. So we have
                // `pin_configured`.
                let pin_auth_token = dev
                    .get_pin_uv_auth_token_using_pin_with_permissions(
                        cmd.pin(),
                        permission,
                        cmd.get_rp().id(),
                    )
                    .map_err(|e| repackage_pin_errors(dev, e))?;
                cmd.set_pin_uv_auth_param(Some(pin_auth_token.clone()))?;
                Ok(
                    PinUvAuthResult::SuccessGetPinUvAuthTokenUsingPinWithPermissions(
                        pin_auth_token,
                    ),
                )
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
                // CTAP 2.1, Section 6.1.1, Step 1.1.2.1.2.
                cmd.set_uv_option(Some(true));
                return Ok(PinUvAuthResult::UsingInternalUv);
            }

            let pin_auth_token = dev
                .get_pin_token(cmd.pin())
                .map_err(|e| repackage_pin_errors(dev, e))?;
            cmd.set_pin_uv_auth_param(Some(pin_auth_token.clone()))?;
            Ok(PinUvAuthResult::SuccessGetPinToken(pin_auth_token))
        }
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
        uv_req: UserVerificationRequirement,
        status: &Sender<StatusUpdate>,
        callback: &StateCallback<crate::Result<U>>,
        alive: &dyn Fn() -> bool,
    ) -> Result<PinUvAuthResult, ()> {
        while alive() {
            debug!("-----------------------------------------------------------------");
            debug!("Getting pinUvAuthParam");
            match Self::get_pin_uv_auth_param(cmd, dev, permission, skip_uv, uv_req) {
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

                // We need a copy of the arguments for this device
                let args = args.clone();

                let mut options = MakeCredentialsOptions::default();

                if let Some(info) = dev.get_authenticator_info() {
                    // Check if extensions have been requested that are not supported by the device
                    if let Some(true) = args.extensions.hmac_secret {
                        if !info.supports_hmac_secret() {
                            callback.call(Err(AuthenticatorError::UnsupportedOption(
                                UnsupportedOption::HmacSecret,
                            )));
                            return;
                        }
                    }

                    // Set options based on the arguments and the device info.
                    // The user verification option will be set in `determine_puap_if_needed`.
                    options.resident_key = match args.resident_key_req {
                        ResidentKeyRequirement::Required => Some(true),
                        ResidentKeyRequirement::Preferred => {
                            // Use a resident key if the authenticator supports it
                            Some(info.options.resident_key)
                        }
                        ResidentKeyRequirement::Discouraged => Some(false),
                    }
                } else {
                    // Check that the request can be processed by a CTAP1 device.
                    // See CTAP 2.1 Section 10.2. Some additional checks are performed in
                    // MakeCredentials::RequestCtap1
                    if args.resident_key_req == ResidentKeyRequirement::Required {
                        callback.call(Err(AuthenticatorError::UnsupportedOption(
                            UnsupportedOption::ResidentKey,
                        )));
                        return;
                    }
                    if args.user_verification_req == UserVerificationRequirement::Required {
                        callback.call(Err(AuthenticatorError::UnsupportedOption(
                            UnsupportedOption::UserVerification,
                        )));
                        return;
                    }
                    if !args
                        .pub_cred_params
                        .iter()
                        .any(|x| x.alg == COSEAlgorithm::ES256)
                    {
                        callback.call(Err(AuthenticatorError::UnsupportedOption(
                            UnsupportedOption::PubCredParams,
                        )));
                        return;
                    }
                }

                let mut makecred = MakeCredentials::new(
                    ClientDataHash(args.client_data_hash),
                    RelyingPartyWrapper::Data(args.relying_party),
                    Some(args.user),
                    args.pub_cred_params,
                    args.exclude_list,
                    options,
                    args.extensions,
                    args.pin,
                );

                let mut skip_uv = false;
                while alive() {
                    // Requesting both because pre-flighting (credential list filtering)
                    // can potentially send GetAssertion-commands
                    let permissions = PinUvAuthTokenPermission::MakeCredential
                        | PinUvAuthTokenPermission::GetAssertion;

                    let pin_uv_auth_result = match Self::determine_puap_if_needed(
                        &mut makecred,
                        &mut dev,
                        skip_uv,
                        permissions,
                        args.user_verification_req,
                        &status,
                        &callback,
                        alive,
                    ) {
                        Ok(r) => r,
                        Err(()) => {
                            break;
                        }
                    };

                    // Do "pre-flight": Filter the exclude-list
                    if dev.get_authenticator_info().is_some() {
                        makecred.exclude_list = unwrap_result!(
                            do_credential_list_filtering_ctap2(
                                &mut dev,
                                &makecred.exclude_list,
                                &makecred.rp,
                                pin_uv_auth_result.get_pin_uv_auth_token(),
                            ),
                            callback
                        );
                    } else {
                        let key_handle = do_credential_list_filtering_ctap1(
                            &mut dev,
                            &makecred.exclude_list,
                            &makecred.rp,
                            &makecred.client_data_hash,
                        );
                        // That handle was already registered with the token
                        if key_handle.is_some() {
                            // Now we need to send a dummy registration request, to make the token blink
                            // Spec says "dummy appid and invalid challenge". We use the same, as we do for
                            // making the token blink upon device selection.
                            send_status(&status, crate::StatusUpdate::PresenceRequired);
                            let msg = dummy_make_credentials_cmd();
                            let _ = dev.send_msg_cancellable(&msg, alive); // Ignore answer, return "CredentialExcluded"
                            callback.call(Err(HIDError::Command(CommandError::StatusCode(
                                StatusCode::CredentialExcluded,
                                None,
                            ))
                            .into()));
                            return;
                        }
                    }

                    debug!("------------------------------------------------------------------");
                    debug!("{makecred:?} using {pin_uv_auth_result:?}");
                    debug!("------------------------------------------------------------------");
                    send_status(&status, crate::StatusUpdate::PresenceRequired);
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
                        ))) if matches!(pin_uv_auth_result, PinUvAuthResult::UsingInternalUv) => {
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
                        ))) if matches!(pin_uv_auth_result, PinUvAuthResult::UsingInternalUv) => {
                            // This should only happen for CTAP2.0 tokens that use internal UV and failed
                            // repeatedly, so that we have to fall back to PINs
                            skip_uv = true;
                            continue;
                        }
                        Err(HIDError::Command(CommandError::StatusCode(
                            StatusCode::UvBlocked,
                            _,
                        ))) if matches!(
                            pin_uv_auth_result,
                            PinUvAuthResult::SuccessGetPinUvAuthTokenUsingUvWithPermissions(..)
                        ) =>
                        {
                            // This should only happen for CTAP2.1 tokens that use internal UV and failed
                            // repeatedly, so that we have to fall back to PINs
                            skip_uv = true;
                            continue;
                        }
                        Err(HIDError::Command(CommandError::StatusCode(
                            StatusCode::CredentialExcluded,
                            _,
                        ))) => {
                            callback.call(Err(AuthenticatorError::CredentialExcluded));
                            break;
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

                // We need a copy of the arguments for this device
                let args = args.clone();

                if let Some(info) = dev.get_authenticator_info() {
                    // Check if extensions have been requested that are not supported by the device
                    if args.extensions.hmac_secret.is_some() && !info.supports_hmac_secret() {
                        callback.call(Err(AuthenticatorError::UnsupportedOption(
                            UnsupportedOption::HmacSecret,
                        )));
                        return;
                    }
                } else {
                    // Check that the request can be processed by a CTAP1 device.
                    // See CTAP 2.1 Section 10.3. Some additional checks are performed in
                    // GetAssertion::RequestCtap1
                    if args.user_verification_req == UserVerificationRequirement::Required {
                        callback.call(Err(AuthenticatorError::UnsupportedOption(
                            UnsupportedOption::UserVerification,
                        )));
                        return;
                    }
                    if args.allow_list.is_empty() {
                        callback.call(Err(AuthenticatorError::UnsupportedOption(
                            UnsupportedOption::EmptyAllowList,
                        )));
                        return;
                    }
                }

                let mut get_assertion = GetAssertion::new(
                    ClientDataHash(args.client_data_hash),
                    RelyingPartyWrapper::Data(RelyingParty {
                        id: args.relying_party_id,
                        name: None,
                        icon: None,
                    }),
                    args.allow_list,
                    GetAssertionOptions {
                        user_presence: Some(args.user_presence_req),
                        user_verification: None,
                    },
                    args.extensions,
                    args.pin,
                    args.alternate_rp_id,
                );

                let mut skip_uv = false;
                while alive() {
                    let pin_uv_auth_result = match Self::determine_puap_if_needed(
                        &mut get_assertion,
                        &mut dev,
                        skip_uv,
                        PinUvAuthTokenPermission::GetAssertion,
                        args.user_verification_req,
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
                    if let Some(extension) = get_assertion.extensions.hmac_secret.as_mut() {
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

                    // Do "pre-flight": Filter the allow-list
                    let original_allow_list_was_empty = get_assertion.allow_list.is_empty();
                    if dev.get_authenticator_info().is_some() {
                        get_assertion.allow_list = unwrap_result!(
                            do_credential_list_filtering_ctap2(
                                &mut dev,
                                &get_assertion.allow_list,
                                &get_assertion.rp,
                                pin_uv_auth_result.get_pin_uv_auth_token(),
                            ),
                            callback
                        );
                    } else {
                        let key_handle = do_credential_list_filtering_ctap1(
                            &mut dev,
                            &get_assertion.allow_list,
                            &get_assertion.rp,
                            &get_assertion.client_data_hash,
                        );
                        match key_handle {
                            Some(key_handle) => {
                                get_assertion.allow_list = vec![key_handle];
                            }
                            None => {
                                get_assertion.allow_list.clear();
                            }
                        }
                    }

                    // If the incoming list was not empty, but the filtered list is, we have to error out
                    if !original_allow_list_was_empty && get_assertion.allow_list.is_empty() {
                        // We have to collect a user interaction
                        send_status(&status, crate::StatusUpdate::PresenceRequired);
                        let msg = dummy_make_credentials_cmd();
                        let _ = dev.send_msg_cancellable(&msg, alive); // Ignore answer, return "NoCredentials"
                        callback.call(Err(HIDError::Command(CommandError::StatusCode(
                            StatusCode::NoCredentials,
                            None,
                        ))
                        .into()));
                        return;
                    }

                    debug!("------------------------------------------------------------------");
                    debug!("{get_assertion:?} using {pin_uv_auth_result:?}");
                    debug!("------------------------------------------------------------------");
                    send_status(&status, crate::StatusUpdate::PresenceRequired);
                    let mut resp = dev.send_msg_cancellable(&get_assertion, alive);
                    if resp.is_err() {
                        // Retry with a different RP ID if one was supplied. This is intended to be
                        // used with the AppID provided in the WebAuthn FIDO AppID extension.
                        if let Some(alternate_rp_id) = get_assertion.alternate_rp_id {
                            get_assertion.rp = RelyingPartyWrapper::Data(RelyingParty {
                                id: alternate_rp_id,
                                ..Default::default()
                            });
                            get_assertion.alternate_rp_id = None;
                            resp = dev.send_msg_cancellable(&get_assertion, alive);
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
                        Ok(assertions) => {
                            callback.call(Ok(SignResult::CTAP2(assertions)));
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
                        ))) if matches!(pin_uv_auth_result, PinUvAuthResult::UsingInternalUv) => {
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
                        ))) if matches!(pin_uv_auth_result, PinUvAuthResult::UsingInternalUv) => {
                            // This should only happen for CTAP2.0 tokens that use internal UV and failed
                            // repeatedly, so that we have to fall back to PINs
                            skip_uv = true;
                            continue;
                        }
                        Err(HIDError::Command(CommandError::StatusCode(
                            StatusCode::UvBlocked,
                            _,
                        ))) if matches!(
                            pin_uv_auth_result,
                            PinUvAuthResult::SuccessGetPinUvAuthTokenUsingUvWithPermissions(..)
                        ) =>
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

    pub fn reset_helper(
        dev: &mut Device,
        selector: Sender<DeviceSelectorEvent>,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ResetResult>>,
        keep_alive: &dyn Fn() -> bool,
    ) {
        let reset = Reset {};
        info!("Device {:?} continues with the reset process", dev.id());

        debug!("------------------------------------------------------------------");
        debug!("{:?}", reset);
        debug!("------------------------------------------------------------------");
        send_status(&status, crate::StatusUpdate::PresenceRequired);
        let resp = dev.send_cbor_cancellable(&reset, keep_alive);
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
            Err(HIDError::Command(CommandError::StatusCode(StatusCode::ChannelBusy, _))) => {}
            Err(e) => {
                warn!("error happened: {}", e);
                callback.call(Err(AuthenticatorError::HIDError(e)));
            }
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
                Self::reset_helper(&mut dev, selector, status, callback.clone(), alive);
            },
        );

        self.transaction = Some(try_or!(transaction, move |e| cbc.call(Err(e))));
    }

    pub fn set_or_change_pin_helper(
        dev: &mut Device,
        mut current_pin: Option<Pin>,
        new_pin: Pin,
        status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::ResetResult>>,
        alive: &dyn Fn() -> bool,
    ) {
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

        // If the device has a min PIN use that, otherwise default to 4 according to Spec
        if new_pin.as_bytes().len() < authinfo.min_pin_length.unwrap_or(4) as usize {
            callback.call(Err(AuthenticatorError::PinError(PinError::PinIsTooShort)));
            return;
        }

        // As per Spec: "Maximum PIN Length: UTF-8 representation MUST NOT exceed 63 bytes"
        if new_pin.as_bytes().len() >= 64 {
            callback.call(Err(AuthenticatorError::PinError(PinError::PinIsTooLong(
                new_pin.as_bytes().len(),
            ))));
            return;
        }

        // Check if a client-pin is already set, or if a new one should be created
        let res = if Some(true) == authinfo.options.client_pin {
            let mut res;
            let mut was_invalid = false;
            let mut retries = None;
            loop {
                // current_pin will only be Some() in the interactive mode (running `manage()`)
                // In case that PIN is wrong, we want to avoid an endless-loop here with re-trying
                // that wrong PIN all the time. So we `take()` it, and only test it once.
                // If that PIN is wrong, we fall back to the "ask_user_for_pin"-method.
                let curr_pin = match current_pin.take() {
                    None => {
                        match Self::ask_user_for_pin(was_invalid, retries, &status, &callback) {
                            Ok(pin) => pin,
                            _ => {
                                return;
                            }
                        }
                    }
                    Some(pin) => pin,
                };

                res = ChangeExistingPin::new(&authinfo, &shared_secret, &curr_pin, &new_pin)
                    .map_err(HIDError::Command)
                    .and_then(|msg| dev.send_cbor_cancellable(&msg, alive))
                    .map_err(|e| repackage_pin_errors(dev, e));

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

                Self::set_or_change_pin_helper(
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
                        dev.get_device_info(),
                        dev.get_authenticator_info().cloned(),
                    )),
                );
                while alive() {
                    match rx.recv_timeout(Duration::from_millis(400)) {
                        Ok(InteractiveRequest::Reset) => {
                            Self::reset_helper(&mut dev, selector, status, callback.clone(), alive);
                        }
                        Ok(InteractiveRequest::ChangePIN(curr_pin, new_pin)) => {
                            Self::set_or_change_pin_helper(
                                &mut dev,
                                Some(curr_pin),
                                new_pin,
                                status,
                                callback.clone(),
                                alive,
                            );
                        }
                        Ok(InteractiveRequest::SetPIN(pin)) => {
                            Self::set_or_change_pin_helper(
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
