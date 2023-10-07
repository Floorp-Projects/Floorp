use super::get_info::AuthenticatorInfo;
use super::{
    Command, CommandError, PinUvAuthCommand, RequestCtap1, RequestCtap2, Retryable, StatusCode,
};
use crate::consts::{
    PARAMETER_SIZE, U2F_AUTHENTICATE, U2F_DONT_ENFORCE_USER_PRESENCE_AND_SIGN,
    U2F_REQUEST_USER_PRESENCE,
};
use crate::crypto::{COSEKey, CryptoError, PinUvAuthParam, PinUvAuthToken, SharedSecret};
use crate::ctap2::attestation::{AuthenticatorData, AuthenticatorDataFlags};
use crate::ctap2::client_data::ClientDataHash;
use crate::ctap2::commands::get_next_assertion::GetNextAssertion;
use crate::ctap2::commands::make_credentials::UserVerification;
use crate::ctap2::server::{
    AuthenticationExtensionsClientInputs, AuthenticationExtensionsClientOutputs,
    AuthenticatorAttachment, PublicKeyCredentialDescriptor, PublicKeyCredentialUserEntity,
    RelyingParty, RpIdHash, UserVerificationRequirement,
};
use crate::ctap2::utils::{read_be_u32, read_byte};
use crate::errors::AuthenticatorError;
use crate::transport::errors::{ApduErrorStatus, HIDError};
use crate::transport::{FidoDevice, VirtualFidoDevice};
use crate::u2ftypes::CTAP1RequestAPDU;
use serde::{
    de::{Error as DesError, MapAccess, Visitor},
    ser::{Error as SerError, SerializeMap},
    Deserialize, Deserializer, Serialize, Serializer,
};
use serde_bytes::ByteBuf;
use serde_cbor::{de::from_slice, ser, Value};
use std::fmt;
use std::io::Cursor;

#[derive(Clone, Copy, Debug, Serialize)]
#[cfg_attr(test, derive(Deserialize))]
pub struct GetAssertionOptions {
    #[serde(rename = "uv", skip_serializing_if = "Option::is_none")]
    pub user_verification: Option<bool>,
    #[serde(rename = "up", skip_serializing_if = "Option::is_none")]
    pub user_presence: Option<bool>,
}

impl Default for GetAssertionOptions {
    fn default() -> Self {
        Self {
            user_presence: Some(true),
            user_verification: None,
        }
    }
}

impl GetAssertionOptions {
    pub(crate) fn has_some(&self) -> bool {
        self.user_presence.is_some() || self.user_verification.is_some()
    }
}

impl UserVerification for GetAssertionOptions {
    fn ask_user_verification(&self) -> bool {
        if let Some(e) = self.user_verification {
            e
        } else {
            false
        }
    }
}

#[derive(Debug, Clone)]
pub struct CalculatedHmacSecretExtension {
    pub public_key: COSEKey,
    pub salt_enc: Vec<u8>,
    pub salt_auth: Vec<u8>,
}

#[derive(Debug, Clone, Default)]
pub struct HmacSecretExtension {
    pub salt1: Vec<u8>,
    pub salt2: Option<Vec<u8>>,
    calculated_hmac: Option<CalculatedHmacSecretExtension>,
}

impl HmacSecretExtension {
    pub fn new(salt1: Vec<u8>, salt2: Option<Vec<u8>>) -> Self {
        HmacSecretExtension {
            salt1,
            salt2,
            calculated_hmac: None,
        }
    }

    pub fn calculate(&mut self, secret: &SharedSecret) -> Result<(), AuthenticatorError> {
        if self.salt1.len() < 32 {
            return Err(CryptoError::WrongSaltLength.into());
        }
        let salt_enc = match &self.salt2 {
            Some(salt2) => {
                if salt2.len() < 32 {
                    return Err(CryptoError::WrongSaltLength.into());
                }
                let salts = [&self.salt1[..32], &salt2[..32]].concat(); // salt1 || salt2
                secret.encrypt(&salts)
            }
            None => secret.encrypt(&self.salt1[..32]),
        }?;
        let salt_auth = secret.authenticate(&salt_enc)?;
        let public_key = secret.client_input().clone();
        self.calculated_hmac = Some(CalculatedHmacSecretExtension {
            public_key,
            salt_enc,
            salt_auth,
        });

        Ok(())
    }
}

impl Serialize for HmacSecretExtension {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        if let Some(calc) = &self.calculated_hmac {
            let mut map = serializer.serialize_map(Some(3))?;
            map.serialize_entry(&1, &calc.public_key)?;
            map.serialize_entry(&2, serde_bytes::Bytes::new(&calc.salt_enc))?;
            map.serialize_entry(&3, serde_bytes::Bytes::new(&calc.salt_auth))?;
            map.end()
        } else {
            Err(SerError::custom(
                "hmac secret has not been calculated before being serialized",
            ))
        }
    }
}

#[derive(Debug, Default, Clone, Serialize)]
pub struct GetAssertionExtensions {
    #[serde(skip_serializing)]
    pub app_id: Option<String>,
    #[serde(rename = "hmac-secret", skip_serializing_if = "Option::is_none")]
    pub hmac_secret: Option<HmacSecretExtension>,
}

impl From<AuthenticationExtensionsClientInputs> for GetAssertionExtensions {
    fn from(input: AuthenticationExtensionsClientInputs) -> Self {
        Self {
            app_id: input.app_id,
            ..Default::default()
        }
    }
}

impl GetAssertionExtensions {
    fn has_content(&self) -> bool {
        self.hmac_secret.is_some()
    }
}

#[derive(Debug, Clone)]
pub struct GetAssertion {
    pub client_data_hash: ClientDataHash,
    pub rp: RelyingParty,
    pub allow_list: Vec<PublicKeyCredentialDescriptor>,

    // https://www.w3.org/TR/webauthn/#client-extension-input
    // The client extension input, which is a value that can be encoded in JSON,
    // is passed from the WebAuthn Relying Party to the client in the get() or
    // create() call, while the CBOR authenticator extension input is passed
    // from the client to the authenticator for authenticator extensions during
    // the processing of these calls.
    pub extensions: GetAssertionExtensions,
    pub options: GetAssertionOptions,
    pub pin_uv_auth_param: Option<PinUvAuthParam>,
}

impl GetAssertion {
    pub fn new(
        client_data_hash: ClientDataHash,
        rp: RelyingParty,
        allow_list: Vec<PublicKeyCredentialDescriptor>,
        options: GetAssertionOptions,
        extensions: GetAssertionExtensions,
    ) -> Self {
        Self {
            client_data_hash,
            rp,
            allow_list,
            extensions,
            options,
            pin_uv_auth_param: None,
        }
    }

    pub fn finalize_result<Dev: FidoDevice>(&self, dev: &Dev, result: &mut GetAssertionResult) {
        result.attachment = match dev.get_authenticator_info() {
            Some(info) if info.options.platform_device => AuthenticatorAttachment::Platform,
            Some(_) => AuthenticatorAttachment::CrossPlatform,
            None => AuthenticatorAttachment::Unknown,
        };

        // Handle extensions whose outputs are not encoded in the authenticator data.
        // 1. appId
        if let Some(app_id) = &self.extensions.app_id {
            result.extensions.app_id =
                Some(result.assertion.auth_data.rp_id_hash == RelyingParty::from(app_id).hash());
        }
    }
}

impl PinUvAuthCommand for GetAssertion {
    fn set_pin_uv_auth_param(
        &mut self,
        pin_uv_auth_token: Option<PinUvAuthToken>,
    ) -> Result<(), AuthenticatorError> {
        let mut param = None;
        if let Some(token) = pin_uv_auth_token {
            param = Some(
                token
                    .derive(self.client_data_hash.as_ref())
                    .map_err(CommandError::Crypto)?,
            );
        }
        self.pin_uv_auth_param = param;
        Ok(())
    }

    fn set_uv_option(&mut self, uv: Option<bool>) {
        self.options.user_verification = uv;
    }

    fn get_rp_id(&self) -> Option<&String> {
        Some(&self.rp.id)
    }

    fn can_skip_user_verification(
        &mut self,
        info: &AuthenticatorInfo,
        uv_req: UserVerificationRequirement,
    ) -> bool {
        let supports_uv = info.options.user_verification == Some(true);
        let pin_configured = info.options.client_pin == Some(true);
        let device_protected = supports_uv || pin_configured;
        let uv_discouraged = uv_req == UserVerificationRequirement::Discouraged;
        let always_uv = info.options.always_uv == Some(true);

        !always_uv && (!device_protected || uv_discouraged)
    }

