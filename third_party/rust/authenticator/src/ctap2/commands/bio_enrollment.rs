use crate::{
    crypto::{PinUvAuthParam, PinUvAuthToken},
    ctap2::server::UserVerificationRequirement,
    errors::AuthenticatorError,
    transport::errors::HIDError,
    AuthenticatorInfo, FidoDevice,
};
use serde::{
    de::{Error as SerdeError, IgnoredAny, MapAccess, Visitor},
    ser::SerializeMap,
    Deserialize, Deserializer, Serialize, Serializer,
};
use serde_bytes::ByteBuf;
use serde_cbor::{from_slice, to_vec, Value};
use std::fmt;

use super::{Command, CommandError, CtapResponse, PinUvAuthCommand, RequestCtap2, StatusCode};

#[derive(Debug, Clone, Copy)]
pub enum BioEnrollmentModality {
    Fingerprint,
    Other(u8),
}

impl From<u8> for BioEnrollmentModality {
    fn from(value: u8) -> Self {
        match value {
            0x01 => BioEnrollmentModality::Fingerprint,
            x => BioEnrollmentModality::Other(x),
        }
    }
}

impl From<BioEnrollmentModality> for u8 {
    fn from(value: BioEnrollmentModality) -> Self {
        match value {
            BioEnrollmentModality::Fingerprint => 0x01,
            BioEnrollmentModality::Other(x) => x,
        }
    }
}

impl Serialize for BioEnrollmentModality {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_u8((*self).into())
    }
}

impl<'de> Deserialize<'de> for BioEnrollmentModality {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct BioEnrollmentModalityVisitor;

        impl<'de> Visitor<'de> for BioEnrollmentModalityVisitor {
            type Value = BioEnrollmentModality;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("an integer")
            }

            fn visit_u8<E>(self, v: u8) -> Result<Self::Value, E>
            where
                E: SerdeError,
            {
                Ok(BioEnrollmentModality::from(v))
            }
        }

        deserializer.deserialize_u8(BioEnrollmentModalityVisitor)
    }
}

pub type BioTemplateId = Vec<u8>;
#[derive(Debug, Clone, Deserialize, Default)]
struct BioEnrollmentParams {
    template_id: Option<BioTemplateId>,     // Template Identifier.
    template_friendly_name: Option<String>, // Template Friendly Name.
    timeout_milliseconds: Option<u64>,      // Timeout in milliSeconds.
}

impl BioEnrollmentParams {
    fn has_some(&self) -> bool {
        self.template_id.is_some()
            || self.template_friendly_name.is_some()
            || self.timeout_milliseconds.is_some()
    }
}

impl Serialize for BioEnrollmentParams {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut map_len = 0;
        if self.template_id.is_some() {
            map_len += 1;
        }
        if self.template_friendly_name.is_some() {
            map_len += 1;
        }
        if self.timeout_milliseconds.is_some() {
            map_len += 1;
        }

        let mut map = serializer.serialize_map(Some(map_len))?;
        if let Some(template_id) = &self.template_id {
            map.serialize_entry(&0x01, &ByteBuf::from(template_id.as_slice()))?;
        }
        if let Some(template_friendly_name) = &self.template_friendly_name {
            map.serialize_entry(&0x02, template_friendly_name)?;
        }
        if let Some(timeout_milliseconds) = &self.timeout_milliseconds {
            map.serialize_entry(&0x03, timeout_milliseconds)?;
        }
        map.end()
    }
}

#[derive(Debug)]
pub enum BioEnrollmentCommand {
    EnrollBegin(Option<u64>),
    EnrollCaptureNextSample((BioTemplateId, Option<u64>)),
    CancelCurrentEnrollment,
    EnumerateEnrollments,
    SetFriendlyName((BioTemplateId, String)),
    RemoveEnrollment(BioTemplateId),
    GetFingerprintSensorInfo,
}

