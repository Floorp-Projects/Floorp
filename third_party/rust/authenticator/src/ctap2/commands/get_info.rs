use super::{Command, CommandError, CtapResponse, RequestCtap2, StatusCode};
use crate::ctap2::attestation::AAGuid;
use crate::ctap2::server::PublicKeyCredentialParameters;
use crate::transport::errors::HIDError;
use crate::transport::{FidoDevice, VirtualFidoDevice};
use serde::{
    de::{Error as SError, IgnoredAny, MapAccess, Visitor},
    Deserialize, Deserializer, Serialize,
};
use serde_cbor::{de::from_slice, Value};
use std::collections::BTreeMap;
use std::fmt;

#[derive(Debug, Default)]
pub struct GetInfo {}

impl RequestCtap2 for GetInfo {
    type Output = AuthenticatorInfo;

    fn command(&self) -> Command {
        Command::GetInfo
    }

    fn wire_format(&self) -> Result<Vec<u8>, HIDError> {
        Ok(Vec::new())
    }

    fn handle_response_ctap2<Dev: FidoDevice>(
        &self,
        _dev: &mut Dev,
        input: &[u8],
    ) -> Result<Self::Output, HIDError> {
        if input.is_empty() {
            return Err(CommandError::InputTooSmall.into());
        }

        let status: StatusCode = input[0].into();

        if input.len() > 1 {
            if status.is_ok() {
                trace!("parsing authenticator info data: {:#04X?}", &input);
                let authenticator_info =
                    from_slice(&input[1..]).map_err(CommandError::Deserializing)?;
                Ok(authenticator_info)
            } else {
                let data: Value = from_slice(&input[1..]).map_err(CommandError::Deserializing)?;
                Err(CommandError::StatusCode(status, Some(data)).into())
            }
        } else {
            Err(CommandError::InputTooSmall.into())
        }
    }

    fn send_to_virtual_device<Dev: VirtualFidoDevice>(
        &self,
        dev: &mut Dev,
    ) -> Result<Self::Output, HIDError> {
        dev.get_info()
    }
}

fn true_val() -> bool {
    true
}

#[derive(Debug, Deserialize, Clone, Eq, PartialEq, Serialize)]
pub struct AuthenticatorOptions {
    /// Indicates that the device is attached to the client and therefore can’t
    /// be removed and used on another client.
    #[serde(rename = "plat", default)]
    pub platform_device: bool,
    /// Indicates that the device is capable of storing keys on the device
    /// itself and therefore can satisfy the authenticatorGetAssertion request
    /// with allowList parameter not specified or empty.
    #[serde(rename = "rk", default)]
    pub resident_key: bool,

    /// Client PIN:
    ///  If present and set to true, it indicates that the device is capable of
    ///   accepting a PIN from the client and PIN has been set.
    ///  If present and set to false, it indicates that the device is capable of
    ///   accepting a PIN from the client and PIN has not been set yet.
    ///  If absent, it indicates that the device is not capable of accepting a
    ///   PIN from the client.
    /// Client PIN is one of the ways to do user verification.
    #[serde(rename = "clientPin")]
    pub client_pin: Option<bool>,

    /// Indicates that the device is capable of testing user presence.
    #[serde(rename = "up", default = "true_val")]
    pub user_presence: bool,

    /// Indicates that the device is capable of verifying the user within
    /// itself. For example, devices with UI, biometrics fall into this
    /// category.
    ///  If present and set to true, it indicates that the device is capable of
    ///   user verification within itself and has been configured.
    ///  If present and set to false, it indicates that the device is capable of
    ///   user verification within itself and has not been yet configured. For
    ///   example, a biometric device that has not yet been configured will
    ///   return this parameter set to false.
    ///  If absent, it indicates that the device is not capable of user
    ///   verification within itself.
    /// A device that can only do Client PIN will not return the "uv" parameter.
    /// If a device is capable of verifying the user within itself as well as
    /// able to do Client PIN, it will return both "uv" and the Client PIN
    /// option.
    // TODO(MS): My Token (key-ID FIDO2) does return Some(false) here, even though
    //           it has no built-in verification method. Not to be trusted...
    #[serde(rename = "uv")]
    pub user_verification: Option<bool>,

    // ----------------------------------------------------
    // CTAP 2.1 options
    // ----------------------------------------------------
    /// If pinUvAuthToken is:
    /// present and set to true
    ///  if the clientPin option id is present and set to true, then the
    ///  authenticator supports authenticatorClientPIN's getPinUvAuthTokenUsingPinWithPermissions
    ///  subcommand. If the uv option id is present and set to true, then
    ///  the authenticator supports authenticatorClientPIN's getPinUvAuthTokenUsingUvWithPermissions
    ///  subcommand.
    /// present and set to false, or absent.
    ///  the authenticator does not support authenticatorClientPIN's
    ///  getPinUvAuthTokenUsingPinWithPermissions and getPinUvAuthTokenUsingUvWithPermissions
    ///  subcommands.
    #[serde(rename = "pinUvAuthToken")]
    pub pin_uv_auth_token: Option<bool>,

