use super::commands::CommandError;
use crate::transport::errors::HIDError;
use base64::Engine;
use serde::de::{self, Deserializer, Error as SerdeError, MapAccess, Visitor};
use serde::ser::SerializeMap;
use serde::{Deserialize, Serialize, Serializer};
use serde_json as json;
use sha2::{Digest, Sha256};
use std::fmt;

/// https://w3c.github.io/webauthn/#dom-collectedclientdata-tokenbinding
// tokenBinding, of type TokenBinding
//
//    This OPTIONAL member contains information about the state of the Token
//    Binding protocol [TokenBinding] used when communicating with the Relying
//    Party. Its absence indicates that the client doesn’t support token
//    binding.
//
//    status, of type TokenBindingStatus
//
//        This member is one of the following:
//
//        supported
//
//            Indicates the client supports token binding, but it was not
//            negotiated when communicating with the Relying Party.
//
//        present
//
//            Indicates token binding was used when communicating with the
//            Relying Party. In this case, the id member MUST be present.
//
//    id, of type DOMString
//
//        This member MUST be present if status is present, and MUST be a
//        base64url encoding of the Token Binding ID that was used when
//        communicating with the Relying Party.
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum TokenBinding {
    Present(String),
    Supported,
}

impl Serialize for TokenBinding {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut map = serializer.serialize_map(Some(2))?;
        match *self {
            TokenBinding::Supported => {
                map.serialize_entry(&"status", &"supported")?;
            }
            TokenBinding::Present(ref v) => {
                map.serialize_entry(&"status", "present")?;
                // Verify here, that `v` is valid base64 encoded?
                // base64::decode_config(&v, base64::URL_SAFE_NO_PAD);
                // For now: Let the token do that.
                map.serialize_entry(&"id", &v)?;
            }
        }
        map.end()
    }
}

impl<'de> Deserialize<'de> for TokenBinding {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct TokenBindingVisitor;

        impl<'de> Visitor<'de> for TokenBindingVisitor {
            type Value = TokenBinding;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a byte string")
            }

            fn visit_map<M>(self, mut map: M) -> Result<Self::Value, M::Error>
            where
                M: MapAccess<'de>,
            {
                let mut id = None;
                let mut status = None;

                while let Some(key) = map.next_key()? {
                    match key {
                        "status" => {
                            status = Some(map.next_value()?);
                        }
                        "id" => {
                            id = Some(map.next_value()?);
                        }
                        k => {
                            return Err(M::Error::custom(format!("unexpected key: {k:?}")));
                        }
                    }
                }

                if let Some(stat) = status {
                    match stat {
                        "present" => {
                            if let Some(id) = id {
                                Ok(TokenBinding::Present(id))
                            } else {
                                Err(SerdeError::missing_field("id"))
                            }
                        }
                        "supported" => Ok(TokenBinding::Supported),
                        k => Err(M::Error::custom(format!("unexpected status key: {k:?}"))),
                    }
                } else {
                    Err(SerdeError::missing_field("status"))
                }
            }
        }

        deserializer.deserialize_map(TokenBindingVisitor)
    }
}

/// https://w3c.github.io/webauthn/#dom-collectedclientdata-type
// type, of type DOMString
//
//    This member contains the string "webauthn.create" when creating new
//    credentials, and "webauthn.get" when getting an assertion from an
//    existing credential. The purpose of this member is to prevent certain
//    types of signature confusion attacks (where an attacker substitutes one
//    legitimate signature for another).
#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub enum WebauthnType {
    Create,
    Get,
}

impl Serialize for WebauthnType {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        match *self {
            WebauthnType::Create => serializer.serialize_str("webauthn.create"),
            WebauthnType::Get => serializer.serialize_str("webauthn.get"),
        }
    }
}

impl<'de> Deserialize<'de> for WebauthnType {
    fn deserialize<D>(deserializer: D) -> Result<Self, D::Error>
    where
        D: Deserializer<'de>,
    {
        struct WebauthnTypeVisitor;

        impl<'de> Visitor<'de> for WebauthnTypeVisitor {
            type Value = WebauthnType;

            fn expecting(&self, formatter: &mut fmt::Formatter) -> fmt::Result {
                formatter.write_str("a string")
            }

            fn visit_str<E>(self, v: &str) -> Result<Self::Value, E>
            where
                E: de::Error,
            {
                match v {
                    "webauthn.create" => Ok(WebauthnType::Create),
                    "webauthn.get" => Ok(WebauthnType::Get),
                    _ => Err(E::custom("unexpected webauthn_type")),
                }
            }
        }

        deserializer.deserialize_str(WebauthnTypeVisitor)
    }
}

#[derive(Serialize, Deserialize, Clone, PartialEq, Eq, Debug)]
pub struct Challenge(pub String);

impl Challenge {
    pub fn new(input: Vec<u8>) -> Self {
        let value = base64::engine::general_purpose::URL_SAFE_NO_PAD.encode(input);
        Challenge(value)
    }
}

impl From<Vec<u8>> for Challenge {
    fn from(v: Vec<u8>) -> Challenge {
        Challenge::new(v)
    }
}

impl AsRef<[u8]> for Challenge {
    fn as_ref(&self) -> &[u8] {
        self.0.as_bytes()
    }
}

pub type Origin = String;

