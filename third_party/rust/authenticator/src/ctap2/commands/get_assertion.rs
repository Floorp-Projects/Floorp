use super::{
    Command, CommandError, PinAuthCommand, Request, RequestCtap1, RequestCtap2, Retryable,
    StatusCode,
};
use crate::consts::{
    PARAMETER_SIZE, U2F_AUTHENTICATE, U2F_CHECK_IS_REGISTERED, U2F_REQUEST_USER_PRESENCE,
};
use crate::crypto::{COSEKey, CryptoError, PinUvAuthParam, SharedSecret};
use crate::ctap2::attestation::{AuthenticatorData, AuthenticatorDataFlags};
use crate::ctap2::client_data::{ClientDataHash, CollectedClientData, CollectedClientDataWrapper};
use crate::ctap2::commands::client_pin::Pin;
use crate::ctap2::commands::get_next_assertion::GetNextAssertion;
use crate::ctap2::commands::make_credentials::UserVerification;
use crate::ctap2::server::{PublicKeyCredentialDescriptor, RelyingPartyWrapper, User};
use crate::errors::AuthenticatorError;
use crate::transport::errors::{ApduErrorStatus, HIDError};
use crate::transport::FidoDevice;
use crate::u2ftypes::{CTAP1RequestAPDU, U2FDevice};
use nom::{
    error::VerboseError,
    number::complete::{be_u32, be_u8},
    sequence::tuple,
};
use serde::{
    de::{Error as DesError, MapAccess, Visitor},
    ser::{Error as SerError, SerializeMap},
    Deserialize, Deserializer, Serialize, Serializer,
};
use serde_bytes::ByteBuf;
use serde_cbor::{de::from_slice, ser, Value};
use std::fmt;
use std::io;

#[derive(Debug, PartialEq, Eq)]
pub enum GetAssertionResult {
    CTAP1(Vec<u8>),
    CTAP2(AssertionObject, CollectedClientDataWrapper),
}

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

#[derive(Debug, Clone, Default)]
pub struct GetAssertionExtensions {
    pub hmac_secret: Option<HmacSecretExtension>,
}

impl Serialize for GetAssertionExtensions {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut map = serializer.serialize_map(Some(1))?;
        map.serialize_entry(&"hmac-secret", &self.hmac_secret)?;
        map.end()
    }
}

impl GetAssertionExtensions {
    fn has_extensions(&self) -> bool {
        self.hmac_secret.is_some()
    }
}

#[derive(Debug, Clone)]
pub struct GetAssertion {
    pub(crate) client_data_wrapper: CollectedClientDataWrapper,
    pub(crate) rp: RelyingPartyWrapper,
    pub(crate) allow_list: Vec<PublicKeyCredentialDescriptor>,

    // https://www.w3.org/TR/webauthn/#client-extension-input
    // The client extension input, which is a value that can be encoded in JSON,
    // is passed from the WebAuthn Relying Party to the client in the get() or
    // create() call, while the CBOR authenticator extension input is passed
    // from the client to the authenticator for authenticator extensions during
    // the processing of these calls.
    pub(crate) extensions: GetAssertionExtensions,
    pub(crate) options: GetAssertionOptions,
    pub(crate) pin: Option<Pin>,
    pub(crate) pin_auth: Option<PinUvAuthParam>,

    // This is used to implement the FIDO AppID extension.
    pub(crate) alternate_rp_id: Option<String>,
}

impl GetAssertion {
    pub fn new(
        client_data_wrapper: CollectedClientData,
        rp: RelyingPartyWrapper,
        allow_list: Vec<PublicKeyCredentialDescriptor>,
        options: GetAssertionOptions,
        extensions: GetAssertionExtensions,
        pin: Option<Pin>,
        alternate_rp_id: Option<String>,
    ) -> Result<Self, HIDError> {
        let client_data_wrapper = CollectedClientDataWrapper::new(client_data_wrapper)?;
        Ok(Self {
            client_data_wrapper,
            rp,
            allow_list,
            extensions,
            options,
            pin,
            pin_auth: None,
            alternate_rp_id,
        })
    }
}