    /// If this noMcGaPermissionsWithClientPin is:
    /// present and set to true
    ///  A pinUvAuthToken obtained via getPinUvAuthTokenUsingPinWithPermissions
    ///  (or getPinToken) cannot be used for authenticatorMakeCredential or
    ///  authenticatorGetAssertion commands, because it will lack the necessary
    ///  mc and ga permissions. In this situation, platforms SHOULD NOT attempt
    ///  to use getPinUvAuthTokenUsingPinWithPermissions if using
    ///  getPinUvAuthTokenUsingUvWithPermissions fails.
    /// present and set to false, or absent.
    ///  A pinUvAuthToken obtained via getPinUvAuthTokenUsingPinWithPermissions
    ///  (or getPinToken) can be used for authenticatorMakeCredential or
    ///  authenticatorGetAssertion commands.
    /// Note: noMcGaPermissionsWithClientPin MUST only be present if the
    ///       clientPin option ID is present.
    #[serde(rename = "noMcGaPermissionsWithClientPin")]
    pub no_mc_ga_permissions_with_client_pin: Option<bool>,

    /// If largeBlobs is:
    /// present and set to true
    ///  the authenticator supports the authenticatorLargeBlobs command.
    /// present and set to false, or absent.
    ///  The authenticatorLargeBlobs command is NOT supported.
    #[serde(rename = "largeBlobs")]
    pub large_blobs: Option<bool>,

    /// Enterprise Attestation feature support:
    /// If ep is:
    /// Present and set to true
    ///  The authenticator is enterprise attestation capable, and enterprise
    ///  attestation is enabled.
    /// Present and set to false
    ///  The authenticator is enterprise attestation capable, and enterprise
    ///  attestation is disabled.
    /// Absent
    ///  The Enterprise Attestation feature is NOT supported.
    #[serde(rename = "ep")]
    pub ep: Option<bool>,

    /// If bioEnroll is:
    /// present and set to true
    ///  the authenticator supports the authenticatorBioEnrollment commands,
    ///  and has at least one bio enrollment presently provisioned.
    /// present and set to false
    ///  the authenticator supports the authenticatorBioEnrollment commands,
    ///  and does not yet have any bio enrollments provisioned.
    /// absent
    ///  the authenticatorBioEnrollment commands are NOT supported.
    #[serde(rename = "bioEnroll")]
    pub bio_enroll: Option<bool>,

    /// "FIDO_2_1_PRE" Prototype Credential management support:
    /// If userVerificationMgmtPreview is:
    /// present and set to true
    ///  the authenticator supports the Prototype authenticatorBioEnrollment (0x41)
    ///  commands, and has at least one bio enrollment presently provisioned.
    /// present and set to false
    ///  the authenticator supports the Prototype authenticatorBioEnrollment (0x41)
    ///  commands, and does not yet have any bio enrollments provisioned.
    /// absent
    ///  the Prototype authenticatorBioEnrollment (0x41) commands are not supported.
    #[serde(rename = "userVerificationMgmtPreview")]
    pub user_verification_mgmt_preview: Option<bool>,

    /// getPinUvAuthTokenUsingUvWithPermissions support for requesting the be permission:
    /// This option ID MUST only be present if bioEnroll is also present.
    /// If uvBioEnroll is:
    /// present and set to true
    ///  requesting the be permission when invoking getPinUvAuthTokenUsingUvWithPermissions
    ///  is supported.
    /// present and set to false, or absent.
    ///  requesting the be permission when invoking getPinUvAuthTokenUsingUvWithPermissions
    ///  is NOT supported.
    #[serde(rename = "uvBioEnroll")]
    pub uv_bio_enroll: Option<bool>,

    /// authenticatorConfig command support:
    /// If authnrCfg is:
    /// present and set to true
    ///  the authenticatorConfig command is supported.
    /// present and set to false, or absent.
    ///  the authenticatorConfig command is NOT supported.
    #[serde(rename = "authnrCfg")]
    pub authnr_cfg: Option<bool>,

    /// getPinUvAuthTokenUsingUvWithPermissions support for requesting the acfg permission:
    /// This option ID MUST only be present if authnrCfg is also present.
    /// If uvAcfg is:
    /// present and set to true
    ///  requesting the acfg permission when invoking getPinUvAuthTokenUsingUvWithPermissions
    ///  is supported.
    /// present and set to false, or absent.
    ///  requesting the acfg permission when invoking getPinUvAuthTokenUsingUvWithPermissions
    ///  is NOT supported.
    #[serde(rename = "uvAcfg")]
    pub uv_acfg: Option<bool>,

    /// Credential management support:
    /// If credMgmt is:
    /// present and set to true
    ///  the authenticatorCredentialManagement command is supported.
    /// present and set to false, or absent.
    ///  the authenticatorCredentialManagement command is NOT supported.
    #[serde(rename = "credMgmt")]
    pub cred_mgmt: Option<bool>,

    /// "FIDO_2_1_PRE" Prototype Credential management support:
    /// If credentialMgmtPreview is:
    /// present and set to true
    ///  the Prototype authenticatorCredentialManagement (0x41) command is supported.
    /// present and set to false, or absent.
    ///  the Prototype authenticatorCredentialManagement (0x41) command is NOT supported.
    #[serde(rename = "credentialMgmtPreview")]
    pub credential_mgmt_preview: Option<bool>,

    /// Support for the Set Minimum PIN Length feature.
    /// If setMinPINLength is:
    /// present and set to true
    ///  the setMinPINLength subcommand is supported.
    /// present and set to false, or absent.
    ///  the setMinPINLength subcommand is NOT supported.
    /// Note: setMinPINLength MUST only be present if the clientPin option ID is present.
    #[serde(rename = "setMinPINLength")]
    pub set_min_pin_length: Option<bool>,

