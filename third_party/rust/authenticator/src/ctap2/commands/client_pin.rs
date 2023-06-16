#![allow(non_upper_case_globals)]
// Note: Needed for PinUvAuthTokenPermission
//       The current version of `bitflags` doesn't seem to allow
//       to set this for an individual bitflag-struct.
use super::{get_info::AuthenticatorInfo, Command, CommandError, RequestCtap2, StatusCode};
use crate::crypto::{COSEKey, CryptoError, PinUvAuthProtocol, PinUvAuthToken, SharedSecret};
use crate::transport::errors::HIDError;
use crate::u2ftypes::U2FDevice;
use serde::{
    de::{Error as SerdeError, IgnoredAny, MapAccess, Visitor},
    ser::SerializeMap,
    Deserialize, Deserializer, Serialize, Serializer,
};
use serde_bytes::ByteBuf;
use serde_cbor::de::from_slice;
use serde_cbor::ser::to_vec;
use serde_cbor::Value;
use sha2::{Digest, Sha256};
use std::convert::TryFrom;
use std::error::Error as StdErrorT;
use std::fmt;

#[derive(Debug, Copy, Clone)]
#[repr(u8)]
pub enum PINSubcommand {
    GetPinRetries = 0x01,
    GetKeyAgreement = 0x02,
    SetPIN = 0x03,
    ChangePIN = 0x04,
    GetPINToken = 0x05, // superseded by GetPinUvAuth*
    GetPinUvAuthTokenUsingUvWithPermissions = 0x06,
    GetUvRetries = 0x07,
    GetPinUvAuthTokenUsingPinWithPermissions = 0x09, // Yes, 0x08 is missing
}

bitflags! {
    pub struct PinUvAuthTokenPermission: u8 {
        const MakeCredential = 0x01;             // rp_id required
        const GetAssertion = 0x02;               // rp_id required
        const CredentialManagement = 0x04;       // rp_id optional
        const BioEnrollment = 0x08;              // rp_id ignored
        const LargeBlobWrite = 0x10;             // rp_id ignored
        const AuthenticatorConfiguration = 0x20; // rp_id ignored
    }
}

impl Default for PinUvAuthTokenPermission {
    fn default() -> Self {
        // CTAP 2.1 spec:
        // If authenticatorClientPIN's getPinToken subcommand is invoked, default permissions
        // of `mc` and `ga` (value 0x03) are granted for the returned pinUvAuthToken.
        PinUvAuthTokenPermission::MakeCredential | PinUvAuthTokenPermission::GetAssertion
    }
}

#[derive(Debug)]
pub struct ClientPIN {
    pin_protocol: Option<PinUvAuthProtocol>,
    subcommand: PINSubcommand,
    key_agreement: Option<COSEKey>,
    pin_auth: Option<ByteBuf>,
    new_pin_enc: Option<ByteBuf>,
    pin_hash_enc: Option<ByteBuf>,
    permissions: Option<u8>,
    rp_id: Option<String>,
}

impl Default for ClientPIN {
    fn default() -> Self {
        ClientPIN {
            pin_protocol: None,
            subcommand: PINSubcommand::GetPinRetries,
            key_agreement: None,
            pin_auth: None,
            new_pin_enc: None,
            pin_hash_enc: None,
            permissions: None,
            rp_id: None,
        }
    }
}

impl Serialize for ClientPIN {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        // Need to define how many elements are going to be in the map
        // beforehand
        let mut map_len = 1;
        if self.pin_protocol.is_some() {
            map_len += 1;
        }
        if self.key_agreement.is_some() {
            map_len += 1;
        }
        if self.pin_auth.is_some() {
            map_len += 1;
        }
        if self.new_pin_enc.is_some() {
            map_len += 1;
        }
        if self.pin_hash_enc.is_some() {
            map_len += 1;
        }
        if self.permissions.is_some() {
            map_len += 1;
        }
        if self.rp_id.is_some() {
            map_len += 1;
        }