impl PinAuthCommand for GetAssertion {
    fn pin(&self) -> &Option<Pin> {
        &self.pin
    }

    fn set_pin(&mut self, pin: Option<Pin>) {
        self.pin = pin;
    }

    fn pin_auth(&self) -> &Option<PinUvAuthParam> {
        &self.pin_auth
    }

    fn set_pin_auth(&mut self, pin_auth: Option<PinUvAuthParam>) {
        self.pin_auth = pin_auth;
    }

    fn client_data_hash(&self) -> ClientDataHash {
        self.client_data_wrapper.hash()
    }

    fn unset_uv_option(&mut self) {
        self.options.user_verification = None;
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
        if self.extensions.has_extensions() {
            map_len += 1;
        }
        if self.options.has_some() {
            map_len += 1;
        }
        if self.pin_auth.is_some() {
            map_len += 2;
        }

        let mut map = serializer.serialize_map(Some(map_len))?;
        match self.rp {
            RelyingPartyWrapper::Data(ref d) => {
                map.serialize_entry(&1, &d.id)?;
            }
            _ => {
                return Err(S::Error::custom(
                    "Can't serialize a RelyingParty::Hash for CTAP2",
                ));
            }
        }

        let client_data_hash = self.client_data_hash();
        map.serialize_entry(&2, &client_data_hash)?;
        if !self.allow_list.is_empty() {
            map.serialize_entry(&3, &self.allow_list)?;
        }
        if self.extensions.has_extensions() {
            map.serialize_entry(&4, &self.extensions)?;
        }
        if self.options.has_some() {
            map.serialize_entry(&5, &self.options)?;
        }
        if let Some(pin_auth) = &self.pin_auth {
            map.serialize_entry(&6, &pin_auth)?;
            map.serialize_entry(&7, &pin_auth.pin_protocol.id())?;
        }
        map.end()
    }
}

impl Request<GetAssertionResult> for GetAssertion {
    fn is_ctap2_request(&self) -> bool {
        match self.rp {
            RelyingPartyWrapper::Data(_) => true,
            RelyingPartyWrapper::Hash(_) => false,
        }
    }
}

/// This command is used to check which key_handle is valid for this
/// token. This is sent before a GetAssertion command, to determine which
/// is valid for a specific token and which key_handle GetAssertion
/// should send to the token. Or before a MakeCredential command, to determine
/// if this token is already registered or not.
#[derive(Debug)]
pub(crate) struct CheckKeyHandle<'assertion> {
    pub(crate) key_handle: &'assertion [u8],
    pub(crate) client_data_wrapper: &'assertion CollectedClientDataWrapper,
    pub(crate) rp: &'assertion RelyingPartyWrapper,
}

impl<'assertion> RequestCtap1 for CheckKeyHandle<'assertion> {
    type Output = ();
    type AdditionalInfo = ();

    fn ctap1_format<Dev>(&self, _dev: &mut Dev) -> Result<(Vec<u8>, Self::AdditionalInfo), HIDError>
    where
        Dev: U2FDevice + io::Read + io::Write + fmt::Debug,
    {
        let flags = U2F_CHECK_IS_REGISTERED;
        // TODO(MS): Need to check "up" here. If up==false, set to 0x08? Or not? Spec is
        // ambiguous
        let mut auth_data = Vec::with_capacity(2 * PARAMETER_SIZE + 1 + self.key_handle.len());

        auth_data.extend_from_slice(self.client_data_wrapper.hash().as_ref());
        auth_data.extend_from_slice(self.rp.hash().as_ref());
        auth_data.extend_from_slice(&[self.key_handle.len() as u8]);
        auth_data.extend_from_slice(self.key_handle);
        let cmd = U2F_AUTHENTICATE;
        let apdu = CTAP1RequestAPDU::serialize(cmd, flags, &auth_data)?;
        Ok((apdu, ()))
    }

