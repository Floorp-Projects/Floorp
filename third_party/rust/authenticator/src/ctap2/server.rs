use crate::crypto::COSEAlgorithm;
use crate::{errors::AuthenticatorError, AuthenticatorTransports, KeyHandle};
use base64::Engine;
use serde::de::MapAccess;
use serde::{
    de::{Error as SerdeError, Unexpected, Visitor},
    ser::SerializeMap,
    Deserialize, Deserializer, Serialize, Serializer,
};
use serde_bytes::{ByteBuf, Bytes};
use sha2::{Digest, Sha256};
use std::convert::{Into, TryFrom};
use std::fmt;

#[derive(Serialize, Deserialize, PartialEq, Eq, Clone)]
pub struct RpIdHash(pub [u8; 32]);

impl fmt::Debug for RpIdHash {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        let value = base64::engine::general_purpose::URL_SAFE_NO_PAD.encode(self.0);
        write!(f, "RpIdHash({value})")
    }
}

impl AsRef<[u8]> for RpIdHash {
    fn as_ref(&self) -> &[u8] {
        self.0.as_ref()
    }
}

impl RpIdHash {
    pub fn from(src: &[u8]) -> Result<RpIdHash, AuthenticatorError> {
        let mut payload = [0u8; 32];
        if src.len() != payload.len() {
            Err(AuthenticatorError::InvalidRelyingPartyInput)
        } else {
            payload.copy_from_slice(src);
            Ok(RpIdHash(payload))
        }
    }
}

// NOTE: WebAuthn requires all fields and CTAP2 does not.
#[derive(Debug, Serialize, Clone, Default, Deserialize, PartialEq, Eq)]
pub struct RelyingParty {
    pub id: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    pub name: Option<String>,
}

impl RelyingParty {
    pub fn from<S>(id: S) -> Self
    where
        S: Into<String>,
    {
        Self {
            id: id.into(),
            name: None,
        }
    }

    pub fn hash(&self) -> RpIdHash {
        let mut hasher = Sha256::new();
        hasher.update(&self.id);

        let mut output = [0u8; 32];
        output.copy_from_slice(hasher.finalize().as_slice());

        RpIdHash(output)
    }
}

// NOTE: WebAuthn requires all fields and CTAP2 does not.
#[derive(Debug, Serialize, Clone, Eq, PartialEq, Deserialize, Default)]
pub struct PublicKeyCredentialUserEntity {
    #[serde(with = "serde_bytes")]
    pub id: Vec<u8>,
    pub name: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none", rename = "displayName")]
    pub display_name: Option<String>,
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PublicKeyCredentialParameters {
    pub alg: COSEAlgorithm,
}

impl TryFrom<i32> for PublicKeyCredentialParameters {
    type Error = AuthenticatorError;
    fn try_from(arg: i32) -> Result<Self, Self::Error> {
        let alg = COSEAlgorithm::try_from(arg as i64)?;
        Ok(PublicKeyCredentialParameters { alg })
    }
}

impl Serialize for PublicKeyCredentialParameters {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut map = serializer.serialize_map(Some(2))?;
        map.serialize_entry("alg", &self.alg)?;
        map.serialize_entry("type", "public-key")?;
        map.end()
    }
}

impl<'de> Deserialize<'de> for PublicKeyCredentialParameters {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct PublicKeyCredentialParametersVisitor;

        impl<'de> Visitor<'de> for PublicKeyCredentialParametersVisitor {
            type Value = PublicKeyCredentialParameters;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a map")
            }

            fn visit_map<M>(self, mut map: M) -> Result<Self::Value, M::Error>
            where
                M: MapAccess<'de>,
            {
                let mut found_type = false;
                let mut alg = None;
                while let Some(key) = map.next_key()? {
                    match key {
                        "alg" => {
                            if alg.is_some() {
                                return Err(SerdeError::duplicate_field("alg"));
                            }
                            alg = Some(map.next_value()?);
                        }
                        "type" => {
                            if found_type {
                                return Err(SerdeError::duplicate_field("type"));
                            }

                            let v: &str = map.next_value()?;
                            if v != "public-key" {
                                return Err(SerdeError::custom(format!("invalid value: {v}")));
                            }
                            found_type = true;
                        }
                        v => {
                            return Err(SerdeError::unknown_field(v, &[]));
                        }
                    }
                }

                if !found_type {
                    return Err(SerdeError::missing_field("type"));
                }

                let alg = alg.ok_or_else(|| SerdeError::missing_field("alg"))?;

                Ok(PublicKeyCredentialParameters { alg })
            }
        }

        deserializer.deserialize_bytes(PublicKeyCredentialParametersVisitor)
    }
}