impl BioEnrollmentCommand {
    fn to_id_and_param(&self) -> (u8, BioEnrollmentParams) {
        let mut params = BioEnrollmentParams::default();
        match &self {
            BioEnrollmentCommand::EnrollBegin(timeout) => {
                params.timeout_milliseconds = *timeout;
                (0x01, params)
            }
            BioEnrollmentCommand::EnrollCaptureNextSample((id, timeout)) => {
                params.template_id = Some(id.clone());
                params.timeout_milliseconds = *timeout;
                (0x02, params)
            }
            BioEnrollmentCommand::CancelCurrentEnrollment => (0x03, params),
            BioEnrollmentCommand::EnumerateEnrollments => (0x04, params),
            BioEnrollmentCommand::SetFriendlyName((id, name)) => {
                params.template_id = Some(id.clone());
                params.template_friendly_name = Some(name.clone());
                (0x05, params)
            }
            BioEnrollmentCommand::RemoveEnrollment(id) => {
                params.template_id = Some(id.clone());
                (0x06, params)
            }
            BioEnrollmentCommand::GetFingerprintSensorInfo => (0x07, params),
        }
    }
}

#[derive(Debug)]
pub struct BioEnrollment {
    /// The user verification modality being requested
    modality: BioEnrollmentModality,
    /// The authenticator user verification sub command currently being requested
    pub(crate) subcommand: BioEnrollmentCommand,
    /// First 16 bytes of HMAC-SHA-256 of contents using pinUvAuthToken.
    pin_uv_auth_param: Option<PinUvAuthParam>,
    use_legacy_preview: bool,
}

impl BioEnrollment {
    pub(crate) fn new(subcommand: BioEnrollmentCommand, use_legacy_preview: bool) -> Self {
        Self {
            modality: BioEnrollmentModality::Fingerprint, // As per spec: Currently always "Fingerprint"
            subcommand,
            pin_uv_auth_param: None,
            use_legacy_preview,
        }
    }
}

impl Serialize for BioEnrollment {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        // Need to define how many elements are going to be in the map
        // beforehand
        let mut map_len = 2;
        let (id, params) = self.subcommand.to_id_and_param();
        if params.has_some() {
            map_len += 1;
        }
        if self.pin_uv_auth_param.is_some() {
            map_len += 2;
        }

        let mut map = serializer.serialize_map(Some(map_len))?;

        map.serialize_entry(&0x01, &self.modality)?; // Per spec currently always Fingerprint
        map.serialize_entry(&0x02, &id)?;
        if params.has_some() {
            map.serialize_entry(&0x03, &params)?;
        }

        if let Some(ref pin_uv_auth_param) = self.pin_uv_auth_param {
            map.serialize_entry(&0x04, &pin_uv_auth_param.pin_protocol.id())?;
            map.serialize_entry(&0x05, pin_uv_auth_param)?;
        }

        map.end()
    }
}

impl PinUvAuthCommand for BioEnrollment {
    fn get_rp_id(&self) -> Option<&String> {
        None
    }

    fn set_pin_uv_auth_param(
        &mut self,
        pin_uv_auth_token: Option<PinUvAuthToken>,
    ) -> Result<(), AuthenticatorError> {
        let mut param = None;
        if let Some(token) = pin_uv_auth_token {
            // pinUvAuthParam (0x04): the result of calling
            // authenticate(pinUvAuthToken, fingerprint (0x01) || uint8(subCommand) || subCommandParams).
            let (id, params) = self.subcommand.to_id_and_param();
            let modality = self.modality.into();
            let mut data = vec![modality, id];
            if params.has_some() {
                data.extend(to_vec(&params).map_err(CommandError::Serializing)?);
            }
            param = Some(token.derive(&data).map_err(CommandError::Crypto)?);
        }
        self.pin_uv_auth_param = param;
        Ok(())
    }

    fn can_skip_user_verification(
        &mut self,
        _info: &crate::AuthenticatorInfo,
        _uv: UserVerificationRequirement,
    ) -> bool {
        // "discouraged" does not exist for BioEnrollment
        false
    }

    fn set_uv_option(&mut self, _uv: Option<bool>) {
        /* No-op */
    }

    fn get_pin_uv_auth_param(&self) -> Option<&PinUvAuthParam> {
        self.pin_uv_auth_param.as_ref()
    }
}

impl RequestCtap2 for BioEnrollment {
    type Output = BioEnrollmentResponse;

    fn command(&self) -> Command {
        if self.use_legacy_preview {
            Command::BioEnrollmentPreview
        } else {
            Command::BioEnrollment
        }
    }

    fn wire_format(&self) -> Result<Vec<u8>, HIDError> {
        let output = to_vec(&self).map_err(CommandError::Serializing)?;
        trace!("client subcommmand: {:04X?}", &output);
        Ok(output)
    }