        let mut map = serializer.serialize_map(Some(map_len))?;
        if let Some(ref pin_protocol) = self.pin_protocol {
            map.serialize_entry(&1, &pin_protocol.id())?;
        }
        let command: u8 = self.subcommand as u8;
        map.serialize_entry(&2, &command)?;
        if let Some(ref key_agreement) = self.key_agreement {
            map.serialize_entry(&3, key_agreement)?;
        }
        if let Some(ref pin_auth) = self.pin_auth {
            map.serialize_entry(&4, pin_auth)?;
        }
        if let Some(ref new_pin_enc) = self.new_pin_enc {
            map.serialize_entry(&5, new_pin_enc)?;
        }
        if let Some(ref pin_hash_enc) = self.pin_hash_enc {
            map.serialize_entry(&6, pin_hash_enc)?;
        }
        if let Some(ref permissions) = self.permissions {
            map.serialize_entry(&9, permissions)?;
        }
        if let Some(ref rp_id) = self.rp_id {
            map.serialize_entry(&0x0A, rp_id)?;
        }

        map.end()
    }
}

pub trait ClientPINSubCommand {
    type Output;
    fn as_client_pin(&self) -> Result<ClientPIN, CommandError>;
    fn parse_response_payload(&self, input: &[u8]) -> Result<Self::Output, CommandError>;
}

struct ClientPinResponse {
    key_agreement: Option<COSEKey>,
    pin_token: Option<EncryptedPinToken>,
    /// Number of PIN attempts remaining before lockout.
    pin_retries: Option<u8>,
    power_cycle_state: Option<bool>,
    uv_retries: Option<u8>,
}

impl<'de> Deserialize<'de> for ClientPinResponse {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct ClientPinResponseVisitor;

        impl<'de> Visitor<'de> for ClientPinResponseVisitor {
            type Value = ClientPinResponse;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a map")
            }

            fn visit_map<M>(self, mut map: M) -> Result<Self::Value, M::Error>
            where
                M: MapAccess<'de>,
            {
                let mut key_agreement = None;
                let mut pin_token = None;
                let mut pin_retries = None;
                let mut power_cycle_state = None;
                let mut uv_retries = None;
                while let Some(key) = map.next_key()? {
                    match key {
                        0x01 => {
                            if key_agreement.is_some() {
                                return Err(SerdeError::duplicate_field("key_agreement"));
                            }
                            key_agreement = map.next_value()?;
                        }
                        0x02 => {
                            if pin_token.is_some() {
                                return Err(SerdeError::duplicate_field("pin_token"));
                            }
                            pin_token = map.next_value()?;
                        }
                        0x03 => {
                            if pin_retries.is_some() {
                                return Err(SerdeError::duplicate_field("pin_retries"));
                            }
                            pin_retries = Some(map.next_value()?);
                        }
                        0x04 => {
                            if power_cycle_state.is_some() {
                                return Err(SerdeError::duplicate_field("power_cycle_state"));
                            }
                            power_cycle_state = Some(map.next_value()?);
                        }
                        0x05 => {
                            if uv_retries.is_some() {
                                return Err(SerdeError::duplicate_field("uv_retries"));
                            }
                            uv_retries = Some(map.next_value()?);
                        }
                        k => {
                            warn!("ClientPinResponse: unexpected key: {:?}", k);
                            let _ = map.next_value::<IgnoredAny>()?;
                            continue;
                        }
                    }
                }
                Ok(ClientPinResponse {
                    key_agreement,
                    pin_token,
                    pin_retries,
                    power_cycle_state,
                    uv_retries,
                })
            }
        }
        deserializer.deserialize_bytes(ClientPinResponseVisitor)
    }
}

#[derive(Debug)]
pub struct GetKeyAgreement {
    pin_protocol: PinUvAuthProtocol,
}

impl GetKeyAgreement {
    pub fn new(pin_protocol: PinUvAuthProtocol) -> Self {
        GetKeyAgreement { pin_protocol }
    }
}

impl ClientPINSubCommand for GetKeyAgreement {
    type Output = KeyAgreement;

    fn as_client_pin(&self) -> Result<ClientPIN, CommandError> {
        Ok(ClientPIN {
            pin_protocol: Some(self.pin_protocol.clone()),
            subcommand: PINSubcommand::GetKeyAgreement,
            ..ClientPIN::default()
        })
    }

    fn parse_response_payload(&self, input: &[u8]) -> Result<Self::Output, CommandError> {
        let value: Value = from_slice(input).map_err(CommandError::Deserializing)?;
        debug!("GetKeyAgreement::parse_response_payload {:?}", value);

        let get_pin_response: ClientPinResponse =
            from_slice(input).map_err(CommandError::Deserializing)?;
        if let Some(key_agreement) = get_pin_response.key_agreement {
            Ok(KeyAgreement {
                pin_protocol: self.pin_protocol.clone(),
                peer_key: key_agreement,
            })
        } else {
            Err(CommandError::MissingRequiredField("key_agreement"))
        }
    }
}