#[derive(Debug, PartialEq, Serialize, Deserialize, Eq, Clone)]
#[serde(rename_all = "lowercase")]
pub enum Transport {
    USB,
    NFC,
    BLE,
    Internal,
}

impl From<AuthenticatorTransports> for Vec<Transport> {
    fn from(t: AuthenticatorTransports) -> Self {
        let mut transports = Vec::new();
        if t.contains(AuthenticatorTransports::USB) {
            transports.push(Transport::USB);
        }
        if t.contains(AuthenticatorTransports::NFC) {
            transports.push(Transport::NFC);
        }
        if t.contains(AuthenticatorTransports::BLE) {
            transports.push(Transport::BLE);
        }

        transports
    }
}

pub type PublicKeyCredentialId = Vec<u8>;

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct PublicKeyCredentialDescriptor {
    pub id: PublicKeyCredentialId,
    pub transports: Vec<Transport>,
}

impl Serialize for PublicKeyCredentialDescriptor {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        // TODO(MS): Transports is OPTIONAL, but some older tokens don't understand it
        //           and return a CBOR-Parsing error. It is only a hint for the token,
        //           so we'll leave it out for the moment
        let mut map = serializer.serialize_map(Some(2))?;
        // let mut map = serializer.serialize_map(Some(3))?;
        map.serialize_entry("id", Bytes::new(&self.id))?;
        map.serialize_entry("type", "public-key")?;
        // map.serialize_entry("transports", &self.transports)?;
        map.end()
    }
}

impl<'de> Deserialize<'de> for PublicKeyCredentialDescriptor {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct PublicKeyCredentialDescriptorVisitor;

        impl<'de> Visitor<'de> for PublicKeyCredentialDescriptorVisitor {
            type Value = PublicKeyCredentialDescriptor;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a map")
            }

            fn visit_map<M>(self, mut map: M) -> Result<Self::Value, M::Error>
            where
                M: MapAccess<'de>,
            {
                let mut found_type = false;
                let mut id = None;
                let mut transports = None;
                while let Some(key) = map.next_key()? {
                    match key {
                        "id" => {
                            if id.is_some() {
                                return Err(SerdeError::duplicate_field("id"));
                            }
                            let id_bytes: ByteBuf = map.next_value()?;
                            id = Some(id_bytes.into_vec());
                        }
                        "transports" => {
                            if transports.is_some() {
                                return Err(SerdeError::duplicate_field("transports"));
                            }
                            transports = Some(map.next_value()?);
                        }
                        "type" => {
                            if found_type {
                                return Err(SerdeError::duplicate_field("type"));
                            }
                            let v: &str = map.next_value()?;
                            if v != "public-key" {
                                return Err(SerdeError::custom(format!("invalid value: {v}")));
                            }
                            found_type = true;
                        }
                        v => {
                            return Err(SerdeError::unknown_field(v, &[]));
                        }
                    }
                }

                if !found_type {
                    return Err(SerdeError::missing_field("type"));
                }

                let id = id.ok_or_else(|| SerdeError::missing_field("id"))?;
                let transports = transports.unwrap_or_default();

                Ok(PublicKeyCredentialDescriptor { id, transports })
            }
        }

        deserializer.deserialize_any(PublicKeyCredentialDescriptorVisitor)
    }
}