    fn handle_response_ctap2<Dev>(
        &self,
        _dev: &mut Dev,
        input: &[u8],
    ) -> Result<Self::Output, HIDError>
    where
        Dev: FidoDevice,
    {
        if input.is_empty() {
            return Err(CommandError::InputTooSmall.into());
        }

        let status: StatusCode = input[0].into();
        if status.is_ok() {
            if input.len() > 1 {
                trace!("parsing bio enrollment response data: {:#04X?}", &input);
                let bio_enrollment =
                    from_slice(&input[1..]).map_err(CommandError::Deserializing)?;
                Ok(bio_enrollment)
            } else {
                // Some subcommands return only an OK-status without any data
                Ok(BioEnrollmentResponse::default())
            }
        } else {
            let data: Option<Value> = if input.len() > 1 {
                Some(from_slice(&input[1..]).map_err(CommandError::Deserializing)?)
            } else {
                None
            };
            Err(CommandError::StatusCode(status, data).into())
        }
    }

    fn send_to_virtual_device<Dev: crate::VirtualFidoDevice>(
        &self,
        _dev: &mut Dev,
    ) -> Result<Self::Output, HIDError> {
        unimplemented!()
    }
}

#[derive(Debug, Copy, Clone, Serialize)]
pub enum LastEnrollmentSampleStatus {
    /// Good fingerprint capture.
    Ctap2EnrollFeedbackFpGood,
    /// Fingerprint was too high.
    Ctap2EnrollFeedbackFpTooHigh,
    /// Fingerprint was too low.
    Ctap2EnrollFeedbackFpTooLow,
    /// Fingerprint was too left.
    Ctap2EnrollFeedbackFpTooLeft,
    /// Fingerprint was too right.
    Ctap2EnrollFeedbackFpTooRight,
    /// Fingerprint was too fast.
    Ctap2EnrollFeedbackFpTooFast,
    /// Fingerprint was too slow.
    Ctap2EnrollFeedbackFpTooSlow,
    /// Fingerprint was of poor quality.
    Ctap2EnrollFeedbackFpPoorQuality,
    /// Fingerprint was too skewed.
    Ctap2EnrollFeedbackFpTooSkewed,
    /// Fingerprint was too short.
    Ctap2EnrollFeedbackFpTooShort,
    /// Merge failure of the capture.
    Ctap2EnrollFeedbackFpMergeFailure,
    /// Fingerprint already exists.
    Ctap2EnrollFeedbackFpExists,
    /// (this error number is available)
    Unused,
    /// User did not touch/swipe the authenticator.
    Ctap2EnrollFeedbackNoUserActivity,
    /// User did not lift the finger off the sensor.
    Ctap2EnrollFeedbackNoUserPresenceTransition,
    /// Other possible failure cases that are not (yet) defined by the spec
    Ctap2EnrollFeedbackOther(u8),
}

impl<'de> Deserialize<'de> for LastEnrollmentSampleStatus {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct LastEnrollmentSampleStatusVisitor;

        impl<'de> Visitor<'de> for LastEnrollmentSampleStatusVisitor {
            type Value = LastEnrollmentSampleStatus;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("an integer")
            }

            fn visit_u8<E>(self, v: u8) -> Result<Self::Value, E>
            where
                E: SerdeError,
            {
                match v {
                    0x00 => Ok(LastEnrollmentSampleStatus::Ctap2EnrollFeedbackFpGood),
                    0x01 => Ok(LastEnrollmentSampleStatus::Ctap2EnrollFeedbackFpTooHigh),
                    0x02 => Ok(LastEnrollmentSampleStatus::Ctap2EnrollFeedbackFpTooLow),
                    0x03 => Ok(LastEnrollmentSampleStatus::Ctap2EnrollFeedbackFpTooLeft),
                    0x04 => Ok(LastEnrollmentSampleStatus::Ctap2EnrollFeedbackFpTooRight),
                    0x05 => Ok(LastEnrollmentSampleStatus::Ctap2EnrollFeedbackFpTooFast),
                    0x06 => Ok(LastEnrollmentSampleStatus::Ctap2EnrollFeedbackFpTooSlow),
                    0x07 => Ok(LastEnrollmentSampleStatus::Ctap2EnrollFeedbackFpPoorQuality),
                    0x08 => Ok(LastEnrollmentSampleStatus::Ctap2EnrollFeedbackFpTooSkewed),
                    0x09 => Ok(LastEnrollmentSampleStatus::Ctap2EnrollFeedbackFpTooShort),
                    0x0A => Ok(LastEnrollmentSampleStatus::Ctap2EnrollFeedbackFpMergeFailure),
                    0x0B => Ok(LastEnrollmentSampleStatus::Ctap2EnrollFeedbackFpExists),
                    0x0C => Ok(LastEnrollmentSampleStatus::Unused),
                    0x0D => Ok(LastEnrollmentSampleStatus::Ctap2EnrollFeedbackNoUserActivity),
                    0x0E => {
                        Ok(LastEnrollmentSampleStatus::Ctap2EnrollFeedbackNoUserPresenceTransition)
                    }
                    x => Ok(LastEnrollmentSampleStatus::Ctap2EnrollFeedbackOther(x)),
                }
            }
        }

        deserializer.deserialize_u8(LastEnrollmentSampleStatusVisitor)
    }
}