    fn get_pin_uv_auth_param(&self) -> Option<&PinUvAuthParam> {
        self.pin_uv_auth_param.as_ref()
    }
}

impl Serialize for GetAssertion {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        // Need to define how many elements are going to be in the map
        // beforehand
        let mut map_len = 2;
        if !self.allow_list.is_empty() {
            map_len += 1;
        }
        if self.extensions.has_content() {
            map_len += 1;
        }
        if self.options.has_some() {
            map_len += 1;
        }
        if self.pin_uv_auth_param.is_some() {
            map_len += 2;
        }

        let mut map = serializer.serialize_map(Some(map_len))?;
        map.serialize_entry(&1, &self.rp.id)?;
        map.serialize_entry(&2, &self.client_data_hash)?;
        if !self.allow_list.is_empty() {
            map.serialize_entry(&3, &self.allow_list)?;
        }
        if self.extensions.has_content() {
            map.serialize_entry(&4, &self.extensions)?;
        }
        if self.options.has_some() {
            map.serialize_entry(&5, &self.options)?;
        }
        if let Some(pin_uv_auth_param) = &self.pin_uv_auth_param {
            map.serialize_entry(&6, &pin_uv_auth_param)?;
            map.serialize_entry(&7, &pin_uv_auth_param.pin_protocol.id())?;
        }
        map.end()
    }
}

impl RequestCtap1 for GetAssertion {
    type Output = Vec<GetAssertionResult>;
    type AdditionalInfo = PublicKeyCredentialDescriptor;

    fn ctap1_format(&self) -> Result<(Vec<u8>, Self::AdditionalInfo), HIDError> {
        // Pre-flighting should reduce the list to exactly one entry
        let key_handle = match &self.allow_list[..] {
            [key_handle] => key_handle,
            [] => {
                return Err(HIDError::Command(CommandError::StatusCode(
                    StatusCode::NoCredentials,
                    None,
                )));
            }
            _ => {
                return Err(HIDError::UnsupportedCommand);
            }
        };

        debug!("sending key_handle = {:?}", key_handle);

        let flags = if self.options.user_presence.unwrap_or(true) {
            U2F_REQUEST_USER_PRESENCE
        } else {
            U2F_DONT_ENFORCE_USER_PRESENCE_AND_SIGN
        };
        let mut auth_data =
            Vec::with_capacity(2 * PARAMETER_SIZE + 1 /* key_handle_len */ + key_handle.id.len());

        auth_data.extend_from_slice(self.client_data_hash.as_ref());
        auth_data.extend_from_slice(self.rp.hash().as_ref());
        auth_data.extend_from_slice(&[key_handle.id.len() as u8]);
        auth_data.extend_from_slice(key_handle.id.as_ref());

        let cmd = U2F_AUTHENTICATE;
        let apdu = CTAP1RequestAPDU::serialize(cmd, flags, &auth_data)?;
        Ok((apdu, key_handle.clone()))
    }

    fn handle_response_ctap1<Dev: FidoDevice>(
        &self,
        dev: &mut Dev,
        status: Result<(), ApduErrorStatus>,
        input: &[u8],
        add_info: &PublicKeyCredentialDescriptor,
    ) -> Result<Self::Output, Retryable<HIDError>> {
        if Err(ApduErrorStatus::ConditionsNotSatisfied) == status {
            return Err(Retryable::Retry);
        }
        if let Err(err) = status {
            return Err(Retryable::Error(HIDError::ApduStatus(err)));
        }

        let mut result = GetAssertionResult::from_ctap1(input, &self.rp.hash(), add_info)
            .map_err(|e| Retryable::Error(HIDError::Command(e)))?;
        self.finalize_result(dev, &mut result);
        // Although there's only one result, we return a vector for consistency with CTAP2.
        Ok(vec![result])
    }

    fn send_to_virtual_device<Dev: VirtualFidoDevice>(
        &self,
        dev: &mut Dev,
    ) -> Result<Self::Output, HIDError> {
        let mut results = dev.get_assertion(self)?;
        for result in results.iter_mut() {
            self.finalize_result(dev, result);
        }
        Ok(results)
    }
}

impl RequestCtap2 for GetAssertion {
    type Output = Vec<GetAssertionResult>;

    fn command(&self) -> Command {
        Command::GetAssertion
    }

    fn wire_format(&self) -> Result<Vec<u8>, HIDError> {
        Ok(ser::to_vec(&self).map_err(CommandError::Serializing)?)
    }

    fn handle_response_ctap2<Dev: FidoDevice>(
        &self,
        dev: &mut Dev,
        input: &[u8],
    ) -> Result<Self::Output, HIDError> {
        if input.is_empty() {
            return Err(CommandError::InputTooSmall.into());
        }

        let status: StatusCode = input[0].into();
        debug!(
            "response status code: {:?}, rest: {:?}",
            status,
            &input[1..]
        );
        if input.len() == 1 {
            if status.is_ok() {
                return Err(CommandError::InputTooSmall.into());
            }
            return Err(CommandError::StatusCode(status, None).into());
        }

        if status.is_ok() {
            let assertion: GetAssertionResponse =
                from_slice(&input[1..]).map_err(CommandError::Deserializing)?;
            let number_of_credentials = assertion.number_of_credentials.unwrap_or(1);

            let mut results = Vec::with_capacity(number_of_credentials);
            results.push(GetAssertionResult {
                assertion: assertion.into(),
                attachment: AuthenticatorAttachment::Unknown,
                extensions: Default::default(),
            });

            let msg = GetNextAssertion;
            // We already have one, so skipping 0
            for _ in 1..number_of_credentials {
                let assertion = dev.send_cbor(&msg)?;
                results.push(GetAssertionResult {
                    assertion: assertion.into(),
                    attachment: AuthenticatorAttachment::Unknown,
                    extensions: Default::default(),
                });
            }

            for result in results.iter_mut() {
                self.finalize_result(dev, result);
            }
            Ok(results)
        } else {
            let data: Value = from_slice(&input[1..]).map_err(CommandError::Deserializing)?;
            Err(CommandError::StatusCode(status, Some(data)).into())
        }
    }

    fn send_to_virtual_device<Dev: VirtualFidoDevice>(
        &self,
        dev: &mut Dev,
    ) -> Result<Self::Output, HIDError> {
        let mut results = dev.get_assertion(self)?;
        for result in results.iter_mut() {
            self.finalize_result(dev, result);
        }
        Ok(results)
    }
}

#[derive(Debug, PartialEq, Eq)]
pub struct Assertion {
    pub credentials: Option<PublicKeyCredentialDescriptor>, /* Was optional in CTAP2.0, is
                                                             * mandatory in CTAP2.1 */
    pub auth_data: AuthenticatorData,
    pub signature: Vec<u8>,
    pub user: Option<PublicKeyCredentialUserEntity>,
}

impl From<GetAssertionResponse> for Assertion {
    fn from(r: GetAssertionResponse) -> Self {
        Assertion {
            credentials: r.credentials,
            auth_data: r.auth_data,
            signature: r.signature,
            user: r.user,
        }
    }
}

#[derive(Debug, PartialEq, Eq)]
pub struct GetAssertionResult {
    pub assertion: Assertion,
    pub attachment: AuthenticatorAttachment,
    pub extensions: AuthenticationExtensionsClientOutputs,
}

impl GetAssertionResult {
    pub fn from_ctap1(
        input: &[u8],
        rp_id_hash: &RpIdHash,
        key_handle: &PublicKeyCredentialDescriptor,
    ) -> Result<GetAssertionResult, CommandError> {
        let mut data = Cursor::new(input);
        let user_presence = read_byte(&mut data).map_err(CommandError::Deserializing)?;
        let counter = read_be_u32(&mut data).map_err(CommandError::Deserializing)?;
        // Remaining data is signature (Note: `data.remaining_slice()` is not yet stabilized)
        let signature = Vec::from(&data.get_ref()[data.position() as usize..]);

        // Step 5 of Section 10.3 of CTAP2.1: "Copy bits 0 (the UP bit) and bit 1 from the
        // CTAP2/U2F response user presence byte to bits 0 and 1 of the CTAP2 flags, respectively.
        // Set all other bits of flags to zero."
        let flag_mask = AuthenticatorDataFlags::USER_PRESENT | AuthenticatorDataFlags::RESERVED_1;
        let flags = flag_mask & AuthenticatorDataFlags::from_bits_truncate(user_presence);
        let auth_data = AuthenticatorData {
            rp_id_hash: rp_id_hash.clone(),
            flags,
            counter,
            credential_data: None,
            extensions: Default::default(),
        };
        let assertion = Assertion {
            credentials: Some(key_handle.clone()),
            signature,
            user: None,
            auth_data,
        };

        Ok(GetAssertionResult {
            assertion,
            attachment: AuthenticatorAttachment::Unknown,
            extensions: Default::default(),
        })
    }
}

