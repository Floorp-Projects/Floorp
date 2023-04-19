use crate::consts::HIDCmd;
use crate::crypto::{PinUvAuthProtocol, PinUvAuthToken, SharedSecret};
use crate::ctap2::commands::client_pin::{
    GetKeyAgreement, GetPinToken, GetPinUvAuthTokenUsingPinWithPermissions,
    GetPinUvAuthTokenUsingUvWithPermissions, PinUvAuthTokenPermission,
};
use crate::ctap2::commands::get_info::{AuthenticatorVersion, GetInfo};
use crate::ctap2::commands::get_version::GetVersion;
use crate::ctap2::commands::make_credentials::dummy_make_credentials_cmd;
use crate::ctap2::commands::selection::Selection;
use crate::ctap2::commands::{
    CommandError, Request, RequestCtap1, RequestCtap2, Retryable, StatusCode,
};
use crate::transport::device_selector::BlinkResult;
use crate::transport::errors::{ApduErrorStatus, HIDError};
use crate::transport::hid::HIDDevice;
use crate::util::io_err;
use crate::Pin;
use std::convert::TryFrom;
use std::thread;
use std::time::Duration;

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

#[derive(Debug)]
pub enum Nonce {
    CreateRandom,
    Use([u8; 8]),
}

// TODO(MS): This is the lazy way: FidoDevice currently only extends HIDDevice by more functions,
//           but the goal is to remove U2FDevice entirely and copy over the trait-definition here
pub trait FidoDevice: HIDDevice {
    fn send_msg<Out, Req: Request<Out>>(&mut self, msg: &Req) -> Result<Out, HIDError> {
        self.send_msg_cancellable(msg, &|| true)
    }

    fn send_cbor<Req: RequestCtap2>(&mut self, msg: &Req) -> Result<Req::Output, HIDError> {
        self.send_cbor_cancellable(msg, &|| true)
    }

    fn send_ctap1<Req: RequestCtap1>(&mut self, msg: &Req) -> Result<Req::Output, HIDError> {
        self.send_ctap1_cancellable(msg, &|| true)
    }

    fn send_msg_cancellable<Out, Req: Request<Out>>(
        &mut self,
        msg: &Req,
        keep_alive: &dyn Fn() -> bool,
    ) -> Result<Out, HIDError> {
        if !self.initialized() {
            return Err(HIDError::DeviceNotInitialized);
        }

        if self.supports_ctap2() {
            self.send_cbor_cancellable(msg, keep_alive)
        } else {
            self.send_ctap1_cancellable(msg, keep_alive)
        }
    }

    fn send_cbor_cancellable<Req: RequestCtap2>(
        &mut self,
        msg: &Req,
        keep_alive: &dyn Fn() -> bool,
    ) -> Result<Req::Output, HIDError> {
        debug!("sending {:?} to {:?}", msg, self);

        let mut data = msg.wire_format(self)?;
        let mut buf: Vec<u8> = Vec::with_capacity(data.len() + 1);
        // CTAP2 command
        buf.push(Req::command() as u8);
        // payload
        buf.append(&mut data);
        let buf = buf;

        let (cmd, resp) = self.sendrecv(HIDCmd::Cbor, &buf, keep_alive)?;
        debug!(
            "got from Device {:?} status={:?}: {:?}",
            self.id(),
            cmd,
            resp
        );
        if cmd == HIDCmd::Cbor {
            Ok(msg.handle_response_ctap2(self, &resp)?)
        } else {
            Err(HIDError::UnexpectedCmd(cmd.into()))
        }
    }

    fn send_ctap1_cancellable<Req: RequestCtap1>(
        &mut self,
        msg: &Req,
        keep_alive: &dyn Fn() -> bool,
    ) -> Result<Req::Output, HIDError> {
        debug!("sending {:?} to {:?}", msg, self);
        let (data, add_info) = msg.ctap1_format(self)?;

        while keep_alive() {
            // sendrecv will not block with a CTAP1 device
            let (cmd, mut data) = self.sendrecv(HIDCmd::Msg, &data, &|| true)?;
            debug!(
                "got from Device {:?} status={:?}: {:?}",
                self.id(),
                cmd,
                data
            );
            if cmd == HIDCmd::Msg {
                if data.len() < 2 {
                    return Err(io_err("Unexpected Response: shorter than expected").into());
                }
                let split_at = data.len() - 2;
                let status = data.split_off(split_at);
                // This will bubble up error if status != no error
                let status = ApduErrorStatus::from([status[0], status[1]]);

                match msg.handle_response_ctap1(status, &data, &add_info) {
                    Ok(out) => return Ok(out),
                    Err(Retryable::Retry) => {
                        // sleep 100ms then loop again
                        // TODO(baloo): meh, use tokio instead?
                        thread::sleep(Duration::from_millis(100));
                    }
                    Err(Retryable::Error(e)) => return Err(e),
                }
            } else {
                return Err(HIDError::UnexpectedCmd(cmd.into()));
            }
        }

        Err(HIDError::Command(CommandError::StatusCode(
            StatusCode::KeepaliveCancel,
            None,
        )))
    }