#[derive(Debug, Copy, Clone, Serialize)]
pub enum FingerprintKind {
    TouchSensor,
    SwipeSensor,
    // Not (yet) defined by the spec
    Other(u8),
}

impl<'de> Deserialize<'de> for FingerprintKind {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct FingerprintKindVisitor;

        impl<'de> Visitor<'de> for FingerprintKindVisitor {
            type Value = FingerprintKind;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("an integer")
            }

            fn visit_u8<E>(self, v: u8) -> Result<Self::Value, E>
            where
                E: SerdeError,
            {
                match v {
                    0x01 => Ok(FingerprintKind::TouchSensor),
                    0x02 => Ok(FingerprintKind::SwipeSensor),
                    x => Ok(FingerprintKind::Other(x)),
                }
            }
        }

        deserializer.deserialize_u8(FingerprintKindVisitor)
    }
}

#[derive(Debug, Serialize)]
pub(crate) struct BioTemplateInfo {
    template_id: BioTemplateId,
    template_friendly_name: Option<String>,
}

impl<'de> Deserialize<'de> for BioTemplateInfo {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct BioTemplateInfoResponseVisitor;

        impl<'de> Visitor<'de> for BioTemplateInfoResponseVisitor {
            type Value = BioTemplateInfo;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a map")
            }

            fn visit_map<M>(self, mut map: M) -> Result<Self::Value, M::Error>
            where
                M: MapAccess<'de>,
            {
                let mut template_id = None; // (0x01)
                let mut template_friendly_name = None; // (0x02)
                while let Some(key) = map.next_key()? {
                    match key {
                        0x01 => {
                            if template_id.is_some() {
                                return Err(SerdeError::duplicate_field("template_id"));
                            }
                            template_id = Some(map.next_value::<ByteBuf>()?.into_vec());
                        }
                        0x02 => {
                            if template_friendly_name.is_some() {
                                return Err(SerdeError::duplicate_field("template_friendly_name"));
                            }
                            template_friendly_name = Some(map.next_value()?);
                        }
                        k => {
                            warn!("BioTemplateInfo: unexpected key: {:?}", k);
                            let _ = map.next_value::<IgnoredAny>()?;
                            continue;
                        }
                    }
                }

                if let Some(template_id) = template_id {
                    Ok(BioTemplateInfo {
                        template_id,
                        template_friendly_name,
                    })
                } else {
                    Err(SerdeError::missing_field("template_id"))
                }
            }
        }
        deserializer.deserialize_bytes(BioTemplateInfoResponseVisitor)
    }
}

#[derive(Default, Debug)]
pub struct BioEnrollmentResponse {
    /// The user verification modality.
    pub(crate) modality: Option<BioEnrollmentModality>,
    /// Indicates the type of fingerprint sensor. For touch type sensor, its value is 1. For swipe type sensor its value is 2.
    pub(crate) fingerprint_kind: Option<FingerprintKind>,
    /// Indicates the maximum good samples required for enrollment.
    pub(crate) max_capture_samples_required_for_enroll: Option<u64>,
    /// Template Identifier.
    pub(crate) template_id: Option<BioTemplateId>,
    /// Last enrollment sample status.
    pub(crate) last_enroll_sample_status: Option<LastEnrollmentSampleStatus>,
    /// Number of more sample required for enrollment to complete
    pub(crate) remaining_samples: Option<u64>,
    /// Array of templateInfo’s
    pub(crate) template_infos: Vec<BioTemplateInfo>,
    /// Indicates the maximum number of bytes the authenticator will accept as a templateFriendlyName.
    pub(crate) max_template_friendly_name: Option<u64>,
}

impl CtapResponse for BioEnrollmentResponse {}