impl From<&KeyHandle> for PublicKeyCredentialDescriptor {
    fn from(kh: &KeyHandle) -> Self {
        Self {
            id: kh.credential.clone(),
            transports: kh.transports.into(),
        }
    }
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum ResidentKeyRequirement {
    Discouraged,
    Preferred,
    Required,
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum UserVerificationRequirement {
    Discouraged,
    Preferred,
    Required,
}

#[derive(Copy, Clone, Debug, Eq, PartialEq)]
pub enum CredentialProtectionPolicy {
    UserVerificationOptional = 1,
    UserVerificationOptionalWithCredentialIDList = 2,
    UserVerificationRequired = 3,
}

impl Serialize for CredentialProtectionPolicy {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_u64(*self as u64)
    }
}

impl<'de> Deserialize<'de> for CredentialProtectionPolicy {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct CredentialProtectionPolicyVisitor;

        impl<'de> Visitor<'de> for CredentialProtectionPolicyVisitor {
            type Value = CredentialProtectionPolicy;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("an integer")
            }

            fn visit_u64<E>(self, v: u64) -> Result<Self::Value, E>
            where
                E: SerdeError,
            {
                match v {
                    1 => Ok(CredentialProtectionPolicy::UserVerificationOptional),
                    2 => Ok(
                        CredentialProtectionPolicy::UserVerificationOptionalWithCredentialIDList,
                    ),
                    3 => Ok(CredentialProtectionPolicy::UserVerificationRequired),
                    _ => Err(SerdeError::invalid_value(
                        Unexpected::Unsigned(v),
                        &"valid CredentialProtectionPolicy",
                    )),
                }
            }
        }

        deserializer.deserialize_any(CredentialProtectionPolicyVisitor)
    }
}

#[derive(Clone, Debug, Default)]
pub struct AuthenticationExtensionsClientInputs {
    pub app_id: Option<String>,
    pub cred_props: Option<bool>,
    pub credential_protection_policy: Option<CredentialProtectionPolicy>,
    pub enforce_credential_protection_policy: Option<bool>,
    pub hmac_create_secret: Option<bool>,
    pub min_pin_length: Option<bool>,
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct CredentialProperties {
    pub rk: bool,
}

#[derive(Clone, Debug, Default, Eq, PartialEq)]
pub struct AuthenticationExtensionsClientOutputs {
    pub app_id: Option<bool>,
    pub cred_props: Option<CredentialProperties>,
    pub hmac_create_secret: Option<bool>,
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum AuthenticatorAttachment {
    CrossPlatform,
    Platform,
    Unknown,
}

#[cfg(test)]
mod test {
    use super::{
        COSEAlgorithm, PublicKeyCredentialDescriptor, PublicKeyCredentialParameters,
        PublicKeyCredentialUserEntity, RelyingParty, Transport,
    };
    use serde_cbor::from_slice;

    fn create_user() -> PublicKeyCredentialUserEntity {
        PublicKeyCredentialUserEntity {
            id: vec![
                0x30, 0x82, 0x01, 0x93, 0x30, 0x82, 0x01, 0x38, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x30,
                0x82, 0x01, 0x93, 0x30, 0x82, 0x01, 0x38, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x30, 0x82,
                0x01, 0x93, 0x30, 0x82,
            ],
            name: Some(String::from("johnpsmith@example.com")),
            display_name: Some(String::from("John P. Smith")),
        }
    }
    #[test]
    fn serialize_rp() {
        let rp = RelyingParty {
            id: String::from("Acme"),
            name: None,
        };

        let payload = ser::to_vec(&rp).unwrap();
        assert_eq!(
            &payload,
            &[
                0xa1, // map(1)
                0x62, // text(2)
                0x69, 0x64, // "id"
                0x64, // text(4)
                0x41, 0x63, 0x6d, 0x65
            ]
        );
    }