#[derive(Debug)]
/// Superseded by GetPinUvAuthTokenUsingUvWithPermissions or
/// GetPinUvAuthTokenUsingPinWithPermissions, thus for backwards compatibility only
pub struct GetPinToken<'sc, 'pin> {
    shared_secret: &'sc SharedSecret,
    pin: &'pin Pin,
}

impl<'sc, 'pin> GetPinToken<'sc, 'pin> {
    pub fn new(shared_secret: &'sc SharedSecret, pin: &'pin Pin) -> Self {
        GetPinToken { shared_secret, pin }
    }
}

impl<'sc, 'pin> ClientPINSubCommand for GetPinToken<'sc, 'pin> {
    type Output = PinUvAuthToken;

    fn as_client_pin(&self) -> Result<ClientPIN, CommandError> {
        let input = self.pin.for_pin_token();
        trace!("pin_hash = {:#04X?}", &input);
        let pin_hash_enc = self.shared_secret.encrypt(&input)?;
        trace!("pin_hash_enc = {:#04X?}", &pin_hash_enc);

        Ok(ClientPIN {
            pin_protocol: Some(self.shared_secret.pin_protocol.clone()),
            subcommand: PINSubcommand::GetPINToken,
            key_agreement: Some(self.shared_secret.client_input().clone()),
            pin_hash_enc: Some(ByteBuf::from(pin_hash_enc)),
            ..ClientPIN::default()
        })
    }

    fn parse_response_payload(&self, input: &[u8]) -> Result<Self::Output, CommandError> {
        let value: Value = from_slice(input).map_err(CommandError::Deserializing)?;
        debug!("GetKeyAgreement::parse_response_payload {:?}", value);

        let get_pin_response: ClientPinResponse =
            from_slice(input).map_err(CommandError::Deserializing)?;
        match get_pin_response.pin_token {
            Some(encrypted_pin_token) => {
                // CTAP 2.1 spec:
                // If authenticatorClientPIN's getPinToken subcommand is invoked, default permissions
                // of `mc` and `ga` (value 0x03) are granted for the returned pinUvAuthToken.
                let default_permissions = PinUvAuthTokenPermission::default();
                let pin_token = self
                    .shared_secret
                    .decrypt_pin_token(default_permissions, encrypted_pin_token.as_ref())?;
                Ok(pin_token)
            }
            None => Err(CommandError::MissingRequiredField("key_agreement")),
        }
    }
}

#[derive(Debug)]
pub struct GetPinUvAuthTokenUsingPinWithPermissions<'sc, 'pin> {
    shared_secret: &'sc SharedSecret,
    pin: &'pin Pin,
    permissions: PinUvAuthTokenPermission,
    rp_id: Option<String>,
}

impl<'sc, 'pin> GetPinUvAuthTokenUsingPinWithPermissions<'sc, 'pin> {
    pub fn new(
        shared_secret: &'sc SharedSecret,
        pin: &'pin Pin,
        permissions: PinUvAuthTokenPermission,
        rp_id: Option<String>,
    ) -> Self {
        GetPinUvAuthTokenUsingPinWithPermissions {
            shared_secret,
            pin,
            permissions,
            rp_id,
        }
    }
}

impl<'sc, 'pin> ClientPINSubCommand for GetPinUvAuthTokenUsingPinWithPermissions<'sc, 'pin> {
    type Output = PinUvAuthToken;

    fn as_client_pin(&self) -> Result<ClientPIN, CommandError> {
        let input = self.pin.for_pin_token();
        let pin_hash_enc = self.shared_secret.encrypt(&input)?;

        Ok(ClientPIN {
            pin_protocol: Some(self.shared_secret.pin_protocol.clone()),
            subcommand: PINSubcommand::GetPinUvAuthTokenUsingPinWithPermissions,
            key_agreement: Some(self.shared_secret.client_input().clone()),
            pin_hash_enc: Some(ByteBuf::from(pin_hash_enc)),
            permissions: Some(self.permissions.bits()),
            rp_id: self.rp_id.clone(), /* TODO: This could probably be done less wasteful with
                                        * &str all the way */
            ..ClientPIN::default()
        })
    }