pub struct GetAssertionResponse {
    pub credentials: Option<PublicKeyCredentialDescriptor>,
    pub auth_data: AuthenticatorData,
    pub signature: Vec<u8>,
    pub user: Option<PublicKeyCredentialUserEntity>,
    pub number_of_credentials: Option<usize>,
}

impl<'de> Deserialize<'de> for GetAssertionResponse {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct GetAssertionResponseVisitor;

        impl<'de> Visitor<'de> for GetAssertionResponseVisitor {
            type Value = GetAssertionResponse;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a byte array")
            }

            fn visit_map<M>(self, mut map: M) -> Result<Self::Value, M::Error>
            where
                M: MapAccess<'de>,
            {
                let mut credentials = None;
                let mut auth_data = None;
                let mut signature = None;
                let mut user = None;
                let mut number_of_credentials = None;

                while let Some(key) = map.next_key()? {
                    match key {
                        1 => {
                            if credentials.is_some() {
                                return Err(M::Error::duplicate_field("credentials"));
                            }
                            credentials = Some(map.next_value()?);
                        }
                        2 => {
                            if auth_data.is_some() {
                                return Err(M::Error::duplicate_field("auth_data"));
                            }
                            auth_data = Some(map.next_value()?);
                        }
                        3 => {
                            if signature.is_some() {
                                return Err(M::Error::duplicate_field("signature"));
                            }
                            let signature_bytes: ByteBuf = map.next_value()?;
                            let signature_bytes: Vec<u8> = signature_bytes.into_vec();
                            signature = Some(signature_bytes);
                        }
                        4 => {
                            if user.is_some() {
                                return Err(M::Error::duplicate_field("user"));
                            }
                            user = map.next_value()?;
                        }
                        5 => {
                            if number_of_credentials.is_some() {
                                return Err(M::Error::duplicate_field("number_of_credentials"));
                            }
                            number_of_credentials = Some(map.next_value()?);
                        }
                        k => return Err(M::Error::custom(format!("unexpected key: {k:?}"))),
                    }
                }

                let auth_data = auth_data.ok_or_else(|| M::Error::missing_field("auth_data"))?;
                let signature = signature.ok_or_else(|| M::Error::missing_field("signature"))?;

                Ok(GetAssertionResponse {
                    credentials,
                    auth_data,
                    signature,
                    user,
                    number_of_credentials,
                })
            }
        }

        deserializer.deserialize_bytes(GetAssertionResponseVisitor)
    }
}

#[cfg(test)]
pub mod test {
    use super::{
        Assertion, CommandError, GetAssertion, GetAssertionOptions, GetAssertionResult, HIDError,
        StatusCode,
    };
    use crate::consts::{
        Capability, HIDCmd, SW_CONDITIONS_NOT_SATISFIED, SW_NO_ERROR, U2F_CHECK_IS_REGISTERED,
        U2F_REQUEST_USER_PRESENCE,
    };
    use crate::ctap2::attestation::{AAGuid, AuthenticatorData, AuthenticatorDataFlags};
    use crate::ctap2::client_data::{Challenge, CollectedClientData, TokenBinding, WebauthnType};
    use crate::ctap2::commands::get_info::tests::AAGUID_RAW;
    use crate::ctap2::commands::get_info::{
        AuthenticatorInfo, AuthenticatorOptions, AuthenticatorVersion,
    };
    use crate::ctap2::commands::RequestCtap1;
    use crate::ctap2::preflight::{
        do_credential_list_filtering_ctap1, do_credential_list_filtering_ctap2,
    };
    use crate::ctap2::server::{
        AuthenticatorAttachment, PublicKeyCredentialDescriptor, PublicKeyCredentialUserEntity,
        RelyingParty, RpIdHash, Transport,
    };
    use crate::transport::device_selector::Device;
    use crate::transport::hid::HIDDevice;
    use crate::transport::{FidoDevice, FidoDeviceIO, FidoProtocol};
    use crate::u2ftypes::U2FDeviceInfo;
    use rand::{thread_rng, RngCore};