    #[test]
    fn test_deserialize_user() {
        // This includes an obsolete "icon" field to test that deserialization
        // ignores it.
        let input = vec![
            0xa4, // map(4)
            0x62, // text(2)
            0x69, 0x64, // "id"
            0x58, 0x20, // bytes(32)
            0x30, 0x82, 0x01, 0x93, 0x30, 0x82, 0x01, 0x38, 0xa0, 0x03, // userid
            0x02, 0x01, 0x02, 0x30, 0x82, 0x01, 0x93, 0x30, 0x82, 0x01, // ...
            0x38, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x30, 0x82, 0x01, 0x93, // ...
            0x30, 0x82, // ...
            0x64, // text(4)
            0x69, 0x63, 0x6f, 0x6e, // "icon"
            0x78, 0x2b, // text(43)
            0x68, 0x74, 0x74, 0x70, 0x73, 0x3a, 0x2f, 0x2f, 0x70,
            0x69, // "https://pics.example.com/00/p/aBjjjpqPb.png"
            0x63, 0x73, 0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, // ...
            0x2e, 0x63, 0x6f, 0x6d, 0x2f, 0x30, 0x30, 0x2f, 0x70, 0x2f, // ...
            0x61, 0x42, 0x6a, 0x6a, 0x6a, 0x70, 0x71, 0x50, 0x62, 0x2e, // ...
            0x70, 0x6e, 0x67, // ...
            0x64, // text(4)
            0x6e, 0x61, 0x6d, 0x65, // "name"
            0x76, // text(22)
            0x6a, 0x6f, 0x68, 0x6e, 0x70, 0x73, 0x6d, 0x69, 0x74,
            0x68, // "johnpsmith@example.com"
            0x40, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, // ...
            0x6f, 0x6d, // ...
            0x6b, // text(11)
            0x64, 0x69, 0x73, 0x70, 0x6c, 0x61, 0x79, 0x4e, 0x61, 0x6d, // "displayName"
            0x65, // ...
            0x6d, // text(13)
            0x4a, 0x6f, 0x68, 0x6e, 0x20, 0x50, 0x2e, 0x20, 0x53, 0x6d, // "John P. Smith"
            0x69, 0x74, 0x68, // ...
        ];
        let expected = create_user();
        let actual: PublicKeyCredentialUserEntity = from_slice(&input).unwrap();
        assert_eq!(expected, actual);
    }

    #[test]
    fn serialize_user() {
        let user = create_user();

        let payload = ser::to_vec(&user).unwrap();
        println!("payload = {payload:?}");
        assert_eq!(
            payload,
            vec![
                0xa3, // map(3)
                0x62, // text(2)
                0x69, 0x64, // "id"
                0x58, 0x20, // bytes(32)
                0x30, 0x82, 0x01, 0x93, 0x30, 0x82, 0x01, 0x38, 0xa0, 0x03, // userid
                0x02, 0x01, 0x02, 0x30, 0x82, 0x01, 0x93, 0x30, 0x82, 0x01, // ...
                0x38, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x30, 0x82, 0x01, 0x93, // ...
                0x30, 0x82, // ...
                0x64, // text(4)
                0x6e, 0x61, 0x6d, 0x65, // "name"
                0x76, // text(22)
                0x6a, 0x6f, 0x68, 0x6e, 0x70, 0x73, 0x6d, 0x69, 0x74,
                0x68, // "johnpsmith@example.com"
                0x40, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, // ...
                0x6f, 0x6d, // ...
                0x6b, // text(11)
                0x64, 0x69, 0x73, 0x70, 0x6c, 0x61, 0x79, 0x4e, 0x61, 0x6d, // "displayName"
                0x65, // ...
                0x6d, // text(13)
                0x4a, 0x6f, 0x68, 0x6e, 0x20, 0x50, 0x2e, 0x20, 0x53, 0x6d, // "John P. Smith"
                0x69, 0x74, 0x68, // ...
            ]
        );
    }

    #[test]
    fn serialize_user_nodisplayname() {
        let user = PublicKeyCredentialUserEntity {
            id: vec![
                0x30, 0x82, 0x01, 0x93, 0x30, 0x82, 0x01, 0x38, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x30,
                0x82, 0x01, 0x93, 0x30, 0x82, 0x01, 0x38, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x30, 0x82,
                0x01, 0x93, 0x30, 0x82,
            ],
            name: Some(String::from("johnpsmith@example.com")),
            display_name: None,
        };

        let payload = ser::to_vec(&user).unwrap();
        println!("payload = {payload:?}");
        assert_eq!(
            payload,
            vec![
                0xa2, // map(2)
                0x62, // text(2)
                0x69, 0x64, // "id"
                0x58, 0x20, // bytes(32)
                0x30, 0x82, 0x01, 0x93, 0x30, 0x82, 0x01, 0x38, 0xa0, 0x03, // userid
                0x02, 0x01, 0x02, 0x30, 0x82, 0x01, 0x93, 0x30, 0x82, 0x01, // ...
                0x38, 0xa0, 0x03, 0x02, 0x01, 0x02, 0x30, 0x82, 0x01, 0x93, // ...
                0x30, 0x82, // ...
                0x64, // text(4)
                0x6e, 0x61, 0x6d, 0x65, // "name"
                0x76, // text(22)
                0x6a, 0x6f, 0x68, 0x6e, 0x70, 0x73, 0x6d, 0x69, 0x74,
                0x68, // "johnpsmith@example.com"
                0x40, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, // ...
                0x6f, 0x6d, // ...
            ]
        );
    }