    fn parse_response_payload(&self, input: &[u8]) -> Result<Self::Output, CommandError> {
        let value: Value = from_slice(input).map_err(CommandError::Deserializing)?;
        debug!(
            "GetPinUvAuthTokenUsingPinWithPermissions::parse_response_payload {:?}",
            value
        );

        let get_pin_response: ClientPinResponse =
            from_slice(input).map_err(CommandError::Deserializing)?;
        match get_pin_response.pin_token {
            Some(encrypted_pin_token) => {
                let pin_token = self
                    .shared_secret
                    .decrypt_pin_token(self.permissions, encrypted_pin_token.as_ref())?;
                Ok(pin_token)
            }
            None => Err(CommandError::MissingRequiredField("key_agreement")),
        }
    }
}

macro_rules! implementRetries {
    ($name:ident, $getter:ident) => {
        #[derive(Debug)]
        pub struct $name {}

        impl $name {
            pub fn new() -> Self {
                Self {}
            }
        }

        impl ClientPINSubCommand for $name {
            type Output = u8;

            fn as_client_pin(&self) -> Result<ClientPIN, CommandError> {
                Ok(ClientPIN {
                    subcommand: PINSubcommand::$name,
                    ..ClientPIN::default()
                })
            }

            fn parse_response_payload(&self, input: &[u8]) -> Result<Self::Output, CommandError> {
                let value: Value = from_slice(input).map_err(CommandError::Deserializing)?;
                debug!("{}::parse_response_payload {:?}", stringify!($name), value);

                let get_pin_response: ClientPinResponse =
                    from_slice(input).map_err(CommandError::Deserializing)?;
                match get_pin_response.$getter {
                    Some($getter) => Ok($getter),
                    None => Err(CommandError::MissingRequiredField(stringify!($getter))),
                }
            }
        }
    };
}

implementRetries!(GetPinRetries, pin_retries);
implementRetries!(GetUvRetries, uv_retries);

#[derive(Debug)]
pub struct GetPinUvAuthTokenUsingUvWithPermissions<'sc> {
    shared_secret: &'sc SharedSecret,
    permissions: PinUvAuthTokenPermission,
    rp_id: Option<String>,
}

impl<'sc> GetPinUvAuthTokenUsingUvWithPermissions<'sc> {
    pub fn new(
        shared_secret: &'sc SharedSecret,
        permissions: PinUvAuthTokenPermission,
        rp_id: Option<String>,
    ) -> Self {
        GetPinUvAuthTokenUsingUvWithPermissions {
            shared_secret,
            permissions,
            rp_id,
        }
    }
}

impl<'sc> ClientPINSubCommand for GetPinUvAuthTokenUsingUvWithPermissions<'sc> {
    type Output = PinUvAuthToken;

    fn as_client_pin(&self) -> Result<ClientPIN, CommandError> {
        Ok(ClientPIN {
            pin_protocol: Some(self.shared_secret.pin_protocol.clone()),
            subcommand: PINSubcommand::GetPinUvAuthTokenUsingUvWithPermissions,
            key_agreement: Some(self.shared_secret.client_input().clone()),
            permissions: Some(self.permissions.bits()),
            rp_id: self.rp_id.clone(), /* TODO: This could probably be done less wasteful with
                                        * &str all the way */
            ..ClientPIN::default()
        })
    }

    fn parse_response_payload(&self, input: &[u8]) -> Result<Self::Output, CommandError> {
        let value: Value = from_slice(input).map_err(CommandError::Deserializing)?;
        debug!("GetKeyAgreement::parse_response_payload {:?}", value);

        let get_pin_response: ClientPinResponse =
            from_slice(input).map_err(CommandError::Deserializing)?;
        match get_pin_response.pin_token {
            Some(encrypted_pin_token) => {
                let pin_token = self
                    .shared_secret
                    .decrypt_pin_token(self.permissions, encrypted_pin_token.as_ref())?;
                Ok(pin_token)
            }
            None => Err(CommandError::MissingRequiredField("key_agreement")),
        }
    }
}

#[derive(Debug)]
pub struct SetNewPin<'sc, 'pin> {
    shared_secret: &'sc SharedSecret,
    new_pin: &'pin Pin,
}

impl<'sc, 'pin> SetNewPin<'sc, 'pin> {
    pub fn new(shared_secret: &'sc SharedSecret, new_pin: &'pin Pin) -> Self {
        SetNewPin {
            shared_secret,
            new_pin,
        }
    }
}

impl<'sc, 'pin> ClientPINSubCommand for SetNewPin<'sc, 'pin> {
    type Output = ();

