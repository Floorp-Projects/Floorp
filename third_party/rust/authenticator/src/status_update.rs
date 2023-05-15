use super::{u2ftypes, Pin};
use crate::ctap2::commands::get_info::AuthenticatorInfo;
use serde::{Deserialize, Serialize as DeriveSer, Serializer};
use std::sync::mpsc::Sender;

#[derive(Debug, Deserialize, DeriveSer)]
pub enum InteractiveRequest {
    Reset,
    ChangePIN(Pin, Pin),
    SetPIN(Pin),
}

// Simply ignoring the Sender when serializing
pub(crate) fn serialize_pin_required<S>(_: &Sender<Pin>, s: S) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    s.serialize_none()
}

// Simply ignoring the Sender when serializing
pub(crate) fn serialize_pin_invalid<S>(
    _: &Sender<Pin>,
    retries: &Option<u8>,
    s: S,
) -> Result<S::Ok, S::Error>
where
    S: Serializer,
{
    if let Some(r) = retries {
        s.serialize_u8(*r)
    } else {
        s.serialize_none()
    }
}

#[derive(Debug, DeriveSer)]
pub enum StatusPinUv {
    #[serde(serialize_with = "serialize_pin_required")]
    PinRequired(Sender<Pin>),
    #[serde(serialize_with = "serialize_pin_invalid")]
    InvalidPin(Sender<Pin>, Option<u8>),
    PinIsTooShort,
    PinIsTooLong(usize),
    InvalidUv(Option<u8>),
    // This SHOULD ever only happen for CTAP2.0 devices that
    // use internal UV (e.g. fingerprint sensors) and failed (e.g. wrong
    // finger used).
    // PinAuthInvalid, // Folded into InvalidUv
    PinAuthBlocked,
    PinBlocked,
    PinNotSet,
    UvBlocked,
}

#[derive(Debug)]
pub enum StatusUpdate {
    /// Device found
    DeviceAvailable { dev_info: u2ftypes::U2FDeviceInfo },
    /// Device got removed
    DeviceUnavailable { dev_info: u2ftypes::U2FDeviceInfo },
    /// We're waiting for the user to touch their token
    PresenceRequired,
    /// We successfully finished the register or sign request
    Success { dev_info: u2ftypes::U2FDeviceInfo },
    /// Sent if a PIN is needed (or was wrong), or some other kind of PIN-related
    /// error occurred. The Sender is for sending back a PIN (if needed).
    PinUvError(StatusPinUv),
    /// Sent, if multiple devices are found and the user has to select one
    SelectDeviceNotice,
    /// Sent, once a device was selected (either automatically or by user-interaction)
    /// and the register or signing process continues with this device
    DeviceSelected(u2ftypes::U2FDeviceInfo),
    /// Sent when a token was selected for interactive management
    InteractiveManagement(
        (
            Sender<InteractiveRequest>,
            u2ftypes::U2FDeviceInfo,
            Option<AuthenticatorInfo>,
        ),
    ),
}

pub(crate) fn send_status(status: &Sender<StatusUpdate>, msg: StatusUpdate) {
    match status.send(msg) {
        Ok(_) => {}
        Err(e) => error!("Couldn't send status: {:?}", e),
    };
}