    /// Support for making non-discoverable credentials without requiring User Verification.
    /// If makeCredUvNotRqd is:
    /// present and set to true
    ///  the authenticator allows creation of non-discoverable credentials without
    ///  requiring any form of user verification, if the platform requests this behaviour.
    /// present and set to false, or absent.
    ///  the authenticator requires some form of user verification for creating
    ///  non-discoverable credentials, regardless of the parameters the platform supplies
    ///  for the authenticatorMakeCredential command.
    /// Authenticators SHOULD include this option with the value true.
    #[serde(rename = "makeCredUvNotRqd")]
    pub make_cred_uv_not_rqd: Option<bool>,

    /// Support for the Always Require User Verification feature:
    /// If alwaysUv is
    /// present and set to true
    ///  the authenticator supports the Always Require User Verification feature and it is enabled.
    /// present and set to false
    ///  the authenticator supports the Always Require User Verification feature but it is disabled.
    /// absent
    ///  the authenticator does not support the Always Require User Verification feature.
    /// Note: If the alwaysUv option ID is present and true the authenticator MUST set the value
    ///       of makeCredUvNotRqd to false.
    #[serde(rename = "alwaysUv")]
    pub always_uv: Option<bool>,
}

impl Default for AuthenticatorOptions {
    fn default() -> Self {
        AuthenticatorOptions {
            platform_device: false,
            resident_key: false,
            client_pin: None,
            user_presence: true,
            user_verification: None,
            pin_uv_auth_token: None,
            no_mc_ga_permissions_with_client_pin: None,
            large_blobs: None,
            ep: None,
            bio_enroll: None,
            user_verification_mgmt_preview: None,
            uv_bio_enroll: None,
            authnr_cfg: None,
            uv_acfg: None,
            cred_mgmt: None,
            credential_mgmt_preview: None,
            set_min_pin_length: None,
            make_cred_uv_not_rqd: None,
            always_uv: None,
        }
    }
}

#[allow(non_camel_case_types)]
#[derive(Clone, Debug, Eq, PartialEq, Serialize, Deserialize)]
pub enum AuthenticatorVersion {
    U2F_V2,
    FIDO_2_0,
    FIDO_2_1_PRE,
    FIDO_2_1,
}

#[derive(Clone, Debug, Default, Eq, PartialEq, Serialize)]
pub struct AuthenticatorInfo {
    pub versions: Vec<AuthenticatorVersion>,
    pub extensions: Vec<String>,
    pub aaguid: AAGuid,
    pub options: AuthenticatorOptions,
    pub max_msg_size: Option<usize>,
    pub pin_protocols: Option<Vec<u64>>,
    // CTAP 2.1
    pub max_credential_count_in_list: Option<usize>,
    pub max_credential_id_length: Option<usize>,
    pub transports: Option<Vec<String>>,
    pub algorithms: Option<Vec<PublicKeyCredentialParameters>>,
    pub max_ser_large_blob_array: Option<u64>,
    pub force_pin_change: Option<bool>,
    pub min_pin_length: Option<u64>,
    pub firmware_version: Option<u64>,
    pub max_cred_blob_length: Option<u64>,
    pub max_rpids_for_set_min_pin_length: Option<u64>,
    pub preferred_platform_uv_attempts: Option<u64>,
    pub uv_modality: Option<u64>,
    pub certifications: Option<BTreeMap<String, u64>>,
    pub remaining_discoverable_credentials: Option<u64>,
    pub vendor_prototype_config_commands: Option<Vec<u64>>,
}

impl AuthenticatorInfo {
    pub fn supports_cred_protect(&self) -> bool {
        self.extensions.contains(&"credProtect".to_string())
    }

    pub fn supports_hmac_secret(&self) -> bool {
        self.extensions.contains(&"hmac-secret".to_string())
    }

    pub fn max_supported_version(&self) -> AuthenticatorVersion {
        let versions = vec![
            AuthenticatorVersion::FIDO_2_1,
            AuthenticatorVersion::FIDO_2_1_PRE,
            AuthenticatorVersion::FIDO_2_0,
            AuthenticatorVersion::U2F_V2,
        ];
        for ver in versions {
            if self.versions.contains(&ver) {
                return ver;
            }
        }
        AuthenticatorVersion::U2F_V2
    }

    pub fn device_is_protected(&self) -> bool {
        self.options.client_pin == Some(true) || self.options.user_verification == Some(true)
    }
}

impl CtapResponse for AuthenticatorInfo {}

macro_rules! parse_next_optional_value {
    ($name:expr, $map:expr) => {
        if $name.is_some() {
            return Err(serde::de::Error::duplicate_field("$name"));
        }
        $name = Some($map.next_value()?);
    };
}

impl<'de> Deserialize<'de> for AuthenticatorInfo {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct AuthenticatorInfoVisitor;

        impl<'de> Visitor<'de> for AuthenticatorInfoVisitor {
            type Value = AuthenticatorInfo;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a byte array")
            }