impl<'de> Deserialize<'de> for BioEnrollmentResponse {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct BioEnrollmentResponseVisitor;

        impl<'de> Visitor<'de> for BioEnrollmentResponseVisitor {
            type Value = BioEnrollmentResponse;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a map")
            }

            fn visit_map<M>(self, mut map: M) -> Result<Self::Value, M::Error>
            where
                M: MapAccess<'de>,
            {
                let mut modality = None; // (0x01)
                let mut fingerprint_kind = None; // (0x02)
                let mut max_capture_samples_required_for_enroll = None; // (0x03)
                let mut template_id = None; // (0x04)
                let mut last_enroll_sample_status = None; // (0x05)
                let mut remaining_samples = None; // (0x06)
                let mut template_infos = None; // (0x07)
                let mut max_template_friendly_name = None; // (0x08)

                while let Some(key) = map.next_key()? {
                    match key {
                        0x01 => {
                            if modality.is_some() {
                                return Err(SerdeError::duplicate_field("modality"));
                            }
                            modality = Some(map.next_value()?);
                        }
                        0x02 => {
                            if fingerprint_kind.is_some() {
                                return Err(SerdeError::duplicate_field("fingerprint_kind"));
                            }
                            fingerprint_kind = Some(map.next_value()?);
                        }
                        0x03 => {
                            if max_capture_samples_required_for_enroll.is_some() {
                                return Err(SerdeError::duplicate_field(
                                    "max_capture_samples_required_for_enroll",
                                ));
                            }
                            max_capture_samples_required_for_enroll = Some(map.next_value()?);
                        }
                        0x04 => {
                            if template_id.is_some() {
                                return Err(SerdeError::duplicate_field("template_id"));
                            }
                            template_id = Some(map.next_value::<ByteBuf>()?.into_vec());
                        }
                        0x05 => {
                            if last_enroll_sample_status.is_some() {
                                return Err(SerdeError::duplicate_field(
                                    "last_enroll_sample_status",
                                ));
                            }
                            last_enroll_sample_status = Some(map.next_value()?);
                        }
                        0x06 => {
                            if remaining_samples.is_some() {
                                return Err(SerdeError::duplicate_field("remaining_samples"));
                            }
                            remaining_samples = Some(map.next_value()?);
                        }
                        0x07 => {
                            if template_infos.is_some() {
                                return Err(SerdeError::duplicate_field("template_infos"));
                            }
                            template_infos = Some(map.next_value()?);
                        }
                        0x08 => {
                            if max_template_friendly_name.is_some() {
                                return Err(SerdeError::duplicate_field(
                                    "max_template_friendly_name",
                                ));
                            }
                            max_template_friendly_name = Some(map.next_value()?);
                        }
                        k => {
                            warn!("BioEnrollmentResponse: unexpected key: {:?}", k);
                            let _ = map.next_value::<IgnoredAny>()?;
                            continue;
                        }
                    }
                }

                Ok(BioEnrollmentResponse {
                    modality,
                    fingerprint_kind,
                    max_capture_samples_required_for_enroll,
                    template_id,
                    last_enroll_sample_status,
                    remaining_samples,
                    template_infos: template_infos.unwrap_or_default(),
                    max_template_friendly_name,
                })
            }
        }
        deserializer.deserialize_bytes(BioEnrollmentResponseVisitor)
    }
}

#[derive(Debug, Serialize)]
pub struct EnrollmentInfo {
    pub template_id: Vec<u8>,
    pub template_friendly_name: Option<String>,
}

impl From<&BioTemplateInfo> for EnrollmentInfo {
    fn from(value: &BioTemplateInfo) -> Self {
        Self {
            template_id: value.template_id.to_vec(),
            template_friendly_name: value.template_friendly_name.clone(),
        }
    }
}

#[derive(Debug, Serialize)]
pub struct FingerprintSensorInfo {
    pub fingerprint_kind: FingerprintKind,
    pub max_capture_samples_required_for_enroll: u64,
    pub max_template_friendly_name: Option<u64>,
}

#[derive(Debug, Serialize)]
pub enum BioEnrollmentResult {
    EnrollmentList(Vec<EnrollmentInfo>),
    DeleteSuccess(AuthenticatorInfo),
    UpdateSuccess,
    AddSuccess(AuthenticatorInfo),
    FingerprintSensorInfo(FingerprintSensorInfo),
    SampleStatus(LastEnrollmentSampleStatus, u64),
}