    fn as_client_pin(&self) -> Result<ClientPIN, CommandError> {
        if self.new_pin.as_bytes().len() > 63 {
            return Err(CommandError::StatusCode(
                StatusCode::PinPolicyViolation,
                None,
            ));
        }

        // newPinEnc: the result of calling encrypt(shared secret, paddedPin) where paddedPin is
        // newPin padded on the right with 0x00 bytes to make it 64 bytes long. (Since the maximum
        // length of newPin is 63 bytes, there is always at least one byte of padding.)
        let new_pin_padded = self.new_pin.padded();
        let new_pin_enc = self.shared_secret.encrypt(&new_pin_padded)?;

        // pinUvAuthParam: the result of calling authenticate(shared secret, newPinEnc).
        let pin_auth = self.shared_secret.authenticate(&new_pin_enc)?;

        Ok(ClientPIN {
            pin_protocol: Some(self.shared_secret.pin_protocol.clone()),
            subcommand: PINSubcommand::SetPIN,
            key_agreement: Some(self.shared_secret.client_input().clone()),
            new_pin_enc: Some(ByteBuf::from(new_pin_enc)),
            pin_auth: Some(ByteBuf::from(pin_auth)),
            ..ClientPIN::default()
        })
    }

    fn parse_response_payload(&self, input: &[u8]) -> Result<Self::Output, CommandError> {
        // Should be an empty response or a valid cbor-value (which we ignore)
        if input.is_empty() {
            Ok(())
        } else {
            let _: Value = from_slice(input).map_err(CommandError::Deserializing)?;
            Ok(())
        }
    }
}

#[derive(Debug)]
pub struct ChangeExistingPin<'sc, 'pin> {
    pin_protocol: PinUvAuthProtocol,
    shared_secret: &'sc SharedSecret,
    current_pin: &'pin Pin,
    new_pin: &'pin Pin,
}

impl<'sc, 'pin> ChangeExistingPin<'sc, 'pin> {
    pub fn new(
        info: &AuthenticatorInfo,
        shared_secret: &'sc SharedSecret,
        current_pin: &'pin Pin,
        new_pin: &'pin Pin,
    ) -> Result<Self, CommandError> {
        Ok(ChangeExistingPin {
            pin_protocol: PinUvAuthProtocol::try_from(info)?,
            shared_secret,
            current_pin,
            new_pin,
        })
    }
}

impl<'sc, 'pin> ClientPINSubCommand for ChangeExistingPin<'sc, 'pin> {
    type Output = ();

    fn as_client_pin(&self) -> Result<ClientPIN, CommandError> {
        if self.new_pin.as_bytes().len() > 63 {
            return Err(CommandError::StatusCode(
                StatusCode::PinPolicyViolation,
                None,
            ));
        }

        // newPinEnc: the result of calling encrypt(shared secret, paddedPin) where paddedPin is
        // newPin padded on the right with 0x00 bytes to make it 64 bytes long. (Since the maximum
        // length of newPin is 63 bytes, there is always at least one byte of padding.)
        let new_pin_padded = self.new_pin.padded();
        let new_pin_enc = self.shared_secret.encrypt(&new_pin_padded)?;

        let current_pin_hash = self.current_pin.for_pin_token();
        let pin_hash_enc = self.shared_secret.encrypt(current_pin_hash.as_ref())?;

        let pin_auth = self
            .shared_secret
            .authenticate(&[new_pin_enc.as_slice(), pin_hash_enc.as_slice()].concat())?;

        Ok(ClientPIN {
            pin_protocol: Some(self.shared_secret.pin_protocol.clone()),
            subcommand: PINSubcommand::ChangePIN,
            key_agreement: Some(self.shared_secret.client_input().clone()),
            new_pin_enc: Some(ByteBuf::from(new_pin_enc)),
            pin_hash_enc: Some(ByteBuf::from(pin_hash_enc)),
            pin_auth: Some(ByteBuf::from(pin_auth)),
            permissions: None,
            rp_id: None,
        })
    }

    fn parse_response_payload(&self, input: &[u8]) -> Result<Self::Output, CommandError> {
        // Should be an empty response or a valid cbor-value (which we ignore)
        if input.is_empty() {
            Ok(())
        } else {
            let _: Value = from_slice(input).map_err(CommandError::Deserializing)?;
            Ok(())
        }
    }
}