            fn visit_map<M>(self, mut map: M) -> Result<Self::Value, M::Error>
            where
                M: MapAccess<'de>,
            {
                let mut versions = Vec::new();
                let mut extensions = Vec::new();
                let mut aaguid = None;
                let mut options = AuthenticatorOptions::default();
                let mut max_msg_size = None;
                let mut pin_protocols: Option<Vec<_>> = None;
                let mut max_credential_count_in_list = None;
                let mut max_credential_id_length = None;
                let mut transports = None;
                let mut algorithms = None;
                let mut max_ser_large_blob_array = None;
                let mut force_pin_change = None;
                let mut min_pin_length = None;
                let mut firmware_version = None;
                let mut max_cred_blob_length = None;
                let mut max_rpids_for_set_min_pin_length = None;
                let mut preferred_platform_uv_attempts = None;
                let mut uv_modality = None;
                let mut certifications = None;
                let mut remaining_discoverable_credentials = None;
                let mut vendor_prototype_config_commands = None;
                while let Some(key) = map.next_key()? {
                    match key {
                        0x01 => {
                            if !versions.is_empty() {
                                return Err(serde::de::Error::duplicate_field("versions"));
                            }
                            versions = map.next_value()?;
                        }
                        0x02 => {
                            if !extensions.is_empty() {
                                return Err(serde::de::Error::duplicate_field("extensions"));
                            }
                            extensions = map.next_value()?;
                        }
                        0x03 => {
                            parse_next_optional_value!(aaguid, map);
                        }
                        0x04 => {
                            options = map.next_value()?;
                        }
                        0x05 => {
                            parse_next_optional_value!(max_msg_size, map);
                        }
                        0x06 => {
                            parse_next_optional_value!(pin_protocols, map);
                        }
                        0x07 => {
                            parse_next_optional_value!(max_credential_count_in_list, map);
                        }
                        0x08 => {
                            parse_next_optional_value!(max_credential_id_length, map);
                        }
                        0x09 => {
                            parse_next_optional_value!(transports, map);
                        }
                        0x0a => {
                            parse_next_optional_value!(algorithms, map);
                        }
                        0x0b => {
                            parse_next_optional_value!(max_ser_large_blob_array, map);
                        }
                        0x0c => {
                            parse_next_optional_value!(force_pin_change, map);
                        }
                        0x0d => {
                            parse_next_optional_value!(min_pin_length, map);
                        }
                        0x0e => {
                            parse_next_optional_value!(firmware_version, map);
                        }
                        0x0f => {
                            parse_next_optional_value!(max_cred_blob_length, map);
                        }
                        0x10 => {
                            parse_next_optional_value!(max_rpids_for_set_min_pin_length, map);
                        }
                        0x11 => {
                            parse_next_optional_value!(preferred_platform_uv_attempts, map);
                        }
                        0x12 => {
                            parse_next_optional_value!(uv_modality, map);
                        }
                        0x13 => {
                            parse_next_optional_value!(certifications, map);
                        }
                        0x14 => {
                            parse_next_optional_value!(remaining_discoverable_credentials, map);
                        }
                        0x15 => {
                            parse_next_optional_value!(vendor_prototype_config_commands, map);
                        }
                        k => {
                            warn!("GetInfo: unexpected key: {:?}", k);
                            let _ = map.next_value::<IgnoredAny>()?;
                            continue;
                        }
                    }
                }

                if versions.is_empty() {
                    return Err(M::Error::custom(
                        "expected at least one version, got none".to_string(),
                    ));
                }

                if let Some(protocols) = &pin_protocols {
                    if protocols.is_empty() {
                        return Err(M::Error::custom(
                            "Token returned empty PIN protocol list, which is not allowed"
                                .to_string(),
                        ));
                    }
                }

                if let Some(aaguid) = aaguid {
                    Ok(AuthenticatorInfo {
                        versions,
                        extensions,
                        aaguid,
                        options,
                        max_msg_size,
                        pin_protocols,
                        max_credential_count_in_list,
                        max_credential_id_length,
                        transports,
                        algorithms,
                        max_ser_large_blob_array,
                        force_pin_change,
                        min_pin_length,
                        firmware_version,
                        max_cred_blob_length,
                        max_rpids_for_set_min_pin_length,
                        preferred_platform_uv_attempts,
                        uv_modality,
                        certifications,
                        remaining_discoverable_credentials,
                        vendor_prototype_config_commands,
                    })
                } else {
                    Err(M::Error::custom("No AAGuid specified".to_string()))
                }
            }
        }

        deserializer.deserialize_bytes(AuthenticatorInfoVisitor)
    }
}

#[cfg(test)]
pub mod tests {
    use super::*;
    use crate::consts::{Capability, HIDCmd, CID_BROADCAST};
    use crate::crypto::COSEAlgorithm;
    use crate::transport::device_selector::Device;
    use crate::transport::platform::device::IN_HID_RPT_SIZE;
    use crate::transport::{hid::HIDDevice, FidoDevice, FidoProtocol};
    use rand::{thread_rng, RngCore};
    use serde_cbor::de::from_slice;

    // Raw data take from https://github.com/Yubico/python-fido2/blob/master/test/test_ctap2.py
    pub const AAGUID_RAW: [u8; 16] = [
        0xF8, 0xA0, 0x11, 0xF3, 0x8C, 0x0A, 0x4D, 0x15, 0x80, 0x06, 0x17, 0x11, 0x1F, 0x9E, 0xDC,
        0x7D,
    ];

