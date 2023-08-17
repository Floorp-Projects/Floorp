use super::{Command, CommandError, PinUvAuthCommand, RequestCtap2, StatusCode};
use crate::{
    crypto::{PinUvAuthParam, PinUvAuthToken},
    ctap2::server::UserVerificationRequirement,
    errors::AuthenticatorError,
    transport::errors::HIDError,
    AuthenticatorInfo, FidoDevice,
};
use serde::{ser::SerializeMap, Deserialize, Serialize, Serializer};
use serde_cbor::{de::from_slice, to_vec, Value};

#[derive(Debug, Clone, Deserialize)]
pub struct SetMinPINLength {
    /// Minimum PIN length in code points
    pub new_min_pin_length: Option<u64>,
    /// RP IDs which are allowed to get this information via the minPinLength extension.
    /// This parameter MUST NOT be used unless the minPinLength extension is supported.  
    pub min_pin_length_rpids: Option<Vec<String>>,
    /// The authenticator returns CTAP2_ERR_PIN_POLICY_VIOLATION until changePIN is successful.    
    pub force_change_pin: Option<bool>,
}

impl Serialize for SetMinPINLength {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        let mut map_len = 0;
        if self.new_min_pin_length.is_some() {
            map_len += 1;
        }
        if self.min_pin_length_rpids.is_some() {
            map_len += 1;
        }
        if self.force_change_pin.is_some() {
            map_len += 1;
        }

        let mut map = serializer.serialize_map(Some(map_len))?;
        if let Some(new_min_pin_length) = self.new_min_pin_length {
            map.serialize_entry(&0x01, &new_min_pin_length)?;
        }
        if let Some(min_pin_length_rpids) = &self.min_pin_length_rpids {
            map.serialize_entry(&0x02, &min_pin_length_rpids)?;
        }
        if let Some(force_change_pin) = self.force_change_pin {
            map.serialize_entry(&0x03, &force_change_pin)?;
        }
        map.end()
    }
}

#[derive(Debug, Clone, Deserialize, Serialize)]
pub enum AuthConfigCommand {
    EnableEnterpriseAttestation,
    ToggleAlwaysUv,
    SetMinPINLength(SetMinPINLength),
}

impl AuthConfigCommand {
    fn has_params(&self) -> bool {
        match self {
            AuthConfigCommand::EnableEnterpriseAttestation => false,
            AuthConfigCommand::ToggleAlwaysUv => false,
            AuthConfigCommand::SetMinPINLength(..) => true,
        }
    }
}

#[derive(Debug, Serialize)]
pub enum AuthConfigResult {
    Success(AuthenticatorInfo),
}

#[derive(Debug)]
pub struct AuthenticatorConfig {
    subcommand: AuthConfigCommand, // subCommand currently being requested
    pin_uv_auth_param: Option<PinUvAuthParam>, // First 16 bytes of HMAC-SHA-256 of contents using pinUvAuthToken.
}

impl AuthenticatorConfig {
    pub(crate) fn new(subcommand: AuthConfigCommand) -> Self {
        Self {
            subcommand,
            pin_uv_auth_param: None,
        }
    }
}

impl Serialize for AuthenticatorConfig {
    fn serialize<S>(&self, serializer: S) -> Result<S::Ok, S::Error>
    where
        S: Serializer,
    {
        // Need to define how many elements are going to be in the map
        // beforehand
        let mut map_len = 1;
        if self.pin_uv_auth_param.is_some() {
            map_len += 2;
        }
        if self.subcommand.has_params() {
            map_len += 1;
        }

        let mut map = serializer.serialize_map(Some(map_len))?;

        match &self.subcommand {
            AuthConfigCommand::EnableEnterpriseAttestation => {
                map.serialize_entry(&0x01, &0x01)?;
            }
            AuthConfigCommand::ToggleAlwaysUv => {
                map.serialize_entry(&0x01, &0x02)?;
            }
            AuthConfigCommand::SetMinPINLength(params) => {
                map.serialize_entry(&0x01, &0x03)?;
                map.serialize_entry(&0x02, &params)?;
            }
        }

        if let Some(ref pin_uv_auth_param) = self.pin_uv_auth_param {
            map.serialize_entry(&0x03, &pin_uv_auth_param.pin_protocol.id())?;
            map.serialize_entry(&0x04, pin_uv_auth_param)?;
        }

        map.end()
    }
}

impl RequestCtap2 for AuthenticatorConfig {
    type Output = ();

    fn command(&self) -> Command {
        Command::AuthenticatorConfig
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
            Ok(())
        } else {
            let msg = if input.len() > 1 {
                let data: Value = from_slice(&input[1..]).map_err(CommandError::Deserializing)?;
                Some(data)
            } else {
                None
            };
            Err(CommandError::StatusCode(status, msg).into())
        }
    }

    fn send_to_virtual_device<Dev: crate::VirtualFidoDevice>(
        &self,
        _dev: &mut Dev,
    ) -> Result<Self::Output, HIDError> {
        unimplemented!()
    }
}

impl PinUvAuthCommand for AuthenticatorConfig {
    fn set_pin_uv_auth_param(
        &mut self,
        pin_uv_auth_token: Option<PinUvAuthToken>,
    ) -> Result<(), AuthenticatorError> {
        let mut param = None;
        if let Some(token) = pin_uv_auth_token {
            // pinUvAuthParam (0x04): the result of calling
            // authenticate(pinUvAuthToken, 32×0xff || 0x0d || uint8(subCommand) || subCommandParams).
            let mut data = vec![0xff; 32];
            data.push(0x0D);
            match &self.subcommand {
                AuthConfigCommand::EnableEnterpriseAttestation => {
                    data.push(0x01);
                }
                AuthConfigCommand::ToggleAlwaysUv => {
                    data.push(0x02);
                }
                AuthConfigCommand::SetMinPINLength(params) => {
                    data.push(0x03);
                    data.extend(to_vec(params).map_err(CommandError::Serializing)?);
                }
            }
            param = Some(token.derive(&data).map_err(CommandError::Crypto)?);
        }
        self.pin_uv_auth_param = param;
        Ok(())
    }

    fn can_skip_user_verification(
        &mut self,
        authinfo: &AuthenticatorInfo,
        _uv_req: UserVerificationRequirement,
    ) -> bool {
        !authinfo.device_is_protected()
    }

    fn set_uv_option(&mut self, _uv: Option<bool>) {
        /* No-op */
    }

    fn get_pin_uv_auth_param(&self) -> Option<&PinUvAuthParam> {
        self.pin_uv_auth_param.as_ref()
    }

    fn get_rp_id(&self) -> Option<&String> {
        None
    }
}