    #[test]
    fn test_get_assertion_ctap2() {
        let client_data = CollectedClientData {
            webauthn_type: WebauthnType::Create,
            challenge: Challenge::from(vec![0x00, 0x01, 0x02, 0x03]),
            origin: String::from("example.com"),
            cross_origin: false,
            token_binding: Some(TokenBinding::Present(String::from("AAECAw"))),
        };
        let assertion = GetAssertion::new(
            client_data.hash().expect("failed to serialize client data"),
            RelyingParty::from("example.com"),
            vec![PublicKeyCredentialDescriptor {
                id: vec![
                    0x3E, 0xBD, 0x89, 0xBF, 0x77, 0xEC, 0x50, 0x97, 0x55, 0xEE, 0x9C, 0x26, 0x35,
                    0xEF, 0xAA, 0xAC, 0x7B, 0x2B, 0x9C, 0x5C, 0xEF, 0x17, 0x36, 0xC3, 0x71, 0x7D,
                    0xA4, 0x85, 0x34, 0xC8, 0xC6, 0xB6, 0x54, 0xD7, 0xFF, 0x94, 0x5F, 0x50, 0xB5,
                    0xCC, 0x4E, 0x78, 0x05, 0x5B, 0xDD, 0x39, 0x6B, 0x64, 0xF7, 0x8D, 0xA2, 0xC5,
                    0xF9, 0x62, 0x00, 0xCC, 0xD4, 0x15, 0xCD, 0x08, 0xFE, 0x42, 0x00, 0x38,
                ],
                transports: vec![Transport::USB],
            }],
            GetAssertionOptions {
                user_presence: Some(true),
                user_verification: None,
            },
            Default::default(),
        );
        let mut device = Device::new("commands/get_assertion").unwrap();
        assert_eq!(device.get_protocol(), FidoProtocol::CTAP2);
        let mut cid = [0u8; 4];
        thread_rng().fill_bytes(&mut cid);
        device.set_cid(cid);

        let mut msg = cid.to_vec();
        msg.extend(vec![HIDCmd::Cbor.into(), 0x00, 0x90]);
        msg.extend(vec![0x2]); // u2f command
        msg.extend(vec![
            0xa4, // map(4)
            0x1,  // rpid
            0x6b, // text(11)
            101, 120, 97, 109, 112, 108, 101, 46, 99, 111, 109, // example.com
            0x2, // clientDataHash
            0x58, 0x20, //bytes(32)
            0x75, 0x35, 0x35, 0x7d, 0x49, 0x6e, 0x33, 0xc8, 0x18, 0x7f, 0xea, 0x8d, 0x11, 0x32,
            0x64, 0xaa, 0xa4, 0x52, 0x3e, 0x13, 0x40, 0x14, 0x9f, 0xbe, 0x00, 0x3f, 0x10, 0x87,
            0x54, 0xc3, 0x2d, 0x80, // hash
            0x3,  //allowList
            0x81, // array(1)
            0xa2, // map(2)
            0x62, // text(2)
            0x69, 0x64, // id
            0x58, // bytes(
        ]);
        device.add_write(&msg, 0);

        msg = cid.to_vec();
        msg.extend([0x0]); //SEQ
        msg.extend([0x40]); // 64)
        msg.extend(&assertion.allow_list[0].id[..58]);
        device.add_write(&msg, 0);

        msg = cid.to_vec();
        msg.extend([0x1]); //SEQ
        msg.extend(&assertion.allow_list[0].id[58..64]);
        msg.extend(vec![
            0x64, // text(4),
            0x74, 0x79, 0x70, 0x65, // type
            0x6a, // text(10)
            0x70, 0x75, 0x62, 0x6C, 0x69, 0x63, 0x2D, 0x6B, 0x65, 0x79, // public-key
            0x5,  // options
            0xa1, // map(1)
            0x62, // text(2)
            0x75, 0x70, // up
            0xf5, // true
        ]);
        device.add_write(&msg, 0);

        // fido response
        let mut msg = cid.to_vec();
        msg.extend([HIDCmd::Cbor.into(), 0x1, 0x2a]); // cmd + bcnt
        msg.extend(&GET_ASSERTION_SAMPLE_RESPONSE_CTAP2[..57]);
        device.add_read(&msg, 0);

        let mut msg = cid.to_vec();
        msg.extend([0x0]); // SEQ
        msg.extend(&GET_ASSERTION_SAMPLE_RESPONSE_CTAP2[57..116]);
        device.add_read(&msg, 0);

        let mut msg = cid.to_vec();
        msg.extend([0x1]); // SEQ
        msg.extend(&GET_ASSERTION_SAMPLE_RESPONSE_CTAP2[116..175]);
        device.add_read(&msg, 0);

        let mut msg = cid.to_vec();
        msg.extend([0x2]); // SEQ
        msg.extend(&GET_ASSERTION_SAMPLE_RESPONSE_CTAP2[175..234]);
        device.add_read(&msg, 0);

        let mut msg = cid.to_vec();
        msg.extend([0x3]); // SEQ
        msg.extend(&GET_ASSERTION_SAMPLE_RESPONSE_CTAP2[234..293]);
        device.add_read(&msg, 0);
        let mut msg = cid.to_vec();
        msg.extend([0x4]); // SEQ
        msg.extend(&GET_ASSERTION_SAMPLE_RESPONSE_CTAP2[293..]);
        device.add_read(&msg, 0);

        // Check if response is correct
        let expected_auth_data = AuthenticatorData {
            rp_id_hash: RpIdHash([
                0x62, 0x5d, 0xda, 0xdf, 0x74, 0x3f, 0x57, 0x27, 0xe6, 0x6b, 0xba, 0x8c, 0x2e, 0x38,
                0x79, 0x22, 0xd1, 0xaf, 0x43, 0xc5, 0x03, 0xd9, 0x11, 0x4a, 0x8f, 0xba, 0x10, 0x4d,
                0x84, 0xd0, 0x2b, 0xfa,
            ]),
            flags: AuthenticatorDataFlags::USER_PRESENT,
            counter: 0x11,
            credential_data: None,
            extensions: Default::default(),
        };

        let expected_assertion = Assertion {
            credentials: Some(PublicKeyCredentialDescriptor {
                id: vec![
                    242, 32, 6, 222, 79, 144, 90, 246, 138, 67, 148, 47, 2, 79, 42, 94, 206, 96,
                    61, 156, 109, 75, 61, 248, 190, 8, 237, 1, 252, 68, 38, 70, 208, 52, 133, 138,
                    199, 91, 237, 63, 213, 128, 191, 152, 8, 217, 79, 203, 238, 130, 185, 178, 239,
                    102, 119, 175, 10, 220, 195, 88, 82, 234, 107, 158,
                ],
                transports: vec![],
            }),
            signature: vec![
                0x30, 0x45, 0x02, 0x20, 0x4a, 0x5a, 0x9d, 0xd3, 0x92, 0x98, 0x14, 0x9d, 0x90, 0x47,
                0x69, 0xb5, 0x1a, 0x45, 0x14, 0x33, 0x00, 0x6f, 0x18, 0x2a, 0x34, 0xfb, 0xdf, 0x66,
                0xde, 0x5f, 0xc7, 0x17, 0xd7, 0x5f, 0xb3, 0x50, 0x02, 0x21, 0x00, 0xa4, 0x6b, 0x8e,
                0xa3, 0xc3, 0xb9, 0x33, 0x82, 0x1c, 0x6e, 0x7f, 0x5e, 0xf9, 0xda, 0xae, 0x94, 0xab,
                0x47, 0xf1, 0x8d, 0xb4, 0x74, 0xc7, 0x47, 0x90, 0xea, 0xab, 0xb1, 0x44, 0x11, 0xe7,
                0xa0,
            ],
            user: Some(PublicKeyCredentialUserEntity {
                id: vec![
                    0x30, 0x82, 0x01, 0x93, 0x30, 0x82, 0x01, 0x38, 0xa0, 0x03, 0x02, 0x01, 0x02,
                    0x30, 0x82, 0x01, 0x93, 0x30, 0x82, 0x01, 0x38, 0xa0, 0x03, 0x02, 0x01, 0x02,
                    0x30, 0x82, 0x01, 0x93, 0x30, 0x82,
                ],
                name: Some("johnpsmith@example.com".to_string()),
                display_name: Some("John P. Smith".to_string()),
            }),
            auth_data: expected_auth_data,
        };

        let expected = vec![GetAssertionResult {
            assertion: expected_assertion,
            attachment: AuthenticatorAttachment::Unknown,
            extensions: Default::default(),
        }];
        let response = device.send_cbor(&assertion).unwrap();
        assert_eq!(response, expected);
    }

    fn fill_device_ctap1(device: &mut Device, cid: [u8; 4], flags: u8, answer_status: [u8; 2]) {
        // ctap2 request
        let mut msg = cid.to_vec();
        msg.extend([HIDCmd::Msg.into(), 0x00, 0x8A]); // cmd + bcnt
        msg.extend([0x00, 0x2]); // U2F_AUTHENTICATE
        msg.extend([flags]);
        msg.extend([0x00, 0x00, 0x00]);
        msg.extend([0x81]); // Data len - 7
        msg.extend(CLIENT_DATA_HASH);
        msg.extend(&RELYING_PARTY_HASH[..18]);
        device.add_write(&msg, 0);

        // Continuation package
        let mut msg = cid.to_vec();
        msg.extend(vec![0x00]); // SEQ
        msg.extend(&RELYING_PARTY_HASH[18..]);
        msg.extend([KEY_HANDLE.len() as u8]);
        msg.extend(&KEY_HANDLE[..44]);
        device.add_write(&msg, 0);

        let mut msg = cid.to_vec();
        msg.extend(vec![0x01]); // SEQ
        msg.extend(&KEY_HANDLE[44..]);
        device.add_write(&msg, 0);

        // fido response
        let mut msg = cid.to_vec();
        msg.extend([HIDCmd::Msg.into(), 0x0, 0x4D]); // cmd + bcnt
        msg.extend(&GET_ASSERTION_SAMPLE_RESPONSE_CTAP1[0..57]);
        device.add_read(&msg, 0);

        let mut msg = cid.to_vec();
        msg.extend([0x0]); // SEQ
        msg.extend(&GET_ASSERTION_SAMPLE_RESPONSE_CTAP1[57..]);
        msg.extend(answer_status);
        device.add_read(&msg, 0);
    }