#[derive(Debug, Deserialize, Serialize, Clone, PartialEq, Eq)]
pub struct CollectedClientData {
    #[serde(rename = "type")]
    pub webauthn_type: WebauthnType,
    pub challenge: Challenge,
    pub origin: Origin,
    // It is optional, according to https://www.w3.org/TR/webauthn/#collectedclientdata-hash-of-the-serialized-client-data
    // But we are serializing it, so we *have to* set crossOrigin (if not given, we have to set it to false)
    // Thus, on our side, it is not optional. For deserializing, we provide a default (bool's default == False)
    #[serde(rename = "crossOrigin", default)]
    pub cross_origin: bool,
    #[serde(rename = "tokenBinding", skip_serializing_if = "Option::is_none")]
    pub token_binding: Option<TokenBinding>,
}

impl CollectedClientData {
    pub fn hash(&self) -> Result<ClientDataHash, HIDError> {
        // WebIDL's dictionary definition specifies that the order of the struct
        // is exactly as the WebIDL specification declares it, with an algorithm
        // for partial dictionaries, so that's how interop works for these
        // things.
        // See: https://heycam.github.io/webidl/#dfn-dictionary
        let json = json::to_vec(&self).map_err(CommandError::Json)?;
        let digest = Sha256::digest(json);
        Ok(ClientDataHash(digest.into()))
    }
}

#[derive(Clone, Debug, Eq, PartialEq)]
pub struct ClientDataHash(pub [u8; 32]);

impl PartialEq<[u8]> for ClientDataHash {
    fn eq(&self, other: &[u8]) -> bool {
        self.0.eq(other)
    }
}

impl AsRef<[u8]> for ClientDataHash {
    fn as_ref(&self) -> &[u8] {
        &self.0
    }
}

impl Serialize for ClientDataHash {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        serializer.serialize_bytes(&self.0)
    }
}

#[cfg(test)]
mod test {
    use super::{Challenge, ClientDataHash, CollectedClientData, TokenBinding, WebauthnType};
    use serde_json as json;

    #[test]
    fn test_token_binding_status() {
        let tok = TokenBinding::Present("AAECAw".to_string());

        let json_value = json::to_string(&tok).unwrap();
        assert_eq!(json_value, "{\"status\":\"present\",\"id\":\"AAECAw\"}");

        let tok = TokenBinding::Supported;

        let json_value = json::to_string(&tok).unwrap();
        assert_eq!(json_value, "{\"status\":\"supported\"}");
    }

    #[test]
    fn test_webauthn_type() {
        let t = WebauthnType::Create;

        let json_value = json::to_string(&t).unwrap();
        assert_eq!(json_value, "\"webauthn.create\"");

        let t = WebauthnType::Get;
        let json_value = json::to_string(&t).unwrap();
        assert_eq!(json_value, "\"webauthn.get\"");
    }

    #[test]
    fn test_collected_client_data_parsing() {
        let original_str = "{\"type\":\"webauthn.create\",\"challenge\":\"AAECAw\",\"origin\":\"example.com\",\"crossOrigin\":false,\"tokenBinding\":{\"status\":\"present\",\"id\":\"AAECAw\"}}";
        let parsed: CollectedClientData = serde_json::from_str(original_str).unwrap();
        let expected = CollectedClientData {
            webauthn_type: WebauthnType::Create,
            challenge: Challenge::new(vec![0x00, 0x01, 0x02, 0x03]),
            origin: String::from("example.com"),
            cross_origin: false,
            token_binding: Some(TokenBinding::Present("AAECAw".to_string())),
        };
        assert_eq!(parsed, expected);

        let back_again = serde_json::to_string(&expected).unwrap();
        assert_eq!(back_again, original_str);
    }

    #[test]
    fn test_collected_client_data_defaults() {
        let cross_origin_str = "{\"type\":\"webauthn.create\",\"challenge\":\"AAECAw\",\"origin\":\"example.com\",\"crossOrigin\":false,\"tokenBinding\":{\"status\":\"present\",\"id\":\"AAECAw\"}}";
        let no_cross_origin_str = "{\"type\":\"webauthn.create\",\"challenge\":\"AAECAw\",\"origin\":\"example.com\",\"tokenBinding\":{\"status\":\"present\",\"id\":\"AAECAw\"}}";
        let parsed: CollectedClientData = serde_json::from_str(no_cross_origin_str).unwrap();
        let expected = CollectedClientData {
            webauthn_type: WebauthnType::Create,
            challenge: Challenge::new(vec![0x00, 0x01, 0x02, 0x03]),
            origin: String::from("example.com"),
            cross_origin: false,
            token_binding: Some(TokenBinding::Present("AAECAw".to_string())),
        };
        assert_eq!(parsed, expected);

        let back_again = serde_json::to_string(&expected).unwrap();
        assert_eq!(back_again, cross_origin_str);
    }

    #[test]
    fn test_collected_client_data() {
        let client_data = CollectedClientData {
            webauthn_type: WebauthnType::Create,
            challenge: Challenge::new(vec![0x00, 0x01, 0x02, 0x03]),
            origin: String::from("example.com"),
            cross_origin: false,
            token_binding: Some(TokenBinding::Present("AAECAw".to_string())),
        };
        assert_eq!(
            client_data.hash().expect("failed to serialize client data"),
            //  echo -n '{"type":"webauthn.create","challenge":"AAECAw","origin":"example.com","crossOrigin":false,"tokenBinding":{"status":"present","id":"AAECAw"}}' | sha256sum -t
            ClientDataHash([
                0x75, 0x35, 0x35, 0x7d, 0x49, 0x6e, 0x33, 0xc8, 0x18, 0x7f, 0xea, 0x8d, 0x11, 0x32,
                0x64, 0xaa, 0xa4, 0x52, 0x3e, 0x13, 0x40, 0x14, 0x9f, 0xbe, 0x00, 0x3f, 0x10, 0x87,
                0x54, 0xc3, 0x2d, 0x80
            ])
        );
    }
}
