use super::Pin;
use crate::{
    ctap2::{
        commands::{
            authenticator_config::{AuthConfigCommand, AuthConfigResult},
            bio_enrollment::BioTemplateId,
            get_info::AuthenticatorInfo,
            PinUvAuthResult,
        },
        server::{PublicKeyCredentialDescriptor, PublicKeyCredentialUserEntity},
    },
    BioEnrollmentResult, CredentialManagementResult,
};
use serde::{Deserialize, Serialize as DeriveSer, Serializer};
use std::sync::mpsc::Sender;

#[derive(Debug, Deserialize, DeriveSer)]
pub enum CredManagementCmd {
    GetCredentials,
    DeleteCredential(PublicKeyCredentialDescriptor),
    UpdateUserInformation(PublicKeyCredentialDescriptor, PublicKeyCredentialUserEntity),
}

#[derive(Debug, Deserialize, DeriveSer)]
pub enum BioEnrollmentCmd {
    GetFingerprintSensorInfo,
    GetEnrollments,
    StartNewEnrollment(Option<String>),
    DeleteEnrollment(BioTemplateId),
    ChangeName(BioTemplateId, String),
}

#[derive(Debug)]
pub enum InteractiveRequest {
    Quit,
    Reset,
    ChangePIN(Pin, Pin),
    SetPIN(Pin),
    ChangeConfig(AuthConfigCommand, Option<PinUvAuthResult>),
    CredentialManagement(CredManagementCmd, Option<PinUvAuthResult>),
    BioEnrollment(BioEnrollmentCmd, Option<PinUvAuthResult>),
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
pub enum InteractiveUpdate {
    StartManagement((Sender<InteractiveRequest>, Option<AuthenticatorInfo>)),
    // We provide the already determined PUAT to be able to issue more requests without
    // forcing the user to enter another PIN.
    BioEnrollmentUpdate((BioEnrollmentResult, Option<PinUvAuthResult>)),
    CredentialManagementUpdate((CredentialManagementResult, Option<PinUvAuthResult>)),
    AuthConfigUpdate((AuthConfigResult, Option<PinUvAuthResult>)),
}

#[derive(Debug)]
pub enum StatusUpdate {
    /// We're waiting for the user to touch their token
    PresenceRequired,
    /// Sent if a PIN is needed (or was wrong), or some other kind of PIN-related
    /// error occurred. The Sender is for sending back a PIN (if needed).
    PinUvError(StatusPinUv),
    /// Sent, if multiple devices are found and the user has to select one
    SelectDeviceNotice,
    /// Sent when a token was selected for interactive management
    InteractiveManagement(InteractiveUpdate),
    /// Sent when a token returns multiple results for a getAssertion request
    SelectResultNotice(Sender<Option<usize>>, Vec<PublicKeyCredentialUserEntity>),
}

pub(crate) fn send_status(status: &Sender<StatusUpdate>, msg: StatusUpdate) {
    match status.send(msg) {
        Ok(_) => {}
        Err(e) => error!("Couldn't send status: {:?}", e),
    };
}