    #[test]
    fn test_get_assertion_ctap1() {
        let client_data = CollectedClientData {
            webauthn_type: WebauthnType::Create,
            challenge: Challenge::from(vec![0x00, 0x01, 0x02, 0x03]),
            origin: String::from("example.com"),
            cross_origin: false,
            token_binding: Some(TokenBinding::Present(String::from("AAECAw"))),
        };
        let allowed_key = PublicKeyCredentialDescriptor {
            id: vec![
                0x3E, 0xBD, 0x89, 0xBF, 0x77, 0xEC, 0x50, 0x97, 0x55, 0xEE, 0x9C, 0x26, 0x35, 0xEF,
                0xAA, 0xAC, 0x7B, 0x2B, 0x9C, 0x5C, 0xEF, 0x17, 0x36, 0xC3, 0x71, 0x7D, 0xA4, 0x85,
                0x34, 0xC8, 0xC6, 0xB6, 0x54, 0xD7, 0xFF, 0x94, 0x5F, 0x50, 0xB5, 0xCC, 0x4E, 0x78,
                0x05, 0x5B, 0xDD, 0x39, 0x6B, 0x64, 0xF7, 0x8D, 0xA2, 0xC5, 0xF9, 0x62, 0x00, 0xCC,
                0xD4, 0x15, 0xCD, 0x08, 0xFE, 0x42, 0x00, 0x38,
            ],
            transports: vec![Transport::USB],
        };
        let mut assertion = GetAssertion::new(
            client_data.hash().expect("failed to serialize client data"),
            RelyingParty::from("example.com"),
            vec![allowed_key.clone()],
            GetAssertionOptions {
                user_presence: Some(true),
                user_verification: None,
            },
            Default::default(),
        );
        let mut device = Device::new("commands/get_assertion").unwrap(); // not really used (all functions ignore it)
                                                                         // channel id
        device.downgrade_to_ctap1();
        assert_eq!(device.get_protocol(), FidoProtocol::CTAP1);
        let mut cid = [0u8; 4];
        thread_rng().fill_bytes(&mut cid);

        device.set_cid(cid);

        // ctap1 request
        fill_device_ctap1(
            &mut device,
            cid,
            U2F_CHECK_IS_REGISTERED,
            SW_CONDITIONS_NOT_SATISFIED,
        );
        let key_handle = do_credential_list_filtering_ctap1(
            &mut device,
            &assertion.allow_list,
            &assertion.rp,
            &assertion.client_data_hash,
        )
        .expect("Did not find a key_handle, even though it should have");
        assertion.allow_list = vec![key_handle];
        let (ctap1_request, key_handle) = assertion.ctap1_format().unwrap();
        assert_eq!(key_handle, allowed_key);
        // Check if the request is going to be correct
        assert_eq!(ctap1_request, GET_ASSERTION_SAMPLE_REQUEST_CTAP1);

        // Now do it again, but parse the actual response
        // Pre-flighting is not done automatically
        fill_device_ctap1(&mut device, cid, U2F_REQUEST_USER_PRESENCE, SW_NO_ERROR);

        let response = device.send_ctap1(&assertion).unwrap();

        // Check if response is correct
        let expected_auth_data = AuthenticatorData {
            rp_id_hash: RpIdHash(RELYING_PARTY_HASH),
            flags: AuthenticatorDataFlags::USER_PRESENT,
            counter: 0x3B,
            credential_data: None,
            extensions: Default::default(),
        };

        let expected_assertion = Assertion {
            credentials: Some(allowed_key),
            signature: vec![
                0x30, 0x44, 0x02, 0x20, 0x7B, 0xDE, 0x0A, 0x52, 0xAC, 0x1F, 0x4C, 0x8B, 0x27, 0xE0,
                0x03, 0xA3, 0x70, 0xCD, 0x66, 0xA4, 0xC7, 0x11, 0x8D, 0xD2, 0x2D, 0x54, 0x47, 0x83,
                0x5F, 0x45, 0xB9, 0x9C, 0x68, 0x42, 0x3F, 0xF7, 0x02, 0x20, 0x3C, 0x51, 0x7B, 0x47,
                0x87, 0x7F, 0x85, 0x78, 0x2D, 0xE1, 0x00, 0x86, 0xA7, 0x83, 0xD1, 0xE7, 0xDF, 0x4E,
                0x36, 0x39, 0xE7, 0x71, 0xF5, 0xF6, 0xAF, 0xA3, 0x5A, 0xAD, 0x53, 0x73, 0x85, 0x8E,
            ],
            user: None,
            auth_data: expected_auth_data,
        };

        let expected = vec![GetAssertionResult {
            assertion: expected_assertion,
            attachment: AuthenticatorAttachment::Unknown,
            extensions: Default::default(),
        }];
        assert_eq!(response, expected);
    }

    #[test]
    fn test_get_assertion_ctap1_long_keys() {
        let client_data = CollectedClientData {
            webauthn_type: WebauthnType::Create,
            challenge: Challenge::from(vec![0x00, 0x01, 0x02, 0x03]),
            origin: String::from("example.com"),
            cross_origin: false,
            token_binding: Some(TokenBinding::Present(String::from("AAECAw"))),
        };

        let too_long_key_handle = PublicKeyCredentialDescriptor {
            id: vec![0; 1000],
            transports: vec![Transport::USB],
        };
        let mut assertion = GetAssertion::new(
            client_data.hash().expect("failed to serialize client data"),
            RelyingParty::from("example.com"),
            vec![too_long_key_handle.clone()],
            GetAssertionOptions {
                user_presence: Some(true),
                user_verification: None,
            },
            Default::default(),
        );

        let mut device = Device::new("commands/get_assertion").unwrap(); // not really used (all functions ignore it)
                                                                         // channel id
        device.downgrade_to_ctap1();
        assert_eq!(device.get_protocol(), FidoProtocol::CTAP1);
        let mut cid = [0u8; 4];
        thread_rng().fill_bytes(&mut cid);

        device.set_cid(cid);

        assert_matches!(
            do_credential_list_filtering_ctap1(
                &mut device,
                &assertion.allow_list,
                &assertion.rp,
                &assertion.client_data_hash,
            ),
            None
        );
        assertion.allow_list = vec![];
        // It should also fail when trying to format
        assert_matches!(
            assertion.ctap1_format(),
            Err(HIDError::Command(CommandError::StatusCode(
                StatusCode::NoCredentials,
                ..
            )))
        );

        // Test also multiple too long keys and an empty allow list
        for allow_list in [vec![], vec![too_long_key_handle.clone(); 5]] {
            assertion.allow_list = allow_list;

            assert_matches!(
                do_credential_list_filtering_ctap1(
                    &mut device,
                    &assertion.allow_list,
                    &assertion.rp,
                    &assertion.client_data_hash,
                ),
                None
            );
        }

        let ok_key_handle = PublicKeyCredentialDescriptor {
            id: vec![
                0x3E, 0xBD, 0x89, 0xBF, 0x77, 0xEC, 0x50, 0x97, 0x55, 0xEE, 0x9C, 0x26, 0x35, 0xEF,
                0xAA, 0xAC, 0x7B, 0x2B, 0x9C, 0x5C, 0xEF, 0x17, 0x36, 0xC3, 0x71, 0x7D, 0xA4, 0x85,
                0x34, 0xC8, 0xC6, 0xB6, 0x54, 0xD7, 0xFF, 0x94, 0x5F, 0x50, 0xB5, 0xCC, 0x4E, 0x78,
                0x05, 0x5B, 0xDD, 0x39, 0x6B, 0x64, 0xF7, 0x8D, 0xA2, 0xC5, 0xF9, 0x62, 0x00, 0xCC,
                0xD4, 0x15, 0xCD, 0x08, 0xFE, 0x42, 0x00, 0x38,
            ],
            transports: vec![Transport::USB],
        };
        assertion.allow_list = vec![
            too_long_key_handle.clone(),
            too_long_key_handle.clone(),
            too_long_key_handle.clone(),
            ok_key_handle.clone(),
            too_long_key_handle,
        ];

        // ctap1 request
        fill_device_ctap1(
            &mut device,
            cid,
            U2F_CHECK_IS_REGISTERED,
            SW_CONDITIONS_NOT_SATISFIED,
        );
        let key_handle = do_credential_list_filtering_ctap1(
            &mut device,
            &assertion.allow_list,
            &assertion.rp,
            &assertion.client_data_hash,
        )
        .expect("Did not find a key_handle, even though it should have");
        assertion.allow_list = vec![key_handle];
        let (ctap1_request, key_handle) = assertion.ctap1_format().unwrap();
        assert_eq!(key_handle, ok_key_handle);
        // Check if the request is going to be correct
        assert_eq!(ctap1_request, GET_ASSERTION_SAMPLE_REQUEST_CTAP1);

        // Now do it again, but parse the actual response
        // Pre-flighting is not done automatically
        fill_device_ctap1(&mut device, cid, U2F_REQUEST_USER_PRESENCE, SW_NO_ERROR);

        let response = device.send_ctap1(&assertion).unwrap();

        // Check if response is correct
        let expected_auth_data = AuthenticatorData {
            rp_id_hash: RpIdHash(RELYING_PARTY_HASH),
            flags: AuthenticatorDataFlags::USER_PRESENT,
            counter: 0x3B,
            credential_data: None,
            extensions: Default::default(),
        };

        let expected_assertion = Assertion {
            credentials: Some(ok_key_handle),
            signature: vec![
                0x30, 0x44, 0x02, 0x20, 0x7B, 0xDE, 0x0A, 0x52, 0xAC, 0x1F, 0x4C, 0x8B, 0x27, 0xE0,
                0x03, 0xA3, 0x70, 0xCD, 0x66, 0xA4, 0xC7, 0x11, 0x8D, 0xD2, 0x2D, 0x54, 0x47, 0x83,
                0x5F, 0x45, 0xB9, 0x9C, 0x68, 0x42, 0x3F, 0xF7, 0x02, 0x20, 0x3C, 0x51, 0x7B, 0x47,
                0x87, 0x7F, 0x85, 0x78, 0x2D, 0xE1, 0x00, 0x86, 0xA7, 0x83, 0xD1, 0xE7, 0xDF, 0x4E,
                0x36, 0x39, 0xE7, 0x71, 0xF5, 0xF6, 0xAF, 0xA3, 0x5A, 0xAD, 0x53, 0x73, 0x85, 0x8E,
            ],
            user: None,
            auth_data: expected_auth_data,
        };

        let expected = vec![GetAssertionResult {
            assertion: expected_assertion,
            attachment: AuthenticatorAttachment::Unknown,
            extensions: Default::default(),
        }];
        assert_eq!(response, expected);
    }

