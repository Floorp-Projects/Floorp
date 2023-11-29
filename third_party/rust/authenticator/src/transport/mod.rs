use crate::crypto::{PinUvAuthProtocol, PinUvAuthToken, SharedSecret};
use crate::ctap2::commands::client_pin::{
    ClientPIN, ClientPinResponse, GetKeyAgreement, GetPinToken,
    GetPinUvAuthTokenUsingPinWithPermissions, GetPinUvAuthTokenUsingUvWithPermissions,
    PinUvAuthTokenPermission,
};
use crate::ctap2::commands::get_assertion::{GetAssertion, GetAssertionResult};
use crate::ctap2::commands::get_info::{AuthenticatorInfo, AuthenticatorVersion, GetInfo};
use crate::ctap2::commands::get_version::{GetVersion, U2FInfo};
use crate::ctap2::commands::make_credentials::{
    dummy_make_credentials_cmd, MakeCredentials, MakeCredentialsResult,
};
use crate::ctap2::commands::reset::Reset;
use crate::ctap2::commands::selection::Selection;
use crate::ctap2::commands::{CommandError, RequestCtap1, RequestCtap2, StatusCode};
use crate::ctap2::preflight::CheckKeyHandle;
use crate::transport::device_selector::BlinkResult;
use crate::transport::errors::HIDError;

use crate::Pin;
use std::convert::TryFrom;
use std::fmt;

pub mod device_selector;
pub mod errors;
pub mod hid;

#[cfg(all(
    any(target_os = "linux", target_os = "freebsd", target_os = "netbsd"),
    not(test)
))]
pub mod hidproto;

#[cfg(all(target_os = "linux", not(test)))]
#[path = "linux/mod.rs"]
pub mod platform;

#[cfg(all(target_os = "freebsd", not(test)))]
#[path = "freebsd/mod.rs"]
pub mod platform;

#[cfg(all(target_os = "netbsd", not(test)))]
#[path = "netbsd/mod.rs"]
pub mod platform;

#[cfg(all(target_os = "openbsd", not(test)))]
#[path = "openbsd/mod.rs"]
pub mod platform;

#[cfg(all(target_os = "macos", not(test)))]
#[path = "macos/mod.rs"]
pub mod platform;

#[cfg(all(target_os = "windows", not(test)))]
#[path = "windows/mod.rs"]
pub mod platform;

#[cfg(not(any(
    target_os = "linux",
    target_os = "freebsd",
    target_os = "openbsd",
    target_os = "netbsd",
    target_os = "macos",
    target_os = "windows",
    test
)))]
#[path = "stub/mod.rs"]
pub mod platform;

#[cfg(test)]
#[path = "mock/mod.rs"]
pub mod platform;

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum FidoProtocol {
    CTAP1,
    CTAP2,
}

pub trait FidoDeviceIO {
    fn send_msg<Out, Req: RequestCtap1<Output = Out> + RequestCtap2<Output = Out>>(
        &mut self,
        msg: &Req,
    ) -> Result<Out, HIDError> {
        self.send_msg_cancellable(msg, &|| true)
    }

    fn send_cbor<Req: RequestCtap2>(&mut self, msg: &Req) -> Result<Req::Output, HIDError> {
        self.send_cbor_cancellable(msg, &|| true)
    }

    fn send_ctap1<Req: RequestCtap1>(&mut self, msg: &Req) -> Result<Req::Output, HIDError> {
        self.send_ctap1_cancellable(msg, &|| true)
    }

    fn send_msg_cancellable<Out, Req: RequestCtap1<Output = Out> + RequestCtap2<Output = Out>>(
        &mut self,
        msg: &Req,
        keep_alive: &dyn Fn() -> bool,
    ) -> Result<Out, HIDError>;

    fn send_cbor_cancellable<Req: RequestCtap2>(
        &mut self,
        msg: &Req,
        keep_alive: &dyn Fn() -> bool,
    ) -> Result<Req::Output, HIDError>;

    fn send_ctap1_cancellable<Req: RequestCtap1>(
        &mut self,
        msg: &Req,
        keep_alive: &dyn Fn() -> bool,
    ) -> Result<Req::Output, HIDError>;
}

pub trait TestDevice {
    #[cfg(test)]
    fn skip_serialization(&self) -> bool;
    #[cfg(test)]
    fn send_ctap1_unserialized<Req: RequestCtap1>(
        &mut self,
        msg: &Req,
    ) -> Result<Req::Output, HIDError>;
    #[cfg(test)]
    fn send_ctap2_unserialized<Req: RequestCtap2>(
        &mut self,
        msg: &Req,
    ) -> Result<Req::Output, HIDError>;
}