    fn handle_response_ctap1(
        &self,
        status: Result<(), ApduErrorStatus>,
        _input: &[u8],
        _add_info: &Self::AdditionalInfo,
    ) -> Result<Self::Output, Retryable<HIDError>> {
        // From the U2F-spec: https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#registration-request-message---u2f_register
        // if the control byte is set to 0x07 by the FIDO Client, the U2F token is supposed to
        // simply check whether the provided key handle was originally created by this token,
        // and whether it was created for the provided application parameter. If so, the U2F
        // token MUST respond with an authentication response
        // message:error:test-of-user-presence-required (note that despite the name this
        // signals a success condition). If the key handle was not created by this U2F
        // token, or if it was created for a different application parameter, the token MUST
        // respond with an authentication response message:error:bad-key-handle.
        match status {
            Ok(_) | Err(ApduErrorStatus::ConditionsNotSatisfied) => Ok(()),
            Err(e) => Err(Retryable::Error(HIDError::ApduStatus(e))),
        }
    }
}

impl RequestCtap1 for GetAssertion {
    type Output = GetAssertionResult;
    type AdditionalInfo = PublicKeyCredentialDescriptor;

    fn ctap1_format<Dev>(&self, dev: &mut Dev) -> Result<(Vec<u8>, Self::AdditionalInfo), HIDError>
    where
        Dev: io::Read + io::Write + fmt::Debug + FidoDevice,
    {
        let key_handle = self
            .allow_list
            .iter()
            // key-handles in CTAP1 are limited to 255 bytes, but are not limited in CTAP2.
            // Filter out key-handles that are too long (can happen if this is a CTAP2-request,
            // but the token only speaks CTAP1). If none is found, return an error.
            .filter(|allowed_handle| allowed_handle.id.len() < 256)
            .find_map(|allowed_handle| {
                let check_command = CheckKeyHandle {
                    key_handle: allowed_handle.id.as_ref(),
                    client_data_wrapper: &self.client_data_wrapper,
                    rp: &self.rp,
                };
                let res = dev.send_ctap1(&check_command);
                match res {
                    Ok(_) => Some(allowed_handle.clone()),
                    _ => None,
                }
            })
            .ok_or(HIDError::Command(CommandError::StatusCode(
                StatusCode::NoCredentials,
                None,
            )))?;

        debug!("sending key_handle = {:?}", key_handle);

        let flags = if self.options.user_presence.unwrap_or(false) {
            U2F_REQUEST_USER_PRESENCE
        } else {
            0
        };
        let mut auth_data =
            Vec::with_capacity(2 * PARAMETER_SIZE + 1 /* key_handle_len */ + key_handle.id.len());

        if self.is_ctap2_request() {
            auth_data.extend_from_slice(self.client_data_hash().as_ref());
        } else {
            let decoded = base64::decode_config(
                &self.client_data_wrapper.client_data.challenge.0,
                base64::URL_SAFE_NO_PAD,
            )
            .map_err(|_| HIDError::DeviceError)?; // We encoded it, so this should never fail
            auth_data.extend_from_slice(&decoded);
        }
        auth_data.extend_from_slice(self.rp.hash().as_ref());
        auth_data.extend_from_slice(&[key_handle.id.len() as u8]);
        auth_data.extend_from_slice(key_handle.id.as_ref());

        let cmd = U2F_AUTHENTICATE;
        let apdu = CTAP1RequestAPDU::serialize(cmd, flags, &auth_data)?;
        Ok((apdu, key_handle))
    }

    fn handle_response_ctap1(
        &self,
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

        if self.is_ctap2_request() {
            let parse_authentication = |input| {
                // Parsing an u8, then a u32, and the rest is the signature
                let (rest, (user_presence, counter)) = tuple((be_u8, be_u32))(input)?;
                let signature = Vec::from(rest);
                Ok((user_presence, counter, signature))
            };
            let (user_presence, counter, signature) = parse_authentication(input)
                .map_err(|e: nom::Err<VerboseError<_>>| {
                    error!("error while parsing authentication: {:?}", e);
                    CommandError::Deserializing(DesError::custom("unable to parse authentication"))
                })
                .map_err(HIDError::Command)
                .map_err(Retryable::Error)?;

            let mut flags = AuthenticatorDataFlags::empty();
            if user_presence == 1 {
                flags |= AuthenticatorDataFlags::USER_PRESENT;
            }
            let auth_data = AuthenticatorData {
                rp_id_hash: self.rp.hash(),
                flags,
                counter,
                credential_data: None,
                extensions: Default::default(),
            };
            let assertion = Assertion {
                credentials: Some(add_info.clone()),
                signature,
                user: None,
                auth_data,
            };

            Ok(GetAssertionResult::CTAP2(
                AssertionObject(vec![assertion]),
                self.client_data_wrapper.clone(),
            ))
        } else {
            Ok(GetAssertionResult::CTAP1(input.to_vec()))
        }
    }
}