    pub const AUTHENTICATOR_INFO_PAYLOAD: [u8; 89] = [
        0xa6, // map(6)
        0x01, //   unsigned(1)
        0x82, //   array(2)
        0x66, //     text(6)
        0x55, 0x32, 0x46, 0x5f, 0x56, 0x32, // "U2F_V2"
        0x68, //     text(8)
        0x46, 0x49, 0x44, 0x4f, 0x5f, 0x32, 0x5f, 0x30, //  "FIDO_2_0"
        0x02, //   unsigned(2)
        0x82, //   array(2)
        0x63, //     text(3)
        0x75, 0x76, 0x6d, // "uvm"
        0x6b, //     text(11)
        0x68, 0x6d, 0x61, 0x63, 0x2d, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74, // "hmac-secret"
        0x03, //  unsigned(3)
        0x50, //  bytes(16)
        0xf8, 0xa0, 0x11, 0xf3, 0x8c, 0x0a, 0x4d, 0x15, 0x80, 0x06, 0x17, 0x11, 0x1f, 0x9e, 0xdc,
        0x7d, // "\xF8\xA0\u0011\xF3\x8C\nM\u0015\x80\u0006\u0017\u0011\u001F\x9E\xDC}"
        0x04, //  unsigned(4)
        0xa4, //  map(4)
        0x62, //    text(2)
        0x72, 0x6b, //  "rk"
        0xf5, //    primitive(21)
        0x62, //    text(2)
        0x75, 0x70, // "up"
        0xf5, //    primitive(21)
        0x64, //    text(4)
        0x70, 0x6c, 0x61, 0x74, // "plat"
        0xf4, //    primitive(20)
        0x69, //    text(9)
        0x63, 0x6c, 0x69, 0x65, 0x6e, 0x74, 0x50, 0x69, 0x6e, // "clientPin"
        0xf4, //    primitive(20)
        0x05, //  unsigned(5)
        0x19, 0x04, 0xb0, // unsigned(1200)
        0x06, //  unsigned(6)
        0x81, //  array(1)
        0x01, //    unsigned(1)
    ];