pub trait FidoDevice: FidoDeviceIO
where
    Self: Sized,
    Self: fmt::Debug,
{
    fn pre_init(&mut self) -> Result<(), HIDError>;
    fn initialized(&self) -> bool;

    // Check if the device is actually a token
    fn is_u2f(&mut self) -> bool;
    fn should_try_ctap2(&self) -> bool;
    fn get_authenticator_info(&self) -> Option<&AuthenticatorInfo>;
    fn set_authenticator_info(&mut self, authenticator_info: AuthenticatorInfo);
    fn refresh_authenticator_info(&mut self) -> Option<&AuthenticatorInfo> {
        let command = GetInfo::default();
        if let Ok(info) = self.send_cbor(&command) {
            debug!("Refreshed authenticator info: {:?}", info);
            self.set_authenticator_info(info);
        }
        self.get_authenticator_info()
    }

    // `get_protocol()` indicates whether we're using CTAP1 or CTAP2.
    // Prior to initializing the device, `get_protocol()` should return CTAP2 unless
    // there's a reason to believe that the device does not support CTAP2 (e.g. if
    // it's a HID device and it does not have the CBOR capability).
    fn get_protocol(&self) -> FidoProtocol;

    // We do not provide a generic `set_protocol(..)` function as this would have complicated
    // interactions with the AuthenticatorInfo state.
    fn downgrade_to_ctap1(&mut self);

    fn get_shared_secret(&self) -> Option<&SharedSecret>;
    fn set_shared_secret(&mut self, secret: SharedSecret);

    fn init(&mut self) -> Result<(), HIDError> {
        self.pre_init()?;

        if self.should_try_ctap2() {
            let command = GetInfo::default();
            if let Ok(info) = self.send_cbor(&command) {
                debug!("{:?}", info);
                if info.max_supported_version() == AuthenticatorVersion::U2F_V2 {
                    self.downgrade_to_ctap1();
                }
                self.set_authenticator_info(info);
                return Ok(());
            }
        }

        self.downgrade_to_ctap1();
        // We want to return an error here if this device doesn't support CTAP1,
        // so we send a U2F_VERSION command.
        let command = GetVersion::default();
        self.send_ctap1(&command)?;
        Ok(())
    }

    fn block_and_blink(&mut self, keep_alive: &dyn Fn() -> bool) -> BlinkResult {
        let supports_select_cmd = self.get_protocol() == FidoProtocol::CTAP2
            && self.get_authenticator_info().map_or(false, |i| {
                i.versions.contains(&AuthenticatorVersion::FIDO_2_1)
            });
        let resp = if supports_select_cmd {
            let msg = Selection {};
            self.send_cbor_cancellable(&msg, keep_alive)
        } else {
            // We need to fake a blink-request, because FIDO2.0 forgot to specify one
            // See: https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#using-pinToken-in-authenticatorMakeCredential
            let msg = dummy_make_credentials_cmd();
            info!("Trying to blink: {:?}", &msg);
            // We don't care about the Ok-value, just if it is Ok or not
            self.send_msg_cancellable(&msg, keep_alive).map(|_| ())
        };

        match resp {
            // Spec only says PinInvalid or PinNotSet should be returned on the fake touch-request,
            // but Yubikeys for example return PinAuthInvalid. A successful return is also possible
            // for CTAP1-tokens so we catch those here as well.
            Ok(_)
            | Err(HIDError::Command(CommandError::StatusCode(StatusCode::PinInvalid, _)))
            | Err(HIDError::Command(CommandError::StatusCode(StatusCode::PinAuthInvalid, _)))
            | Err(HIDError::Command(CommandError::StatusCode(StatusCode::PinNotSet, _))) => {
                BlinkResult::DeviceSelected
            }
            // We cancelled the receive, because another device was selected.
            Err(HIDError::Command(CommandError::StatusCode(StatusCode::KeepaliveCancel, _)))
            | Err(HIDError::Command(CommandError::StatusCode(StatusCode::OperationDenied, _)))
            | Err(HIDError::Command(CommandError::StatusCode(StatusCode::UserActionTimeout, _))) => {
                // TODO: Repeat the request, if it is a UserActionTimeout?
                debug!("Device {:?} got cancelled", &self);
                BlinkResult::Cancelled
            }
            // Something unexpected happened, so we assume this device is not usable and
            // interpreting this equivalent to being cancelled.
            e => {
                info!("Device {:?} received unexpected answer, so we assume an error occurred and we are NOT using this device (assuming the request was cancelled): {:?}", &self, e);
                BlinkResult::Cancelled
            }
        }
    }

    fn establish_shared_secret(
        &mut self,
        alive: &dyn Fn() -> bool,
    ) -> Result<SharedSecret, HIDError> {
        // CTAP1 devices don't support establishing a shared secret
        let info = match (self.get_protocol(), self.get_authenticator_info()) {
            (FidoProtocol::CTAP2, Some(info)) => info,
            _ => return Err(HIDError::UnsupportedCommand),
        };

        let pin_protocol = PinUvAuthProtocol::try_from(info)?;

        // Not reusing the shared secret here, if it exists, since we might start again
        // with a different PIN (e.g. if the last one was wrong)
        let pin_command = GetKeyAgreement::new(pin_protocol.clone());
        let resp = self.send_cbor_cancellable(&pin_command, alive)?;
        if let Some(device_key_agreement_key) = resp.key_agreement {
            let shared_secret = pin_protocol
                .encapsulate(&device_key_agreement_key)
                .map_err(CommandError::from)?;
            self.set_shared_secret(shared_secret.clone());
            Ok(shared_secret)
        } else {
            Err(HIDError::Command(CommandError::MissingRequiredField(
                "key_agreement",
            )))
        }
    }

    /// CTAP 2.0-only version:
    /// "Getting pinUvAuthToken using getPinToken (superseded)"
    fn get_pin_token(
        &mut self,
        pin: &Option<Pin>,
        alive: &dyn Fn() -> bool,
    ) -> Result<PinUvAuthToken, HIDError> {
        // Asking the user for PIN before establishing the shared secret
        let pin = pin
            .as_ref()
            .ok_or(CommandError::StatusCode(StatusCode::PinRequired, None))?;

        // Not reusing the shared secret here, if it exists, since we might start again
        // with a different PIN (e.g. if the last one was wrong)
        let shared_secret = self.establish_shared_secret(alive)?;

        let pin_command = GetPinToken::new(&shared_secret, pin);
        let resp = self.send_cbor_cancellable(&pin_command, alive)?;
        if let Some(encrypted_pin_token) = resp.pin_token {
            // CTAP 2.1 spec:
            // If authenticatorClientPIN's getPinToken subcommand is invoked, default permissions
            // of `mc` and `ga` (value 0x03) are granted for the returned pinUvAuthToken.
            let default_permissions = PinUvAuthTokenPermission::default();
            let pin_token = shared_secret
                .decrypt_pin_token(default_permissions, encrypted_pin_token.as_ref())
                .map_err(CommandError::from)?;
            Ok(pin_token)
        } else {
            Err(HIDError::Command(CommandError::MissingRequiredField(
                "pin_token",
            )))
        }
    }

    fn get_pin_uv_auth_token_using_uv_with_permissions(
        &mut self,
        permission: PinUvAuthTokenPermission,
        rp_id: Option<&String>,
        alive: &dyn Fn() -> bool,
    ) -> Result<PinUvAuthToken, HIDError> {
        // Explicitly not reusing the shared secret here
        let shared_secret = self.establish_shared_secret(alive)?;
        let pin_command = GetPinUvAuthTokenUsingUvWithPermissions::new(
            &shared_secret,
            permission,
            rp_id.cloned(),
        );

        let resp = self.send_cbor_cancellable(&pin_command, alive)?;

        if let Some(encrypted_pin_token) = resp.pin_token {
            let pin_token = shared_secret
                .decrypt_pin_token(permission, encrypted_pin_token.as_ref())
                .map_err(CommandError::from)?;
            Ok(pin_token)
        } else {
            Err(HIDError::Command(CommandError::MissingRequiredField(
                "pin_token",
            )))
        }
    }

    fn get_pin_uv_auth_token_using_pin_with_permissions(
        &mut self,
        pin: &Option<Pin>,
        permission: PinUvAuthTokenPermission,
        rp_id: Option<&String>,
        alive: &dyn Fn() -> bool,
    ) -> Result<PinUvAuthToken, HIDError> {
        // Asking the user for PIN before establishing the shared secret
        let pin = pin
            .as_ref()
            .ok_or(CommandError::StatusCode(StatusCode::PinRequired, None))?;

        // Not reusing the shared secret here, if it exists, since we might start again
        // with a different PIN (e.g. if the last one was wrong)
        let shared_secret = self.establish_shared_secret(alive)?;
        let pin_command = GetPinUvAuthTokenUsingPinWithPermissions::new(
            &shared_secret,
            pin,
            permission,
            rp_id.cloned(),
        );

        let resp = self.send_cbor_cancellable(&pin_command, alive)?;

        if let Some(encrypted_pin_token) = resp.pin_token {
            let pin_token = shared_secret
                .decrypt_pin_token(permission, encrypted_pin_token.as_ref())
                .map_err(CommandError::from)?;
            Ok(pin_token)
        } else {
            Err(HIDError::Command(CommandError::MissingRequiredField(
                "pin_token",
            )))
        }
    }
}

pub trait VirtualFidoDevice: FidoDevice {
    fn check_key_handle(&self, req: &CheckKeyHandle) -> Result<(), HIDError>;
    fn client_pin(&self, req: &ClientPIN) -> Result<ClientPinResponse, HIDError>;
    fn get_assertion(&self, req: &GetAssertion) -> Result<Vec<GetAssertionResult>, HIDError>;
    fn get_info(&self) -> Result<AuthenticatorInfo, HIDError>;
    fn get_version(&self, req: &GetVersion) -> Result<U2FInfo, HIDError>;
    fn make_credentials(&self, req: &MakeCredentials) -> Result<MakeCredentialsResult, HIDError>;
    fn reset(&self, req: &Reset) -> Result<(), HIDError>;
    fn selection(&self, req: &Selection) -> Result<(), HIDError>;
}