    #[test]
    fn test_get_assertion_ctap2_pre_flight() {
        let client_data = CollectedClientData {
            webauthn_type: WebauthnType::Create,
            challenge: Challenge::from(vec![0x00, 0x01, 0x02, 0x03]),
            origin: String::from("example.com"),
            cross_origin: false,
            token_binding: Some(TokenBinding::Present(String::from("AAECAw"))),
        };
        let assertion = GetAssertion::new(
            client_data.hash().expect("failed to serialize client data"),
            RelyingParty::from("example.com"),
            vec![
                // This should never be tested, because it gets pre-filtered, since it is too long
                // (see max_credential_id_length)
                PublicKeyCredentialDescriptor {
                    id: vec![0x10; 100],
                    transports: vec![Transport::USB],
                },
                // One we test and skip
                PublicKeyCredentialDescriptor {
                    id: vec![
                        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
                        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
                        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
                        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
                        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
                        0x11, 0x11, 0x11, 0x11,
                    ],
                    transports: vec![Transport::USB],
                },
                // This one is the 'right' one
                PublicKeyCredentialDescriptor {
                    id: vec![
                        0x3E, 0xBD, 0x89, 0xBF, 0x77, 0xEC, 0x50, 0x97, 0x55, 0xEE, 0x9C, 0x26,
                        0x35, 0xEF, 0xAA, 0xAC, 0x7B, 0x2B, 0x9C, 0x5C, 0xEF, 0x17, 0x36, 0xC3,
                        0x71, 0x7D, 0xA4, 0x85, 0x34, 0xC8, 0xC6, 0xB6, 0x54, 0xD7, 0xFF, 0x94,
                        0x5F, 0x50, 0xB5, 0xCC, 0x4E, 0x78, 0x05, 0x5B, 0xDD, 0x39, 0x6B, 0x64,
                        0xF7, 0x8D, 0xA2, 0xC5, 0xF9, 0x62, 0x00, 0xCC, 0xD4, 0x15, 0xCD, 0x08,
                        0xFE, 0x42, 0x00, 0x38,
                    ],
                    transports: vec![Transport::USB],
                },
                // We should never test this one
                PublicKeyCredentialDescriptor {
                    id: vec![
                        0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
                        0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
                        0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
                        0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
                        0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22, 0x22,
                        0x22, 0x22, 0x22, 0x22,
                    ],
                    transports: vec![Transport::USB],
                },
            ],
            GetAssertionOptions {
                user_presence: Some(true),
                user_verification: None,
            },
            Default::default(),
        );
        let mut device = Device::new("commands/get_assertion").unwrap();
        assert_eq!(device.get_protocol(), FidoProtocol::CTAP2);
        let mut cid = [0u8; 4];
        thread_rng().fill_bytes(&mut cid);
        device.set_cid(cid);
        device.set_device_info(U2FDeviceInfo {
            vendor_name: Vec::new(),
            device_name: Vec::new(),
            version_interface: 0x02,
            version_major: 0x04,
            version_minor: 0x01,
            version_build: 0x08,
            cap_flags: Capability::WINK | Capability::CBOR,
        });
        device.set_authenticator_info(AuthenticatorInfo {
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
            max_credential_id_length: Some(80),
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
        });

        // Sending first GetAssertion with first allow_list-entry, that will return an error
        let mut msg = cid.to_vec();
        msg.extend(vec![HIDCmd::Cbor.into(), 0x00, 0x90]);
        msg.extend(vec![0x2]); // u2f command
        msg.extend(vec![
            0xa4, // map(4)
            0x1,  // rpid
            0x6b, // text(11)
            101, 120, 97, 109, 112, 108, 101, 46, 99, 111, 109, // example.com
            0x2, // clientDataHash
            0x58, 0x20, //bytes(32)
            0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f,
            0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b,
            0x78, 0x52, 0xb8, 0x55, // empty hash
            0x3,  //allowList
            0x81, // array(1)
            0xa2, // map(2)
            0x62, // text(2)
            0x69, 0x64, // id
            0x58, // bytes(
        ]);
        device.add_write(&msg, 0);

        msg = cid.to_vec();
        msg.extend([0x0]); //SEQ
        msg.extend([0x40]); // 64)
        msg.extend(&assertion.allow_list[1].id[..58]);
        device.add_write(&msg, 0);

        msg = cid.to_vec();
        msg.extend([0x1]); //SEQ
        msg.extend(&assertion.allow_list[1].id[58..64]);
        msg.extend(vec![
            0x64, // text(4),
            0x74, 0x79, 0x70, 0x65, // type
            0x6a, // text(10)
            0x70, 0x75, 0x62, 0x6C, 0x69, 0x63, 0x2D, 0x6B, 0x65, 0x79, // public-key
            0x5,  // options
            0xa1, // map(1)
            0x62, // text(2)
            0x75, 0x70, // up
            0xf4, // false
        ]);
        device.add_write(&msg, 0);

        // fido response
        let len = 0x1;
        let mut msg = cid.to_vec();
        msg.extend(vec![HIDCmd::Cbor.into(), 0x00, len]); // cmd + bcnt
        msg.push(0x2e); // Status code: NoCredentials
        device.add_read(&msg, 0);

        // Sending second GetAssertion with first allow_list-entry, that will return a success
        let mut msg = cid.to_vec();
        msg.extend(vec![HIDCmd::Cbor.into(), 0x00, 0x90]);
        msg.extend(vec![0x2]); // u2f command
        msg.extend(vec![
            0xa4, // map(4)
            0x1,  // rpid
            0x6b, // text(11)
            101, 120, 97, 109, 112, 108, 101, 46, 99, 111, 109, // example.com
            0x2, // clientDataHash
            0x58, 0x20, //bytes(32)
            0xe3, 0xb0, 0xc4, 0x42, 0x98, 0xfc, 0x1c, 0x14, 0x9a, 0xfb, 0xf4, 0xc8, 0x99, 0x6f,
            0xb9, 0x24, 0x27, 0xae, 0x41, 0xe4, 0x64, 0x9b, 0x93, 0x4c, 0xa4, 0x95, 0x99, 0x1b,
            0x78, 0x52, 0xb8, 0x55, // empty hash
            0x3,  //allowList
            0x81, // array(1)
            0xa2, // map(2)
            0x62, // text(2)
            0x69, 0x64, // id
            0x58, // bytes(
        ]);
        device.add_write(&msg, 0);

        msg = cid.to_vec();
        msg.extend([0x0]); //SEQ
        msg.extend([0x40]); // 64)
        msg.extend(&assertion.allow_list[2].id[..58]);
        device.add_write(&msg, 0);

        msg = cid.to_vec();
        msg.extend([0x1]); //SEQ
        msg.extend(&assertion.allow_list[2].id[58..64]);
        msg.extend(vec![
            0x64, // text(4),
            0x74, 0x79, 0x70, 0x65, // type
            0x6a, // text(10)
            0x70, 0x75, 0x62, 0x6C, 0x69, 0x63, 0x2D, 0x6B, 0x65, 0x79, // public-key
            0x5,  // options
            0xa1, // map(1)
            0x62, // text(2)
            0x75, 0x70, // up
            0xf4, // false
        ]);
        device.add_write(&msg, 0);

        let mut msg = cid.to_vec();
        msg.extend([HIDCmd::Cbor.into(), 0x1, 0x2a]); // cmd + bcnt
        msg.extend(&GET_ASSERTION_SAMPLE_RESPONSE_CTAP2[..57]);
        device.add_read(&msg, 0);

        let mut msg = cid.to_vec();
        msg.extend([0x0]); // SEQ
        msg.extend(&GET_ASSERTION_SAMPLE_RESPONSE_CTAP2[57..116]);
        device.add_read(&msg, 0);

        let mut msg = cid.to_vec();
        msg.extend([0x1]); // SEQ
        msg.extend(&GET_ASSERTION_SAMPLE_RESPONSE_CTAP2[116..175]);
        device.add_read(&msg, 0);

        let mut msg = cid.to_vec();
        msg.extend([0x2]); // SEQ
        msg.extend(&GET_ASSERTION_SAMPLE_RESPONSE_CTAP2[175..234]);
        device.add_read(&msg, 0);

        let mut msg = cid.to_vec();
        msg.extend([0x3]); // SEQ
        msg.extend(&GET_ASSERTION_SAMPLE_RESPONSE_CTAP2[234..293]);
        device.add_read(&msg, 0);
        let mut msg = cid.to_vec();
        msg.extend([0x4]); // SEQ
        msg.extend(&GET_ASSERTION_SAMPLE_RESPONSE_CTAP2[293..]);
        device.add_read(&msg, 0);

        assert_matches!(
            do_credential_list_filtering_ctap2(
                &mut device,
                &assertion.allow_list,
                &assertion.rp,
                None,
            ),
            Ok(..)
        );
    }