    // Real world example from Yubikey Bio
    pub const AUTHENTICATOR_INFO_PAYLOAD_YK_BIO_5C: [u8; 409] = [
        0xB3, // map(19)
        0x01, //   unsigned(1)
        0x84, //     array(4)
        0x66, //       text(6)
        0x55, 0x32, 0x46, 0x5F, 0x56, 0x32, // "U2F_V2"
        0x68, //       text(8)
        0x46, 0x49, 0x44, 0x4F, 0x5F, 0x32, 0x5F, 0x30, // "FIDO_2_0"
        0x6C, //       text(12)
        0x46, 0x49, 0x44, 0x4F, 0x5F, 0x32, 0x5F, 0x31, 0x5F, 0x50, 0x52,
        0x45, //         "FIDO_2_1_PRE"
        0x68, //       text(8)
        0x46, 0x49, 0x44, 0x4F, 0x5F, 0x32, 0x5F, 0x31, // "FIDO_2_1"
        0x02, //   unsigned(2)
        0x85, //   array(5)
        0x6B, //     text(11)
        0x63, 0x72, 0x65, 0x64, 0x50, 0x72, 0x6F, 0x74, 0x65, 0x63, 0x74, // "credProtect"
        0x6B, //     text(11)
        0x68, 0x6D, 0x61, 0x63, 0x2D, 0x73, 0x65, 0x63, 0x72, 0x65, 0x74, // "hmac-secret"
        0x6C, //     text(12)
        0x6C, 0x61, 0x72, 0x67, 0x65, 0x42, 0x6C, 0x6F, 0x62, 0x4B, 0x65,
        0x79, //       "largeBlobKey"
        0x68, //     text(8)
        0x63, 0x72, 0x65, 0x64, 0x42, 0x6C, 0x6F, 0x62, // "credBlob"
        0x6C, //     text(12)
        0x6D, 0x69, 0x6E, 0x50, 0x69, 0x6E, 0x4C, 0x65, 0x6E, 0x67, 0x74,
        0x68, //       "minPinLength"
        0x03, //   unsigned(3)
        0x50, //   bytes(16)
        0xD8, 0x52, 0x2D, 0x9F, 0x57, 0x5B, 0x48, 0x66, 0x88, 0xA9, 0xBA, 0x99, 0xFA, 0x02, 0xF3,
        0x5B, //     "\xD8R-\x9FW[Hf\x88\xA9\xBA\x99\xFA\u0002\xF3["
        0x04, //   unsigned(4)
        0xB0, //   map(16)
        0x62, //     text(2)
        0x72, 0x6B, // "rk"
        0xF5, //     primitive(21)
        0x62, //     text(2)
        0x75, 0x70, // "up"
        0xF5, //     primitive(21)
        0x62, //     text(2)
        0x75, 0x76, // "uv"
        0xF5, //     primitive(21)
        0x64, //     text(4)
        0x70, 0x6C, 0x61, 0x74, // "plat"
        0xF4, //     primitive(20)
        0x67, //     text(7)
        0x75, 0x76, 0x54, 0x6F, 0x6B, 0x65, 0x6E, // "uvToken"
        0xF5, //     primitive(21)
        0x68, //     text(8)
        0x61, 0x6C, 0x77, 0x61, 0x79, 0x73, 0x55, 0x76, // "alwaysUv"
        0xF5, //     primitive(21)
        0x68, //     text(8)
        0x63, 0x72, 0x65, 0x64, 0x4D, 0x67, 0x6D, 0x74, // "credMgmt"
        0xF5, //     primitive(21)
        0x69, //     text(9)
        0x61, 0x75, 0x74, 0x68, 0x6E, 0x72, 0x43, 0x66, 0x67, // "authnrCfg"
        0xF5, //     primitive(21)
        0x69, //     text(9)
        0x62, 0x69, 0x6F, 0x45, 0x6E, 0x72, 0x6F, 0x6C, 0x6C, // "bioEnroll"
        0xF5, //     primitive(21)
        0x69, //     text(9)
        0x63, 0x6C, 0x69, 0x65, 0x6E, 0x74, 0x50, 0x69, 0x6E, // "clientPin"
        0xF5, //     primitive(21)
        0x6A, //     text(10)
        0x6C, 0x61, 0x72, 0x67, 0x65, 0x42, 0x6C, 0x6F, 0x62, 0x73, // "largeBlobs"
        0xF5, //     primitive(21)
        0x6E, //     text(14)
        0x70, 0x69, 0x6E, 0x55, 0x76, 0x41, 0x75, 0x74, 0x68, 0x54, 0x6F, 0x6B, 0x65,
        0x6E, //       "pinUvAuthToken"
        0xF5, //     primitive(21)
        0x6F, //     text(15)
        0x73, 0x65, 0x74, 0x4D, 0x69, 0x6E, 0x50, 0x49, 0x4E, 0x4C, 0x65, 0x6E, 0x67, 0x74,
        0x68, //       "setMinPINLength"
        0xF5, //     primitive(21)
        0x70, //     text(16)
        0x6D, 0x61, 0x6B, 0x65, 0x43, 0x72, 0x65, 0x64, 0x55, 0x76, 0x4E, 0x6F, 0x74, 0x52, 0x71,
        0x64, //       "makeCredUvNotRqd"
        0xF4, //     primitive(20)
        0x75, //     text(21)
        0x63, 0x72, 0x65, 0x64, 0x65, 0x6E, 0x74, 0x69, 0x61, 0x6C, 0x4D, 0x67, 0x6D, 0x74, 0x50,
        0x72, 0x65, 0x76, 0x69, 0x65, 0x77, // "credentialMgmtPreview"
        0xF5, //     primitive(21)
        0x78, 0x1B, // text(27)
        0x75, 0x73, 0x65, 0x72, 0x56, 0x65, 0x72, 0x69, 0x66, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6F,
        0x6E, 0x4D, 0x67, 0x6D, 0x74, 0x50, 0x72, 0x65, 0x76, 0x69, 0x65,
        0x77, //       "userVerificationMgmtPreview"
        0xF5, //     primitive(21)
        0x05, //   unsigned(5)
        0x19, 0x04, 0xB0, // unsigned(1200)
        0x06, //   unsigned(6)
        0x82, //   array(2)
        0x02, //     unsigned(2)
        0x01, //     unsigned(1)
        0x07, //   unsigned(7)
        0x08, //   unsigned(8)
        0x08, //   unsigned(8)
        0x18, 0x80, // unsigned(128)
        0x09, //   unsigned(9)
        0x81, //   array(1)
        0x63, //     text(3)
        0x75, 0x73, 0x62, // "usb"
        0x0A, //   unsigned(10)
        0x82, //   array(2)
        0xA2, //     map(2)
        0x63, //       text(3)
        0x61, 0x6C, 0x67, // "alg"
        0x26, //     negative(6)
        0x64, //     text(4)
        0x74, 0x79, 0x70, 0x65, // "type"
        0x6A, //     text(10)
        0x70, 0x75, 0x62, 0x6C, 0x69, 0x63, 0x2D, 0x6B, 0x65, 0x79, // "public-key"
        0xA2, //     map(2)
        0x63, //       text(3)
        0x61, 0x6C, 0x67, // "alg"
        0x27, //     negative(7)
        0x64, //     text(4)
        0x74, 0x79, 0x70, 0x65, // "type"
        0x6A, //     text(10)
        0x70, 0x75, 0x62, 0x6C, 0x69, 0x63, 0x2D, 0x6B, 0x65, 0x79, // "public-key"
        0x0B, //  unsigned(11)
        0x19, 0x04, 0x00, // unsigned(1024)
        0x0C, //   unsigned(12)
        0xF4, //   primitive(20)
        0x0D, //   unsigned(13)
        0x04, //   unsigned(4)
        0x0E, //   unsigned(14)
        0x1A, 0x00, 0x05, 0x05, 0x06, // unsigned(328966)
        0x0F, //   unsigned(15)
        0x18, 0x20, // unsigned(32)
        0x10, //   unsigned(16)
        0x01, //   unsigned(1)
        0x11, //   unsigned(17)
        0x03, //   unsigned(3)
        0x12, //   unsigned(18)
        0x02, //   unsigned(2)
        0x14, //   unsigned(20)
        0x18, 0x18, // unsigned(24)
    ];