    use serde_cbor::ser;

    #[test]
    fn public_key() {
        let keys = vec![
            PublicKeyCredentialParameters {
                alg: COSEAlgorithm::ES256,
            },
            PublicKeyCredentialParameters {
                alg: COSEAlgorithm::RS256,
            },
        ];

        let payload = ser::to_vec(&keys);
        println!("payload = {payload:?}");
        let payload = payload.unwrap();
        assert_eq!(
            payload,
            vec![
                0x82, // array(2)
                0xa2, // map(2)
                0x63, // text(3)
                0x61, 0x6c, 0x67, // "alg"
                0x26, // -7 (ES256)
                0x64, // text(4)
                0x74, 0x79, 0x70, 0x65, // "type"
                0x6a, // text(10)
                0x70, 0x75, 0x62, 0x6C, 0x69, 0x63, // "public-key"
                0x2D, 0x6B, 0x65, 0x79, // ...
                0xa2, // map(2)
                0x63, // text(3)
                0x61, 0x6c, 0x67, // "alg"
                0x39, 0x01, 0x00, // -257 (RS256)
                0x64, // text(4)
                0x74, 0x79, 0x70, 0x65, // "type"
                0x6a, // text(10)
                0x70, 0x75, 0x62, 0x6C, 0x69, 0x63, // "public-key"
                0x2D, 0x6B, 0x65, 0x79 // ...
            ]
        );
    }

    #[test]
    fn public_key_desc() {
        let key = PublicKeyCredentialDescriptor {
            id: vec![
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, 0x0c, 0x0d,
                0x0e, 0x0f, 0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1a, 0x1b,
                0x1c, 0x1d, 0x1e, 0x1f,
            ],
            transports: vec![Transport::BLE, Transport::USB],
        };

        let payload = ser::to_vec(&key);
        println!("payload = {payload:?}");
        let payload = payload.unwrap();

        assert_eq!(
            payload,
            vec![
                // 0xa3, // map(3)
                0xa2, // map(2)
                0x62, // text(2)
                0x69, 0x64, // "id"
                0x58, 0x20, // bytes(32)
                0x00, 0x01, 0x02, 0x03, 0x04, 0x05, // key id
                0x06, 0x07, 0x08, 0x09, 0x0a, 0x0b, // ...
                0x0c, 0x0d, 0x0e, 0x0f, 0x10, 0x11, // ...
                0x12, 0x13, 0x14, 0x15, 0x16, 0x17, // ...
                0x18, 0x19, 0x1a, 0x1b, 0x1c, 0x1d, // ...
                0x1e, 0x1f, // ...
                0x64, // text(4)
                0x74, 0x79, 0x70, 0x65, // "type"
                0x6a, // text(10)
                0x70, 0x75, 0x62, 0x6C, 0x69, 0x63, // "public-key"
                0x2D, 0x6B, 0x65,
                0x79, // ...

                      // Deactivated for now
                      //0x6a, // text(10)
                      //0x74, 0x72, 0x61, 0x6e, 0x73, 0x70, // "transports"
                      //0x6f, 0x72, 0x74, 0x73, // ...
                      //0x82, // array(2)
                      //0x63, // text(3)
                      //0x62, 0x6c, 0x65, // "ble"
                      //0x63, // text(3)
                      //0x75, 0x73, 0x62 // "usb"
            ]
        );
    }
}