    #[test]
    fn test_get_assertion_ctap1_flags() {
        // Ensure that only the two low bits of flags are preserved when repackaging a
        // CTAP1 response.
        let mut sample = GET_ASSERTION_SAMPLE_RESPONSE_CTAP1.to_vec();
        sample[0] = 0xff; // Set all 8 flag bits before repackaging
        let add_info = PublicKeyCredentialDescriptor {
            id: vec![],
            transports: vec![],
        };
        let rp_hash = RpIdHash([0u8; 32]);
        let resp = GetAssertionResult::from_ctap1(&sample, &rp_hash, &add_info)
            .expect("could not handle response");
        assert_eq!(
            resp.assertion.auth_data.flags,
            AuthenticatorDataFlags::USER_PRESENT | AuthenticatorDataFlags::RESERVED_1
        );
    }

    // Manually assembled according to https://www.w3.org/TR/webauthn-2/#clientdatajson-serialization
    const CLIENT_DATA_VEC: [u8; 140] = [
        0x7b, 0x22, 0x74, 0x79, 0x70, 0x65, 0x22, 0x3a, // {"type":
        0x22, 0x77, 0x65, 0x62, 0x61, 0x75, 0x74, 0x68, 0x6e, 0x2e, 0x63, 0x72, 0x65, 0x61, 0x74,
        0x65, 0x22, // "webauthn.create"
        0x2c, 0x22, 0x63, 0x68, 0x61, 0x6c, 0x6c, 0x65, 0x6e, 0x67, 0x65, 0x22,
        0x3a, // (,"challenge":
        0x22, 0x41, 0x41, 0x45, 0x43, 0x41, 0x77, 0x22, // challenge in base64
        0x2c, 0x22, 0x6f, 0x72, 0x69, 0x67, 0x69, 0x6e, 0x22, 0x3a, // ,"origin":
        0x22, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d,
        0x22, // "example.com"
        0x2c, 0x22, 0x63, 0x72, 0x6f, 0x73, 0x73, 0x4f, 0x72, 0x69, 0x67, 0x69, 0x6e, 0x22,
        0x3a, // ,"crossOrigin":
        0x66, 0x61, 0x6c, 0x73, 0x65, // false
        0x2c, 0x22, 0x74, 0x6f, 0x6b, 0x65, 0x6e, 0x42, 0x69, 0x6e, 0x64, 0x69, 0x6e, 0x67, 0x22,
        0x3a, // ,"tokenBinding":
        0x7b, 0x22, 0x73, 0x74, 0x61, 0x74, 0x75, 0x73, 0x22, 0x3a, // {"status":
        0x22, 0x70, 0x72, 0x65, 0x73, 0x65, 0x6e, 0x74, 0x22, // "present"
        0x2c, 0x22, 0x69, 0x64, 0x22, 0x3a, // ,"id":
        0x22, 0x41, 0x41, 0x45, 0x43, 0x41, 0x77, 0x22, // "AAECAw"
        0x7d, // }
        0x7d, // }
    ];

    const CLIENT_DATA_HASH: [u8; 32] = [
        0x75, 0x35, 0x35, 0x7d, 0x49, 0x6e, 0x33, 0xc8, 0x18, 0x7f, 0xea, 0x8d, 0x11, // hash
        0x32, 0x64, 0xaa, 0xa4, 0x52, 0x3e, 0x13, 0x40, 0x14, 0x9f, 0xbe, 0x00, 0x3f, // hash
        0x10, 0x87, 0x54, 0xc3, 0x2d, 0x80, // hash
    ];

    const RELYING_PARTY_HASH: [u8; 32] = [
        0xA3, 0x79, 0xA6, 0xF6, 0xEE, 0xAF, 0xB9, 0xA5, 0x5E, 0x37, 0x8C, 0x11, 0x80, 0x34, 0xE2,
        0x75, 0x1E, 0x68, 0x2F, 0xAB, 0x9F, 0x2D, 0x30, 0xAB, 0x13, 0xD2, 0x12, 0x55, 0x86, 0xCE,
        0x19, 0x47,
    ];
    const KEY_HANDLE: [u8; 64] = [
        0x3E, 0xBD, 0x89, 0xBF, 0x77, 0xEC, 0x50, 0x97, 0x55, 0xEE, 0x9C, 0x26, 0x35, 0xEF, 0xAA,
        0xAC, 0x7B, 0x2B, 0x9C, 0x5C, 0xEF, 0x17, 0x36, 0xC3, 0x71, 0x7D, 0xA4, 0x85, 0x34, 0xC8,
        0xC6, 0xB6, 0x54, 0xD7, 0xFF, 0x94, 0x5F, 0x50, 0xB5, 0xCC, 0x4E, 0x78, 0x05, 0x5B, 0xDD,
        0x39, 0x6B, 0x64, 0xF7, 0x8D, 0xA2, 0xC5, 0xF9, 0x62, 0x00, 0xCC, 0xD4, 0x15, 0xCD, 0x08,
        0xFE, 0x42, 0x00, 0x38,
    ];

    const GET_ASSERTION_SAMPLE_REQUEST_CTAP1: [u8; 138] = [
        // CBOR Header
        0x0, // CLA
        0x2, // INS U2F_Authenticate
        0x3, // P1 Flags (user presence)
        0x0, // P2
        0x0, 0x0, 0x81, // Lc
        // NOTE: This has been taken from CTAP2.0 spec, but the clientDataHash has been replaced
        //       to be able to operate with known values for CollectedClientData (spec doesn't say
        //       what values led to the provided example hash)
        // clientDataHash:
        0x75, 0x35, 0x35, 0x7d, 0x49, 0x6e, 0x33, 0xc8, 0x18, 0x7f, 0xea, 0x8d, 0x11, // hash
        0x32, 0x64, 0xaa, 0xa4, 0x52, 0x3e, 0x13, 0x40, 0x14, 0x9f, 0xbe, 0x00, 0x3f, // hash
        0x10, 0x87, 0x54, 0xc3, 0x2d, 0x80, // hash
        // rpIdHash:
        0xA3, 0x79, 0xA6, 0xF6, 0xEE, 0xAF, 0xB9, 0xA5, 0x5E, 0x37, 0x8C, 0x11, 0x80, 0x34, 0xE2,
        0x75, 0x1E, 0x68, 0x2F, 0xAB, 0x9F, 0x2D, 0x30, 0xAB, 0x13, 0xD2, 0x12, 0x55, 0x86, 0xCE,
        0x19, 0x47, // ..
        // Key Handle Length (1 Byte):
        0x40, // ..
        // Key Handle (Key Handle Length Bytes):
        0x3E, 0xBD, 0x89, 0xBF, 0x77, 0xEC, 0x50, 0x97, 0x55, 0xEE, 0x9C, 0x26, 0x35, 0xEF, 0xAA,
        0xAC, 0x7B, 0x2B, 0x9C, 0x5C, 0xEF, 0x17, 0x36, 0xC3, 0x71, 0x7D, 0xA4, 0x85, 0x34, 0xC8,
        0xC6, 0xB6, 0x54, 0xD7, 0xFF, 0x94, 0x5F, 0x50, 0xB5, 0xCC, 0x4E, 0x78, 0x05, 0x5B, 0xDD,
        0x39, 0x6B, 0x64, 0xF7, 0x8D, 0xA2, 0xC5, 0xF9, 0x62, 0x00, 0xCC, 0xD4, 0x15, 0xCD, 0x08,
        0xFE, 0x42, 0x00, 0x38, // ..
        // Le (Ne=65536):
        0x0, 0x0,
    ];

