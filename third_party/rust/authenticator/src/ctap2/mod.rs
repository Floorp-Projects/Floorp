pub mod attestation;
pub mod client_data;
#[allow(dead_code)] // TODO(MS): Remove me asap
pub mod commands;
pub mod preflight;
pub mod server;
pub(crate) mod utils;

use crate::authenticatorservice::{RegisterArgs, SignArgs};

use crate::crypto::COSEAlgorithm;

use crate::ctap2::client_data::ClientDataHash;
use crate::ctap2::commands::client_pin::{
    ChangeExistingPin, Pin, PinError, PinUvAuthTokenPermission, SetNewPin,
};
use crate::ctap2::commands::get_assertion::{GetAssertion, GetAssertionOptions};
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
    RelyingParty, RelyingPartyWrapper, ResidentKeyRequirement, UserVerificationRequirement,
};
use crate::errors::{AuthenticatorError, UnsupportedOption};
use crate::statecallback::StateCallback;
use crate::transport::device_selector::{Device, DeviceSelectorEvent};

use crate::status_update::send_status;
use crate::transport::{errors::HIDError, hid::HIDDevice, FidoDevice, FidoDeviceIO, FidoProtocol};

use crate::{RegisterResult, SignResult, StatusPinUv, StatusUpdate};
use std::sync::mpsc::{channel, RecvError, Sender};
use std::thread;
use std::time::Duration;