impl RequestCtap2 for GetAssertion {
    type Output = GetAssertionResult;

    fn command() -> Command {
        Command::GetAssertion
    }

    fn wire_format<Dev>(&self, _dev: &mut Dev) -> Result<Vec<u8>, HIDError>
    where
        Dev: FidoDevice + io::Read + io::Write + fmt::Debug,
    {
        Ok(ser::to_vec(&self).map_err(CommandError::Serializing)?)
    }

    fn handle_response_ctap2<Dev>(
        &self,
        dev: &mut Dev,
        input: &[u8],
    ) -> Result<Self::Output, HIDError>
    where
        Dev: FidoDevice + io::Read + io::Write + fmt::Debug,
    {
        if input.is_empty() {
            return Err(CommandError::InputTooSmall.into());
        }

        let status: StatusCode = input[0].into();
        debug!(
            "response status code: {:?}, rest: {:?}",
            status,
            &input[1..]
        );
        if input.len() > 1 {
            if status.is_ok() {
                let assertion: GetAssertionResponse =
                    from_slice(&input[1..]).map_err(CommandError::Deserializing)?;
                let number_of_credentials = assertion.number_of_credentials.unwrap_or(1);
                let mut assertions = Vec::with_capacity(number_of_credentials);
                assertions.push(assertion.into());

                let msg = GetNextAssertion;
                // We already have one, so skipping 0
                for _ in 1..number_of_credentials {
                    let new_cred = dev.send_cbor(&msg)?;
                    assertions.push(new_cred.into());
                }

                Ok(GetAssertionResult::CTAP2(
                    AssertionObject(assertions),
                    self.client_data_wrapper.clone(),
                ))
            } else {
                let data: Value = from_slice(&input[1..]).map_err(CommandError::Deserializing)?;
                Err(CommandError::StatusCode(status, Some(data)).into())
            }
        } else if status.is_ok() {
            Err(CommandError::InputTooSmall.into())
        } else {
            Err(CommandError::StatusCode(status, None).into())
        }
    }
}