    const GET_ASSERTION_SAMPLE_REQUEST_CTAP2: [u8; 138] = [
        // CBOR Header
        0x0, // leading zero
        0x2, // CMD U2F_Authenticate
        0x3, // Flags (user presence)
        0x0, 0x0, // zero bits
        0x0, 0x81, // size
        // NOTE: This has been taken from CTAP2.0 spec, but the clientDataHash has been replaced
        //       to be able to operate with known values for CollectedClientData (spec doesn't say
        //       what values led to the provided example hash)
        // clientDataHash:
        0x75, 0x35, 0x35, 0x7d, 0x49, 0x6e, 0x33, 0xc8, 0x18, 0x7f, 0xea, 0x8d, 0x11, 0x32, 0x64,
        0xaa, 0xa4, 0x52, 0x3e, 0x13, 0x40, 0x14, 0x9f, 0xbe, 0x00, 0x3f, 0x10, 0x87, 0x54, 0xc3,
        0x2d, 0x80, // hash
        // rpIdHash:
        0xA3, 0x79, 0xA6, 0xF6, 0xEE, 0xAF, 0xB9, 0xA5, 0x5E, 0x37, 0x8C, 0x11, 0x80, 0x34, 0xE2,
        0x75, 0x1E, 0x68, 0x2F, 0xAB, 0x9F, 0x2D, 0x30, 0xAB, 0x13, 0xD2, 0x12, 0x55, 0x86, 0xCE,
        0x19, 0x47, // ..
        // Key Handle Length (1 Byte):
        0x40, // ..
        // Key Handle (Key Handle Length Bytes):
        0x3E, 0xBD, 0x89, 0xBF, 0x77, 0xEC, 0x50, 0x97, 0x55, 0xEE, 0x9C, 0x26, 0x35, 0xEF, 0xAA,
        0xAC, 0x7B, 0x2B, 0x9C, 0x5C, 0xEF, 0x17, 0x36, 0xC3, 0x71, 0x7D, 0xA4, 0x85, 0x34, 0xC8,
        0xC6, 0xB6, 0x54, 0xD7, 0xFF, 0x94, 0x5F, 0x50, 0xB5, 0xCC, 0x4E, 0x78, 0x05, 0x5B, 0xDD,
        0x39, 0x6B, 0x64, 0xF7, 0x8D, 0xA2, 0xC5, 0xF9, 0x62, 0x00, 0xCC, 0xD4, 0x15, 0xCD, 0x08,
        0xFE, 0x42, 0x00, 0x38, 0x0, 0x0, // 2 trailing zeros from protocol
    ];

    const GET_ASSERTION_SAMPLE_RESPONSE_CTAP1: [u8; 75] = [
        0x01, // User Presence (1 Byte)
        0x00, 0x00, 0x00, 0x3B, // Sign Count (4 Bytes)
        // Signature (variable Length)
        0x30, 0x44, 0x02, 0x20, 0x7B, 0xDE, 0x0A, 0x52, 0xAC, 0x1F, 0x4C, 0x8B, 0x27, 0xE0, 0x03,
        0xA3, 0x70, 0xCD, 0x66, 0xA4, 0xC7, 0x11, 0x8D, 0xD2, 0x2D, 0x54, 0x47, 0x83, 0x5F, 0x45,
        0xB9, 0x9C, 0x68, 0x42, 0x3F, 0xF7, 0x02, 0x20, 0x3C, 0x51, 0x7B, 0x47, 0x87, 0x7F, 0x85,
        0x78, 0x2D, 0xE1, 0x00, 0x86, 0xA7, 0x83, 0xD1, 0xE7, 0xDF, 0x4E, 0x36, 0x39, 0xE7, 0x71,
        0xF5, 0xF6, 0xAF, 0xA3, 0x5A, 0xAD, 0x53, 0x73, 0x85, 0x8E,
    ];

    const GET_ASSERTION_SAMPLE_RESPONSE_CTAP2: [u8; 298] = [
        0x00, // status == success
        0xA5, // map(5)
        0x01, // unsigned(1)
        0xA2, // map(2)
        0x62, // text(2)
        0x69, 0x64, // "id"
        0x58, 0x40, // bytes(0x64, ) credential_id
        0xF2, 0x20, 0x06, 0xDE, 0x4F, 0x90, 0x5A, 0xF6, 0x8A, 0x43, 0x94, 0x2F, 0x02, 0x4F, 0x2A,
        0x5E, 0xCE, 0x60, 0x3D, 0x9C, 0x6D, 0x4B, 0x3D, 0xF8, 0xBE, 0x08, 0xED, 0x01, 0xFC, 0x44,
        0x26, 0x46, 0xD0, 0x34, 0x85, 0x8A, 0xC7, 0x5B, 0xED, 0x3F, 0xD5, 0x80, 0xBF, 0x98, 0x08,
        0xD9, 0x4F, 0xCB, 0xEE, 0x82, 0xB9, 0xB2, 0xEF, 0x66, 0x77, 0xAF, 0x0A, 0xDC, 0xC3, 0x58,
        0x52, 0xEA, 0x6B, 0x9E, // end: credential_id
        0x64, // text(4)
        0x74, 0x79, 0x70, 0x65, // "type"
        0x6A, // text(0x10, )
        0x70, 0x75, 0x62, 0x6C, 0x69, 0x63, 0x2D, 0x6B, 0x65, 0x79, // "public-key"
        0x02, // unsigned(2)
        0x58, 0x25, // bytes(0x37, ) auth_data
        0x62, 0x5D, 0xDA, 0xDF, 0x74, 0x3F, 0x57, 0x27, 0xE6, 0x6B, 0xBA, 0x8C, 0x2E, 0x38, 0x79,
        0x22, 0xD1, 0xAF, 0x43, 0xC5, 0x03, 0xD9, 0x11, 0x4A, 0x8F, 0xBA, 0x10, 0x4D, 0x84, 0xD0,
        0x2B, 0xFA, 0x01, 0x00, 0x00, 0x00, 0x11, // end: auth_data
        0x03, // unsigned(3)
        0x58, 0x47, // bytes(0x71, ) signature
        0x30, 0x45, 0x02, 0x20, 0x4A, 0x5A, 0x9D, 0xD3, 0x92, 0x98, 0x14, 0x9D, 0x90, 0x47, 0x69,
        0xB5, 0x1A, 0x45, 0x14, 0x33, 0x00, 0x6F, 0x18, 0x2A, 0x34, 0xFB, 0xDF, 0x66, 0xDE, 0x5F,
        0xC7, 0x17, 0xD7, 0x5F, 0xB3, 0x50, 0x02, 0x21, 0x00, 0xA4, 0x6B, 0x8E, 0xA3, 0xC3, 0xB9,
        0x33, 0x82, 0x1C, 0x6E, 0x7F, 0x5E, 0xF9, 0xDA, 0xAE, 0x94, 0xAB, 0x47, 0xF1, 0x8D, 0xB4,
        0x74, 0xC7, 0x47, 0x90, 0xEA, 0xAB, 0xB1, 0x44, 0x11, 0xE7, 0xA0, // end: signature
        0x04, // unsigned(4)
        0xA3, // map(3)
        0x62, // text(2)
        0x69, 0x64, // "id"
        0x58, 0x20, // bytes(0x32, ) user_id
        0x30, 0x82, 0x01, 0x93, 0x30, 0x82, 0x01, 0x38, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x30, 0x82,
        0x01, 0x93, 0x30, 0x82, 0x01, 0x38, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x30, 0x82, 0x01, 0x93,
        0x30, 0x82, // end: user_id
        0x64, // text(4)
        0x6E, 0x61, 0x6D, 0x65, // "name"
        0x76, // text(0x22, )
        0x6A, 0x6F, 0x68, 0x6E, 0x70, 0x73, 0x6D, 0x69, 0x74, 0x68, 0x40, 0x65, 0x78, 0x61, 0x6D,
        0x70, 0x6C, 0x65, 0x2E, 0x63, 0x6F, 0x6D, // "johnpsmith@example.com"
        0x6B, // text(0x11, )
        0x64, 0x69, 0x73, 0x70, 0x6C, 0x61, 0x79, 0x4E, 0x61, 0x6D, 0x65, // "displayName"
        0x6D, // text(0x13, )
        0x4A, 0x6F, 0x68, 0x6E, 0x20, 0x50, 0x2E, 0x20, 0x53, 0x6D, 0x69, 0x74,
        0x68, // "John P. Smith"
        0x05, // unsigned(5)
        0x01, // unsigned(1)
    ];
}