    #[test]
    fn parse_authenticator_info() {
        let authenticator_info: AuthenticatorInfo =
            from_slice(&AUTHENTICATOR_INFO_PAYLOAD).unwrap();

        let expected = AuthenticatorInfo {
            versions: vec![AuthenticatorVersion::U2F_V2, AuthenticatorVersion::FIDO_2_0],
            extensions: vec!["uvm".to_string(), "hmac-secret".to_string()],
            aaguid: AAGuid(AAGUID_RAW),
            options: AuthenticatorOptions {
                platform_device: false,
                resident_key: true,
                client_pin: Some(false),
                user_presence: true,
                user_verification: None,
                ..Default::default()
            },
            max_msg_size: Some(1200),
            pin_protocols: Some(vec![1]),
            max_credential_count_in_list: None,
            max_credential_id_length: None,
            transports: None,
            algorithms: None,
            max_ser_large_blob_array: None,
            force_pin_change: None,
            min_pin_length: None,
            firmware_version: None,
            max_cred_blob_length: None,
            max_rpids_for_set_min_pin_length: None,
            preferred_platform_uv_attempts: None,
            uv_modality: None,
            certifications: None,
            remaining_discoverable_credentials: None,
            vendor_prototype_config_commands: None,
        };

        assert_eq!(authenticator_info, expected);

        // Test broken auth info
        let mut broken_payload = AUTHENTICATOR_INFO_PAYLOAD.to_vec();
        // Have one more entry in the map
        broken_payload[0] += 1;
        // Add the additional entry at the back with an invalid key
        broken_payload.extend_from_slice(&[
            0x17, // unsigned(23) -> invalid key-number. CTAP2.1 goes only to 0x15
            0x6B, //   text(11)
            0x69, 0x6E, 0x76, 0x61, 0x6C, 0x69, 0x64, 0x5F, 0x6B, 0x65, 0x79, // "invalid_key"
        ]);

        let authenticator_info: AuthenticatorInfo = from_slice(&broken_payload).unwrap();
        assert_eq!(authenticator_info, expected);
    }

    #[test]
    fn parse_authenticator_info_yk_bio_5c() {
        let authenticator_info: AuthenticatorInfo =
            from_slice(&AUTHENTICATOR_INFO_PAYLOAD_YK_BIO_5C).unwrap();

        let expected = AuthenticatorInfo {
            versions: vec![
                AuthenticatorVersion::U2F_V2,
                AuthenticatorVersion::FIDO_2_0,
                AuthenticatorVersion::FIDO_2_1_PRE,
                AuthenticatorVersion::FIDO_2_1,
            ],
            extensions: vec![
                "credProtect".to_string(),
                "hmac-secret".to_string(),
                "largeBlobKey".to_string(),
                "credBlob".to_string(),
                "minPinLength".to_string(),
            ],
            aaguid: AAGuid([
                0xd8, 0x52, 0x2d, 0x9f, 0x57, 0x5b, 0x48, 0x66, 0x88, 0xa9, 0xba, 0x99, 0xfa, 0x02,
                0xf3, 0x5b,
            ]),
            options: AuthenticatorOptions {
                platform_device: false,
                resident_key: true,
                client_pin: Some(true),
                user_presence: true,
                user_verification: Some(true),
                pin_uv_auth_token: Some(true),
                no_mc_ga_permissions_with_client_pin: None,
                large_blobs: Some(true),
                ep: None,
                bio_enroll: Some(true),
                user_verification_mgmt_preview: Some(true),
                uv_bio_enroll: None,
                authnr_cfg: Some(true),
                uv_acfg: None,
                cred_mgmt: Some(true),
                credential_mgmt_preview: Some(true),
                set_min_pin_length: Some(true),
                make_cred_uv_not_rqd: Some(false),
                always_uv: Some(true),
            },
            max_msg_size: Some(1200),
            pin_protocols: Some(vec![2, 1]),
            max_credential_count_in_list: Some(8),
            max_credential_id_length: Some(128),
            transports: Some(vec!["usb".to_string()]),
            algorithms: Some(vec![
                PublicKeyCredentialParameters {
                    alg: COSEAlgorithm::ES256,
                },
                PublicKeyCredentialParameters {
                    alg: COSEAlgorithm::EDDSA,
                },
            ]),
            max_ser_large_blob_array: Some(1024),
            force_pin_change: Some(false),
            min_pin_length: Some(4),
            firmware_version: Some(328966),
            max_cred_blob_length: Some(32),
            max_rpids_for_set_min_pin_length: Some(1),
            preferred_platform_uv_attempts: Some(3),
            uv_modality: Some(2),
            certifications: None,
            remaining_discoverable_credentials: Some(24),
            vendor_prototype_config_commands: None,
        };

        assert_eq!(authenticator_info, expected);
    }