impl<T> RequestCtap2 for T
where
    T: ClientPINSubCommand,
    T: fmt::Debug,
{
    type Output = <T as ClientPINSubCommand>::Output;

    fn command() -> Command {
        Command::ClientPin
    }

    fn wire_format(&self) -> Result<Vec<u8>, HIDError> {
        let client_pin = self.as_client_pin()?;
        let output = to_vec(&client_pin).map_err(CommandError::Serializing)?;
        trace!("client subcommmand: {:04X?}", &output);

        Ok(output)
    }

    fn handle_response_ctap2<Dev>(
        &self,
        _dev: &mut Dev,
        input: &[u8],
    ) -> Result<Self::Output, HIDError>
    where
        Dev: U2FDevice,
    {
        trace!("Client pin subcomand response:{:04X?}", &input);
        if input.is_empty() {
            return Err(CommandError::InputTooSmall.into());
        }

        let status: StatusCode = input[0].into();
        debug!("response status code: {:?}", status);
        if status.is_ok() {
            <T as ClientPINSubCommand>::parse_response_payload(self, &input[1..])
                .map_err(HIDError::Command)
        } else {
            let add_data = if input.len() > 1 {
                let data: Value = from_slice(&input[1..]).map_err(CommandError::Deserializing)?;
                Some(data)
            } else {
                None
            };
            Err(CommandError::StatusCode(status, add_data).into())
        }
    }
}

#[derive(Debug)]
pub struct KeyAgreement {
    pin_protocol: PinUvAuthProtocol,
    peer_key: COSEKey,
}

impl KeyAgreement {
    pub fn shared_secret(&self) -> Result<SharedSecret, CommandError> {
        Ok(self.pin_protocol.encapsulate(&self.peer_key)?)
    }
}

#[derive(Debug, Deserialize)]
pub struct EncryptedPinToken(ByteBuf);

impl AsRef<[u8]> for EncryptedPinToken {
    fn as_ref(&self) -> &[u8] {
        self.0.as_ref()
    }
}

#[derive(Clone, Serialize, Deserialize)]
pub struct Pin(String);

impl fmt::Debug for Pin {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(f, "Pin(redacted)")
    }
}

impl Pin {
    pub fn new(value: &str) -> Pin {
        Pin(String::from(value))
    }

    pub fn for_pin_token(&self) -> Vec<u8> {
        let mut hasher = Sha256::new();
        hasher.update(self.0.as_bytes());

        let mut output = [0u8; 16];
        let len = output.len();
        output.copy_from_slice(&hasher.finalize().as_slice()[..len]);

        output.to_vec()
    }

    pub fn padded(&self) -> Vec<u8> {
        let mut out = self.0.as_bytes().to_vec();
        out.resize(64, 0x00);
        out
    }

    pub fn as_bytes(&self) -> &[u8] {
        self.0.as_bytes()
    }
}

#[derive(Clone, Debug, Serialize)]
pub enum PinError {
    PinRequired,
    PinIsTooShort,
    PinIsTooLong(usize),
    InvalidPin(Option<u8>),
    InvalidUv(Option<u8>),
    PinAuthBlocked,
    PinBlocked,
    PinNotSet,
    UvBlocked,
    /// Used for CTAP2.0 UV (fingerprints)
    PinAuthInvalid,
    Crypto(CryptoError),
}

impl fmt::Display for PinError {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        match *self {
            PinError::PinRequired => write!(f, "Pin required."),
            PinError::PinIsTooShort => write!(f, "pin is too short"),
            PinError::PinIsTooLong(len) => write!(f, "pin is too long ({len})"),
            PinError::InvalidPin(ref e) => {
                let mut res = write!(f, "Invalid Pin.");
                if let Some(pin_retries) = e {
                    res = write!(f, " Retries left: {pin_retries:?}")
                }
                res
            }
            PinError::InvalidUv(ref e) => {
                let mut res = write!(f, "Invalid Uv.");
                if let Some(uv_retries) = e {
                    res = write!(f, " Retries left: {uv_retries:?}")
                }
                res
            }
            PinError::PinAuthBlocked => {
                write!(f, "Pin authentication blocked. Device needs power cycle.")
            }
            PinError::PinBlocked => write!(f, "No retries left. Pin blocked. Device needs reset."),
            PinError::PinNotSet => write!(f, "Pin needed but not set on device."),
            PinError::UvBlocked => write!(f, "No retries left. Uv blocked. Device needs reset."),
            PinError::PinAuthInvalid => write!(f, "PinAuth invalid."),
            PinError::Crypto(ref e) => write!(f, "Crypto backend error: {e:?}"),
        }
    }
}

impl StdErrorT for PinError {}

impl From<CryptoError> for PinError {
    fn from(e: CryptoError) -> Self {
        PinError::Crypto(e)
    }
}