#[derive(Debug, PartialEq, Eq)]
pub struct Assertion {
    pub credentials: Option<PublicKeyCredentialDescriptor>, /* Was optional in CTAP2.0, is
                                                             * mandatory in CTAP2.1 */
    pub auth_data: AuthenticatorData,
    pub signature: Vec<u8>,
    pub user: Option<User>,
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

// TODO(baloo): Move this to src/ctap2/mod.rs?
#[derive(Debug, PartialEq, Eq)]
pub struct AssertionObject(pub Vec<Assertion>);

impl AssertionObject {
    pub fn u2f_sign_data(&self) -> Vec<u8> {
        if let Some(first) = self.0.first() {
            let mut res = Vec::new();
            res.push(first.auth_data.flags.bits());
            res.extend(first.auth_data.counter.to_be_bytes());
            res.extend(&first.signature);
            res
            // first.signature.clone()
        } else {
            Vec::new()
        }
    }
}

pub(crate) struct GetAssertionResponse {
    credentials: Option<PublicKeyCredentialDescriptor>,
    auth_data: AuthenticatorData,
    signature: Vec<u8>,
    user: Option<User>,
    number_of_credentials: Option<usize>,
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
        HIDCmd, SW_CONDITIONS_NOT_SATISFIED, SW_NO_ERROR, U2F_CHECK_IS_REGISTERED,
        U2F_REQUEST_USER_PRESENCE,
    };
    use crate::ctap2::attestation::{AuthenticatorData, AuthenticatorDataFlags};
    use crate::ctap2::client_data::{
        Challenge, CollectedClientData, CollectedClientDataWrapper, TokenBinding, WebauthnType,
    };
    use crate::ctap2::commands::get_assertion::AssertionObject;
    use crate::ctap2::commands::RequestCtap1;
    use crate::ctap2::server::{
        PublicKeyCredentialDescriptor, RelyingParty, RelyingPartyWrapper, RpIdHash, Transport, User,
    };
    use crate::transport::device_selector::Device;
    use crate::transport::hid::HIDDevice;
    use crate::transport::FidoDevice;
    use crate::u2ftypes::U2FDevice;
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
            client_data.clone(),
            RelyingPartyWrapper::Data(RelyingParty {
                id: String::from("example.com"),
                name: Some(String::from("Acme")),
                icon: None,
            }),
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
            None,
            None,
        )
        .expect("Failed to create GetAssertion");
        let mut device = Device::new("commands/get_assertion").unwrap();
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
        msg.extend([HIDCmd::Cbor.into(), 0x1, 0x5c]); // cmd + bcnt
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
            user: Some(User {
                id: vec![
                    0x30, 0x82, 0x01, 0x93, 0x30, 0x82, 0x01, 0x38, 0xa0, 0x03, 0x02, 0x01, 0x02,
                    0x30, 0x82, 0x01, 0x93, 0x30, 0x82, 0x01, 0x38, 0xa0, 0x03, 0x02, 0x01, 0x02,
                    0x30, 0x82, 0x01, 0x93, 0x30, 0x82,
                ],
                icon: Some("https://pics.example.com/00/p/aBjjjpqPb.png".to_string()),
                name: Some("johnpsmith@example.com".to_string()),
                display_name: Some("John P. Smith".to_string()),
            }),
            auth_data: expected_auth_data,
        };

        let expected = GetAssertionResult::CTAP2(
            AssertionObject(vec![expected_assertion]),
            CollectedClientDataWrapper {
                client_data,
                serialized_data: CLIENT_DATA_VEC.to_vec(),
            },
        );
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
        let assertion = GetAssertion::new(
            client_data.clone(),
            RelyingPartyWrapper::Data(RelyingParty {
                id: String::from("example.com"),
                name: Some(String::from("Acme")),
                icon: None,
            }),
            vec![allowed_key.clone()],
            GetAssertionOptions {
                user_presence: Some(true),
                user_verification: None,
            },
            Default::default(),
            None,
            None,
        )
        .expect("Failed to create GetAssertion");
        let mut device = Device::new("commands/get_assertion").unwrap(); // not really used (all functions ignore it)
                                                                         // channel id
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
        let (ctap1_request, key_handle) = assertion.ctap1_format(&mut device).unwrap();
        // Check if the request is going to be correct
        assert_eq!(ctap1_request, GET_ASSERTION_SAMPLE_REQUEST_CTAP1);
        assert_eq!(key_handle, allowed_key);

        // Now do it again, but parse the actual response
        fill_device_ctap1(
            &mut device,
            cid,
            U2F_CHECK_IS_REGISTERED,
            SW_CONDITIONS_NOT_SATISFIED,
        );
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

        let expected = GetAssertionResult::CTAP2(
            AssertionObject(vec![expected_assertion]),
            CollectedClientDataWrapper {
                client_data,
                serialized_data: CLIENT_DATA_VEC.to_vec(),
            },
        );

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
            client_data.clone(),
            RelyingPartyWrapper::Data(RelyingParty {
                id: String::from("example.com"),
                name: Some(String::from("Acme")),
                icon: None,
            }),
            vec![too_long_key_handle.clone()],
            GetAssertionOptions {
                user_presence: Some(true),
                user_verification: None,
            },
            Default::default(),
            None,
            None,
        )
        .expect("Failed to create GetAssertion");

        let mut device = Device::new("commands/get_assertion").unwrap(); // not really used (all functions ignore it)
                                                                         // channel id
        let mut cid = [0u8; 4];
        thread_rng().fill_bytes(&mut cid);

        device.set_cid(cid);

        assert_matches!(
            assertion.ctap1_format(&mut device),
            Err(HIDError::Command(CommandError::StatusCode(
                StatusCode::NoCredentials,
                ..
            )))
        );

        // Test also multiple too long keys and an empty allow list
        for allow_list in [vec![], vec![too_long_key_handle.clone(); 5]] {
            assertion.allow_list = allow_list;

            assert_matches!(
                assertion.ctap1_format(&mut device),
                Err(HIDError::Command(CommandError::StatusCode(
                    StatusCode::NoCredentials,
                    ..
                )))
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
        let (ctap1_request, key_handle) = assertion.ctap1_format(&mut device).unwrap();
        // Check if the request is going to be correct
        assert_eq!(ctap1_request, GET_ASSERTION_SAMPLE_REQUEST_CTAP1);
        assert_eq!(key_handle, ok_key_handle);

        // Now do it again, but parse the actual response
        fill_device_ctap1(
            &mut device,
            cid,
            U2F_CHECK_IS_REGISTERED,
            SW_CONDITIONS_NOT_SATISFIED,
        );
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

        let expected = GetAssertionResult::CTAP2(
            AssertionObject(vec![expected_assertion]),
            CollectedClientDataWrapper {
                client_data,
                serialized_data: CLIENT_DATA_VEC.to_vec(),
            },
        );

        assert_eq!(response, expected);
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

    const GET_ASSERTION_SAMPLE_RESPONSE_CTAP2: [u8; 348] = [
        0x00, // status == success
        0xA5, // map(5)
        0x01, // unsigned(1)
        0xA2, // map(2)
        0x62, // text(2)
        0x69, 0x64, // "id"
        0x58, 0x40, // bytes(0x64, )
        0xF2, 0x20, 0x06, 0xDE, 0x4F, 0x90, 0x5A, 0xF6, 0x8A, 0x43, 0x94, 0x2F, 0x02, 0x4F, 0x2A,
        0x5E, 0xCE, 0x60, 0x3D, 0x9C, 0x6D, 0x4B, 0x3D, 0xF8, 0xBE, 0x08, 0xED, 0x01, 0xFC, 0x44,
        0x26, 0x46, 0xD0, 0x34, 0x85, 0x8A, 0xC7, 0x5B, 0xED, 0x3F, 0xD5, 0x80, 0xBF, 0x98, 0x08,
        0xD9, 0x4F, 0xCB, 0xEE, 0x82, 0xB9, 0xB2, 0xEF, 0x66, 0x77, 0xAF, 0x0A, 0xDC, 0xC3, 0x58,
        0x52, 0xEA, 0x6B,
        0x9E, // "\x0xF2,  \x0x06, \x0xDE, O\x0x90, Z\x0xF6, \x0x8A, C\x0x94, /\x0x02, O*^\x0xCE, `=\x0x9C, mK=\x0xF8, \x0xBE, \b\x0xED, \x0x01, \x0xFC, D&F\x0xD0, 4\x0x85, \x0x8A, \x0xC7, [\x0xED, ?\x0xD5, \x0x80, \x0xBF, \x0x98, \b\x0xD9, O\x0xCB, \x0xEE, \x0x82, \x0xB9, \x0xB2, \x0xEF, fw\x0xAF, \n\x0xDC, \x0xC3, 0xXR, \x0xEA, k\x0x9E, "
        0x64, // text(4)
        0x74, 0x79, 0x70, 0x65, // "type"
        0x6A, // text(0x10, )
        0x70, 0x75, 0x62, 0x6C, 0x69, 0x63, 0x2D, 0x6B, 0x65, 0x79, // "public-key"
        0x02, // unsigned(2)
        0x58, 0x25, // bytes(0x37, )
        0x62, 0x5D, 0xDA, 0xDF, 0x74, 0x3F, 0x57, 0x27, 0xE6, 0x6B, 0xBA, 0x8C, 0x2E, 0x38, 0x79,
        0x22, 0xD1, 0xAF, 0x43, 0xC5, 0x03, 0xD9, 0x11, 0x4A, 0x8F, 0xBA, 0x10, 0x4D, 0x84, 0xD0,
        0x2B, 0xFA, 0x01, 0x00, 0x00, 0x00,
        0x11, // "b]\x0xDA, \x0xDF, t?W'\x0xE6, k\x0xBA, \x0x8C, .8y\"\x0xD1, \x0xAF, C\x0xC5, \x0x03, \x0xD9, \x0x11, J\x0x8F, \x0xBA, \x0x10, M\x0x84, \x0xD0, +\x0xFA, \x0x01, \x0x00, \x0x00, \x0x00, \x0x11, "
        0x03, // unsigned(3)
        0x58, 0x47, // bytes(0x71, )
        0x30, 0x45, 0x02, 0x20, 0x4A, 0x5A, 0x9D, 0xD3, 0x92, 0x98, 0x14, 0x9D, 0x90, 0x47, 0x69,
        0xB5, 0x1A, 0x45, 0x14, 0x33, 0x00, 0x6F, 0x18, 0x2A, 0x34, 0xFB, 0xDF, 0x66, 0xDE, 0x5F,
        0xC7, 0x17, 0xD7, 0x5F, 0xB3, 0x50, 0x02, 0x21, 0x00, 0xA4, 0x6B, 0x8E, 0xA3, 0xC3, 0xB9,
        0x33, 0x82, 0x1C, 0x6E, 0x7F, 0x5E, 0xF9, 0xDA, 0xAE, 0x94, 0xAB, 0x47, 0xF1, 0x8D, 0xB4,
        0x74, 0xC7, 0x47, 0x90, 0xEA, 0xAB, 0xB1, 0x44, 0x11, 0xE7,
        0xA0, // "0x0E, \x0x02,  0xJZ, \x0x9D, \x0xD3, \x0x92, \x0x98, \x0x14, \x0x9D, \x0x90, Gi\x0xB5, \x0x1A, E\x0x14, 3\x0x00, o\x0x18, *4\x0xFB, \x0xDF, f\x0xDE, _\x0xC7, \x0x17, \x0xD7, _\x0xB3, P\x0x02, !\x0x00, \x0xA4, k\x0x8E, \x0xA3, \x0xC3, \x0xB9, 3\x0x82, \x0x1C, n\x0x7F, ^\x0xF9, \x0xDA, \x0xAE, \x0x94, \x0xAB, G\x0xF1, \x0x8D, \x0xB4, t\x0xC7, G\x0x90, \x0xEA, \x0xAB, \x0xB1, D\x0x11, \x0xE7, \x0xA0, "
        0x04, // unsigned(4)
        0xA4, // map(4)
        0x62, // text(2)
        0x69, 0x64, // "id"
        0x58, 0x20, // bytes(0x32, )
        0x30, 0x82, 0x01, 0x93, 0x30, 0x82, 0x01, 0x38, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x30, 0x82,
        0x01, 0x93, 0x30, 0x82, 0x01, 0x38, 0xA0, 0x03, 0x02, 0x01, 0x02, 0x30, 0x82, 0x01, 0x93,
        0x30,
        0x82, // "0\x0x82, \x0x01, \x0x93, 0\x0x82, \x0x01, 8\x0xA0, \x0x03, \x0x02, \x0x01, \x0x02, 0\x0x82, \x0x01, \x0x93, 0\x0x82, \x0x01, 8\x0xA0, \x0x03, \x0x02, \x0x01, \x0x02, 0\x0x82, \x0x01, \x0x93, 0\x0x82, "
        0x64, // text(4)
        0x69, 0x63, 0x6F, 0x6E, // "icon"
        0x78, 0x2B, // text(0x43, )
        0x68, 0x74, 0x74, 0x70, 0x73, 0x3A, 0x2F, 0x2F, 0x70, 0x69, 0x63, 0x73, 0x2E, 0x65, 0x78,
        0x61, 0x6D, 0x70, 0x6C, 0x65, 0x2E, 0x63, 0x6F, 0x6D, 0x2F, 0x30, 0x30, 0x2F, 0x70, 0x2F,
        0x61, 0x42, 0x6A, 0x6A, 0x6A, 0x70, 0x71, 0x50, 0x62, 0x2E, 0x70, 0x6E,
        0x67, // "https://pics.example.com/0x00, /p/aBjjjpqPb.png"
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