    // This is ugly as we have 2 init-functions now, but the fastest way currently.
    fn init(&mut self, nonce: Nonce) -> Result<(), HIDError> {
        <Self as HIDDevice>::initialize(self, nonce)?;
        // TODO(baloo): this logic should be moved to
        //              transport/mod.rs::Device trait
        if self.supports_ctap2() {
            let command = GetInfo::default();
            let info = self.send_cbor(&command)?;
            debug!("{:?} infos: {:?}", self.id(), info);

            self.set_authenticator_info(info);
        }
        if self.supports_ctap1() {
            let command = GetVersion::default();
            // We don't really use the result here
            self.send_ctap1(&command)?;
        }
        Ok(())
    }

    fn block_and_blink(&mut self, keep_alive: &dyn Fn() -> bool) -> BlinkResult {
        let supports_select_cmd = self.get_authenticator_info().map_or(false, |i| {
            i.versions.contains(&AuthenticatorVersion::FIDO_2_1)
        });
        let resp = if supports_select_cmd {
            let msg = Selection {};
            self.send_cbor_cancellable(&msg, keep_alive)
        } else {
            // We need to fake a blink-request, because FIDO2.0 forgot to specify one
            // See: https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#using-pinToken-in-authenticatorMakeCredential
            let msg = match dummy_make_credentials_cmd() {
                Ok(m) => m,
                Err(_) => {
                    return BlinkResult::Cancelled;
                }
            };
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

    fn establish_shared_secret(&mut self) -> Result<SharedSecret, HIDError> {
        if !self.supports_ctap2() {
            return Err(HIDError::UnsupportedCommand);
        }

        let info = self
            .get_authenticator_info()
            .ok_or(HIDError::DeviceNotInitialized)?;
        let pin_protocol = PinUvAuthProtocol::try_from(info)?;

        // Not reusing the shared secret here, if it exists, since we might start again
        // with a different PIN (e.g. if the last one was wrong)
        let pin_command = GetKeyAgreement::new(pin_protocol);
        let device_key_agreement = self.send_cbor(&pin_command)?;
        let shared_secret = device_key_agreement.shared_secret()?;
        self.set_shared_secret(shared_secret.clone());
        Ok(shared_secret)
    }

    /// CTAP 2.0-only version:
    /// "Getting pinUvAuthToken using getPinToken (superseded)"
    fn get_pin_token(&mut self, pin: &Option<Pin>) -> Result<PinUvAuthToken, HIDError> {
        // Asking the user for PIN before establishing the shared secret
        let pin = pin
            .as_ref()
            .ok_or(CommandError::StatusCode(StatusCode::PinRequired, None))?;

        // Not reusing the shared secret here, if it exists, since we might start again
        // with a different PIN (e.g. if the last one was wrong)
        let shared_secret = self.establish_shared_secret()?;

        let pin_command = GetPinToken::new(&shared_secret, pin);
        let pin_token = self.send_cbor(&pin_command)?;

        Ok(pin_token)
    }

    fn get_pin_uv_auth_token_using_uv_with_permissions(
        &mut self,
        permission: PinUvAuthTokenPermission,
        rp_id: Option<&String>,
    ) -> Result<PinUvAuthToken, HIDError> {
        // Explicitly not reusing the shared secret here
        let shared_secret = self.establish_shared_secret()?;
        let pin_command = GetPinUvAuthTokenUsingUvWithPermissions::new(
            &shared_secret,
            permission,
            rp_id.cloned(),
        );
        let pin_auth_token = self.send_cbor(&pin_command)?;

        Ok(pin_auth_token)
    }

    fn get_pin_uv_auth_token_using_pin_with_permissions(
        &mut self,
        pin: &Option<Pin>,
        permission: PinUvAuthTokenPermission,
        rp_id: Option<&String>,
    ) -> Result<PinUvAuthToken, HIDError> {
        // Asking the user for PIN before establishing the shared secret
        let pin = pin
            .as_ref()
            .ok_or(CommandError::StatusCode(StatusCode::PinRequired, None))?;

        // Not reusing the shared secret here, if it exists, since we might start again
        // with a different PIN (e.g. if the last one was wrong)
        let shared_secret = self.establish_shared_secret()?;
        let pin_command = GetPinUvAuthTokenUsingPinWithPermissions::new(
            &shared_secret,
            pin,
            permission,
            rp_id.cloned(),
        );
        let pin_auth_token = self.send_cbor(&pin_command)?;

        Ok(pin_auth_token)
    }
}