    #[test]
    fn test_get_info_ctap2_only() {
        let mut device = Device::new("commands/get_info").unwrap();
        assert_eq!(device.get_protocol(), FidoProtocol::CTAP2);
        let nonce = [0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01];

        // channel id
        let mut cid = [0u8; 4];
        thread_rng().fill_bytes(&mut cid);

        // init packet
        let mut msg = CID_BROADCAST.to_vec();
        msg.extend(vec![HIDCmd::Init.into(), 0x00, 0x08]); // cmd + bcnt
        msg.extend_from_slice(&nonce);
        device.add_write(&msg, 0);

        // init_resp packet
        let mut msg = CID_BROADCAST.to_vec();
        msg.extend(vec![
            0x06, /* HIDCmd::Init without TYPE_INIT */
            0x00, 0x11,
        ]); // cmd + bcnt
        msg.extend_from_slice(&nonce);
        msg.extend_from_slice(&cid); // new channel id

        // We are setting NMSG, to signal that the device does not support CTAP1
        msg.extend(vec![0x02, 0x04, 0x01, 0x08, 0x01 | 0x04 | 0x08]); // versions + flags (wink+cbor+nmsg)
        device.add_read(&msg, 0);

        // ctap2 request
        let mut msg = cid.to_vec();
        msg.extend(vec![HIDCmd::Cbor.into(), 0x00, 0x1]); // cmd + bcnt
        msg.extend(vec![0x04]); // authenticatorGetInfo
        device.add_write(&msg, 0);

        // ctap2 response
        let mut msg = cid.to_vec();
        msg.extend(vec![HIDCmd::Cbor.into(), 0x00, 0x5A]); // cmd + bcnt
        msg.extend(vec![0]); // Status code: Success
        msg.extend(&AUTHENTICATOR_INFO_PAYLOAD[0..(IN_HID_RPT_SIZE - 8)]);
        device.add_read(&msg, 0);
        // Continuation package
        let mut msg = cid.to_vec();
        msg.extend(vec![0x00]); // SEQ
        msg.extend(&AUTHENTICATOR_INFO_PAYLOAD[(IN_HID_RPT_SIZE - 8)..]);
        device.add_read(&msg, 0);
        device.init().expect("Failed to init device");

        assert_eq!(device.get_cid(), &cid);

        let dev_info = device.get_device_info();
        assert_eq!(
            dev_info.cap_flags,
            Capability::WINK | Capability::CBOR | Capability::NMSG
        );

        let result = device
            .get_authenticator_info()
            .expect("Didn't get any authenticator_info");
        let expected = AuthenticatorInfo {
            versions: vec![AuthenticatorVersion::U2F_V2, AuthenticatorVersion::FIDO_2_0],
            extensions: vec!["uvm".to_string(), "hmac-secret".to_string()],
            aaguid: AAGuid(AAGUID_RAW),
            options: AuthenticatorOptions {
                platform_device: false,
                resident_key: true,
                client_pin: Some(false),
                user_presence: true,
                user_verification: None,
                ..Default::default()
            },
            max_msg_size: Some(1200),
            pin_protocols: Some(vec![1]),
            max_credential_count_in_list: None,
            max_credential_id_length: None,
            transports: None,
            algorithms: None,
            max_ser_large_blob_array: None,
            force_pin_change: None,
            min_pin_length: None,
            firmware_version: None,
            max_cred_blob_length: None,
            max_rpids_for_set_min_pin_length: None,
            preferred_platform_uv_attempts: None,
            uv_modality: None,
            certifications: None,
            remaining_discoverable_credentials: None,
            vendor_prototype_config_commands: None,
        };

        assert_eq!(result, &expected);
    }

    #[test]
    fn test_authenticator_info_max_version() {
        let fido2_0 = AuthenticatorInfo {
            versions: vec![AuthenticatorVersion::U2F_V2, AuthenticatorVersion::FIDO_2_0],
            ..Default::default()
        };
        assert_eq!(
            fido2_0.max_supported_version(),
            AuthenticatorVersion::FIDO_2_0
        );

        let fido2_1_pre = AuthenticatorInfo {
            versions: vec![
                AuthenticatorVersion::FIDO_2_1_PRE,
                AuthenticatorVersion::U2F_V2,
            ],
            ..Default::default()
        };
        assert_eq!(
            fido2_1_pre.max_supported_version(),
            AuthenticatorVersion::FIDO_2_1_PRE
        );

        let fido2_1 = AuthenticatorInfo {
            versions: vec![
                AuthenticatorVersion::FIDO_2_1_PRE,
                AuthenticatorVersion::FIDO_2_1,
                AuthenticatorVersion::U2F_V2,
                AuthenticatorVersion::FIDO_2_0,
            ],
            ..Default::default()
        };
        assert_eq!(
            fido2_1.max_supported_version(),
            AuthenticatorVersion::FIDO_2_1
        );
    }

    #[test]
    fn parse_authenticator_info_protocol_versions() {
        let mut expected = AuthenticatorInfo {
            versions: vec![AuthenticatorVersion::U2F_V2, AuthenticatorVersion::FIDO_2_0],
            extensions: vec!["uvm".to_string(), "hmac-secret".to_string()],
            aaguid: AAGuid(AAGUID_RAW),
            options: AuthenticatorOptions {
                platform_device: false,
                resident_key: true,
                client_pin: Some(false),
                user_presence: true,
                ..Default::default()
            },
            max_msg_size: Some(1200),
            pin_protocols: None,
            ..Default::default()
        };

        let raw_data = AUTHENTICATOR_INFO_PAYLOAD.to_vec();
        // pin protocol entry (last 3 bytes in payload):
        // 0x06, //  unsigned(6)
        // 0x81, //  array(1)
        // 0x01, //    unsigned(1)

        let mut raw_empty_list = raw_data.clone();
        raw_empty_list.pop();
        let raw_list_len = raw_empty_list.len();
        raw_empty_list[raw_list_len - 1] = 0x80; // array(0) instead of array(1)

        // Empty protocols-array, that should produce an error
        from_slice::<AuthenticatorInfo>(&raw_empty_list).unwrap_err();

        // No protocols specified
        let mut raw_no_list = raw_data.clone();
        raw_no_list.pop();
        raw_no_list.pop();
        raw_no_list.pop();
        raw_no_list[0] = 0xa5; // map(5) instead of map(6)

        let authenticator_info: AuthenticatorInfo = from_slice(&raw_no_list).unwrap();
        assert_eq!(authenticator_info, expected);

        // Both 1 and 2
        let mut raw_list = raw_data;
        let raw_list_len = raw_list.len();
        raw_list[raw_list_len - 2] = 0x82; // array(2) instead of array(1)
        raw_list.push(0x02);

        expected.pin_protocols = Some(vec![1, 2]);
        let authenticator_info: AuthenticatorInfo = from_slice(&raw_list).unwrap();
        assert_eq!(authenticator_info, expected);
    }
}