macro_rules! unwrap_result {
    ($item: expr, $callback: expr) => {
        match $item {
            Ok(r) => r,
            Err(e) => {
                $callback.call(Err(e.into()));
                return false;
            }
        }
    };
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
fn get_pin_uv_auth_param<Dev: FidoDevice, T: PinUvAuthCommand + Request<V>, V>(
    cmd: &mut T,
    dev: &mut Dev,
    permission: PinUvAuthTokenPermission,
    skip_uv: bool,
    uv_req: UserVerificationRequirement,
    alive: &dyn Fn() -> bool,
) -> Result<PinUvAuthResult, AuthenticatorError> {
    // CTAP 2.1 is very specific that the request should either include pinUvAuthParam
    // OR uv=true, but not both at the same time. We now have to decide which (if either)
    // to send. We may omit both values. Will never send an explicit uv=false, because
    //  a) this is the default, and
    //  b) some CTAP 2.0 authenticators return UnsupportedOption when uv=false.

    // We ensure both pinUvAuthParam and uv are not set to start.
    cmd.set_pin_uv_auth_param(None)?;
    cmd.set_uv_option(None);

    // Skip user verification if we're using CTAP1 or if the device does not support CTAP2.
    let info = match (dev.get_protocol(), dev.get_authenticator_info()) {
        (FidoProtocol::CTAP2, Some(info)) => info,
        _ => return Ok(PinUvAuthResult::DeviceIsCtap1),
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
                .get_pin_uv_auth_token_using_uv_with_permissions(
                    permission,
                    cmd.get_rp().id(),
                    alive,
                )
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
                    alive,
                )
                .map_err(|e| repackage_pin_errors(dev, e))?;
            cmd.set_pin_uv_auth_param(Some(pin_auth_token.clone()))?;
            Ok(PinUvAuthResult::SuccessGetPinUvAuthTokenUsingPinWithPermissions(pin_auth_token))
        }
    } else {
        // CTAP 2.0 fallback
        if !skip_uv && supports_uv && cmd.pin().is_none() {
            // If the device supports internal user-verification (e.g. fingerprints),
            // skip PIN-stuff

            // We may need the shared secret for HMAC-extension, so we
            // have to establish one
            if info.supports_hmac_secret() {
                let _shared_secret = dev.establish_shared_secret(alive)?;
            }
            // CTAP 2.1, Section 6.1.1, Step 1.1.2.1.2.
            cmd.set_uv_option(Some(true));
            return Ok(PinUvAuthResult::UsingInternalUv);
        }

        let pin_auth_token = dev
            .get_pin_token(cmd.pin(), alive)
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
fn determine_puap_if_needed<Dev: FidoDevice, T: PinUvAuthCommand + Request<V>, U, V>(
    cmd: &mut T,
    dev: &mut Dev,
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
        match get_pin_uv_auth_param(cmd, dev, permission, skip_uv, uv_req, alive) {
            Ok(r) => {
                return Ok(r);
            }

            Err(AuthenticatorError::PinError(PinError::PinRequired)) => {
                if let Ok(pin) = ask_user_for_pin(false, None, status, callback) {
                    cmd.set_pin(Some(pin));
                    skip_uv = true;
                    continue;
                } else {
                    return Err(());
                }
            }
            Err(AuthenticatorError::PinError(PinError::InvalidPin(retries))) => {
                if let Ok(pin) = ask_user_for_pin(true, retries, status, callback) {
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

pub fn register<Dev: FidoDevice>(
    dev: &mut Dev,
    args: RegisterArgs,
    status: Sender<crate::StatusUpdate>,
    callback: StateCallback<crate::Result<crate::RegisterResult>>,
    alive: &dyn Fn() -> bool,
) -> bool {
    let mut options = MakeCredentialsOptions::default();

    if dev.get_protocol() == FidoProtocol::CTAP2 {
        let info = match dev.get_authenticator_info() {
            Some(info) => info,
            None => {
                callback.call(Err(HIDError::DeviceNotInitialized.into()));
                return false;
            }
        };
        // Check if extensions have been requested that are not supported by the device
        if let Some(true) = args.extensions.hmac_secret {
            if !info.supports_hmac_secret() {
                callback.call(Err(AuthenticatorError::UnsupportedOption(
                    UnsupportedOption::HmacSecret,
                )));
                return false;
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
            return false;
        }
        if args.user_verification_req == UserVerificationRequirement::Required {
            callback.call(Err(AuthenticatorError::UnsupportedOption(
                UnsupportedOption::UserVerification,
            )));
            return false;
        }
        if !args
            .pub_cred_params
            .iter()
            .any(|x| x.alg == COSEAlgorithm::ES256)
        {
            callback.call(Err(AuthenticatorError::UnsupportedOption(
                UnsupportedOption::PubCredParams,
            )));
            return false;
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
        let permissions =
            PinUvAuthTokenPermission::MakeCredential | PinUvAuthTokenPermission::GetAssertion;

        let pin_uv_auth_result = match determine_puap_if_needed(
            &mut makecred,
            dev,
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
        if dev.get_protocol() == FidoProtocol::CTAP2 {
            makecred.exclude_list = unwrap_result!(
                do_credential_list_filtering_ctap2(
                    dev,
                    &makecred.exclude_list,
                    &makecred.rp,
                    pin_uv_auth_result.get_pin_uv_auth_token(),
                ),
                callback
            );
        } else {
            let key_handle = do_credential_list_filtering_ctap1(
                dev,
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
                return false;
            }
        }

        debug!("------------------------------------------------------------------");
        debug!("{makecred:?} using {pin_uv_auth_result:?}");
        debug!("------------------------------------------------------------------");
        send_status(&status, crate::StatusUpdate::PresenceRequired);
        let resp = dev.send_msg_cancellable(&makecred, alive);
        match resp {
            Ok(MakeCredentialsResult(attestation)) => {
                callback.call(Ok(RegisterResult::CTAP2(attestation)));
                return true;
            }
            Err(HIDError::Command(CommandError::StatusCode(StatusCode::ChannelBusy, _))) => {
                // Channel busy. Client SHOULD retry the request after a short delay.
                thread::sleep(Duration::from_millis(100));
                continue;
            }
            Err(HIDError::Command(CommandError::StatusCode(StatusCode::PinAuthInvalid, _)))
                if matches!(pin_uv_auth_result, PinUvAuthResult::UsingInternalUv) =>
            {
                // This should only happen for CTAP2.0 tokens that use internal UV and
                // failed (e.g. wrong fingerprint used), while doing MakeCredentials
                send_status(
                    &status,
                    StatusUpdate::PinUvError(StatusPinUv::InvalidUv(None)),
                );
                continue;
            }
            Err(HIDError::Command(CommandError::StatusCode(StatusCode::PinRequired, _)))
                if matches!(pin_uv_auth_result, PinUvAuthResult::UsingInternalUv) =>
            {
                // This should only happen for CTAP2.0 tokens that use internal UV and failed
                // repeatedly, so that we have to fall back to PINs
                skip_uv = true;
                continue;
            }
            Err(HIDError::Command(CommandError::StatusCode(StatusCode::UvBlocked, _)))
                if matches!(
                    pin_uv_auth_result,
                    PinUvAuthResult::SuccessGetPinUvAuthTokenUsingUvWithPermissions(..)
                ) =>
            {
                // This should only happen for CTAP2.1 tokens that use internal UV and failed
                // repeatedly, so that we have to fall back to PINs
                skip_uv = true;
                continue;
            }
            Err(HIDError::Command(CommandError::StatusCode(StatusCode::CredentialExcluded, _))) => {
                callback.call(Err(AuthenticatorError::CredentialExcluded));
                return false;
            }
            Err(e) => {
                warn!("error happened: {e}");
                callback.call(Err(AuthenticatorError::HIDError(e)));
                return false;
            }
        }
    }
    false
}

pub fn sign<Dev: FidoDevice>(
    dev: &mut Dev,
    args: SignArgs,
    status: Sender<crate::StatusUpdate>,
    callback: StateCallback<crate::Result<crate::SignResult>>,
    alive: &dyn Fn() -> bool,
) -> bool {
    if dev.get_protocol() == FidoProtocol::CTAP2 {
        let info = match dev.get_authenticator_info() {
            Some(info) => info,
            None => {
                callback.call(Err(HIDError::DeviceNotInitialized.into()));
                return false;
            }
        };
        // Check if extensions have been requested that are not supported by the device
        if args.extensions.hmac_secret.is_some() && !info.supports_hmac_secret() {
            callback.call(Err(AuthenticatorError::UnsupportedOption(
                UnsupportedOption::HmacSecret,
            )));
            return false;
        }
    } else {
        // Check that the request can be processed by a CTAP1 device.
        // See CTAP 2.1 Section 10.3. Some additional checks are performed in
        // GetAssertion::RequestCtap1
        if args.user_verification_req == UserVerificationRequirement::Required {
            callback.call(Err(AuthenticatorError::UnsupportedOption(
                UnsupportedOption::UserVerification,
            )));
            return false;
        }
        if args.allow_list.is_empty() {
            callback.call(Err(AuthenticatorError::UnsupportedOption(
                UnsupportedOption::EmptyAllowList,
            )));
            return false;
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
        let pin_uv_auth_result = match determine_puap_if_needed(
            &mut get_assertion,
            dev,
            skip_uv,
            PinUvAuthTokenPermission::GetAssertion,
            args.user_verification_req,
            &status,
            &callback,
            alive,
        ) {
            Ok(r) => r,
            Err(()) => {
                return false;
            }
        };

        // Third, use the shared secret in the extensions, if requested
        if let Some(extension) = get_assertion.extensions.hmac_secret.as_mut() {
            if let Some(secret) = dev.get_shared_secret() {
                match extension.calculate(secret) {
                    Ok(x) => x,
                    Err(e) => {
                        callback.call(Err(e));
                        return false;
                    }
                }
            }
        }

        // Do "pre-flight": Filter the allow-list
        let original_allow_list_was_empty = get_assertion.allow_list.is_empty();
        if dev.get_protocol() == FidoProtocol::CTAP2 {
            get_assertion.allow_list = unwrap_result!(
                do_credential_list_filtering_ctap2(
                    dev,
                    &get_assertion.allow_list,
                    &get_assertion.rp,
                    pin_uv_auth_result.get_pin_uv_auth_token(),
                ),
                callback
            );
        } else {
            let key_handle = do_credential_list_filtering_ctap1(
                dev,
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
            return false;
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
        match resp {
            Ok(assertions) => {
                callback.call(Ok(SignResult::CTAP2(assertions)));
                return true;
            }
            Err(HIDError::Command(CommandError::StatusCode(StatusCode::ChannelBusy, _))) => {
                // Channel busy. Client SHOULD retry the request after a short delay.
                thread::sleep(Duration::from_millis(100));
                continue;
            }
            Err(HIDError::Command(CommandError::StatusCode(StatusCode::OperationDenied, _)))
                if matches!(pin_uv_auth_result, PinUvAuthResult::UsingInternalUv) =>
            {
                // This should only happen for CTAP2.0 tokens that use internal UV and failed
                // (e.g. wrong fingerprint used), while doing GetAssertion
                // Yes, this is a different error code than for MakeCredential.
                send_status(
                    &status,
                    StatusUpdate::PinUvError(StatusPinUv::InvalidUv(None)),
                );
                continue;
            }
            Err(HIDError::Command(CommandError::StatusCode(StatusCode::PinRequired, _)))
                if matches!(pin_uv_auth_result, PinUvAuthResult::UsingInternalUv) =>
            {
                // This should only happen for CTAP2.0 tokens that use internal UV and failed
                // repeatedly, so that we have to fall back to PINs
                skip_uv = true;
                continue;
            }
            Err(HIDError::Command(CommandError::StatusCode(StatusCode::UvBlocked, _)))
                if matches!(
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
                return false;
            }
        }
    }
    false
}

pub(crate) fn reset_helper(
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

pub(crate) fn set_or_change_pin_helper(
    dev: &mut Device,
    mut current_pin: Option<Pin>,
    new_pin: Pin,
    status: Sender<crate::StatusUpdate>,
    callback: StateCallback<crate::Result<crate::ResetResult>>,
    alive: &dyn Fn() -> bool,
) {
    let mut shared_secret = match dev.establish_shared_secret(alive) {
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
                None => match ask_user_for_pin(was_invalid, retries, &status, &callback) {
                    Ok(pin) => pin,
                    _ => {
                        return;
                    }
                },
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
                match dev.establish_shared_secret(alive) {
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
    // the callback is expecting `Result<(), AuthenticatorError>`, but `ChangeExistingPin`
    // and `SetNewPin` return the default `ClientPinResponse` on success. Just discard it.
    callback.call(res.map(|_| ()));
}
