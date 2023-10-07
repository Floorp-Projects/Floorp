use super::client_data::ClientDataHash;
use super::commands::get_assertion::{GetAssertion, GetAssertionExtensions, GetAssertionOptions};
use super::commands::{PinUvAuthCommand, RequestCtap1, Retryable};
use crate::consts::{PARAMETER_SIZE, U2F_AUTHENTICATE, U2F_CHECK_IS_REGISTERED};
use crate::crypto::PinUvAuthToken;
use crate::ctap2::server::{PublicKeyCredentialDescriptor, RelyingParty};
use crate::errors::AuthenticatorError;
use crate::transport::errors::{ApduErrorStatus, HIDError};
use crate::transport::{FidoDevice, FidoProtocol, VirtualFidoDevice};
use crate::u2ftypes::CTAP1RequestAPDU;
use sha2::{Digest, Sha256};

/// This command is used to check which key_handle is valid for this
/// token. This is sent before a GetAssertion command, to determine which
/// is valid for a specific token and which key_handle GetAssertion
/// should send to the token. Or before a MakeCredential command, to determine
/// if this token is already registered or not.
#[derive(Debug)]
pub struct CheckKeyHandle<'assertion> {
    pub key_handle: &'assertion [u8],
    pub client_data_hash: &'assertion [u8],
    pub rp: &'assertion RelyingParty,
}

impl<'assertion> RequestCtap1 for CheckKeyHandle<'assertion> {
    type Output = ();
    type AdditionalInfo = ();

    fn ctap1_format(&self) -> Result<(Vec<u8>, Self::AdditionalInfo), HIDError> {
        // In theory, we only need to do this for up=true, for up=false, we could
        // use U2F_DONT_ENFORCE_USER_PRESENCE_AND_SIGN instead and use the answer directly.
        // But that would involve another major refactoring to implement, and so we accept
        // that we will send the final request twice to the authenticator. Once with
        // U2F_CHECK_IS_REGISTERED followed by U2F_DONT_ENFORCE_USER_PRESENCE_AND_SIGN.
        let flags = U2F_CHECK_IS_REGISTERED;
        let mut auth_data = Vec::with_capacity(2 * PARAMETER_SIZE + 1 + self.key_handle.len());

        auth_data.extend_from_slice(self.client_data_hash);
        auth_data.extend_from_slice(self.rp.hash().as_ref());
        auth_data.extend_from_slice(&[self.key_handle.len() as u8]);
        auth_data.extend_from_slice(self.key_handle);
        let cmd = U2F_AUTHENTICATE;
        let apdu = CTAP1RequestAPDU::serialize(cmd, flags, &auth_data)?;
        Ok((apdu, ()))
    }

    fn handle_response_ctap1<Dev: FidoDevice>(
        &self,
        _dev: &mut Dev,
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

    fn send_to_virtual_device<Dev: VirtualFidoDevice>(
        &self,
        dev: &mut Dev,
    ) -> Result<Self::Output, HIDError> {
        dev.check_key_handle(self)
    }
}

/// "pre-flight": In order to determine whether authenticatorMakeCredential's excludeList or
/// authenticatorGetAssertion's allowList contain credential IDs that are already
/// present on an authenticator, a platform typically invokes authenticatorGetAssertion
/// with the "up" option key set to false and optionally pinUvAuthParam one or more times.
/// For CTAP1, the resulting list will always be of length 1.
pub(crate) fn do_credential_list_filtering_ctap1<Dev: FidoDevice>(
    dev: &mut Dev,
    cred_list: &[PublicKeyCredentialDescriptor],
    rp: &RelyingParty,
    client_data_hash: &ClientDataHash,
) -> Option<PublicKeyCredentialDescriptor> {
    let key_handle = cred_list
        .iter()
        // key-handles in CTAP1 are limited to 255 bytes, but are not limited in CTAP2.
        // Filter out key-handles that are too long (can happen if this is a CTAP2-request,
        // but the token only speaks CTAP1).
        .filter(|key_handle| key_handle.id.len() < 256)
        .find_map(|key_handle| {
            let check_command = CheckKeyHandle {
                key_handle: key_handle.id.as_ref(),
                client_data_hash: client_data_hash.as_ref(),
                rp,
            };
            let res = dev.send_ctap1(&check_command);
            match res {
                Ok(_) => Some(key_handle.clone()),
                _ => None,
            }
        });
    key_handle
}

/// "pre-flight": In order to determine whether authenticatorMakeCredential's excludeList or
/// authenticatorGetAssertion's allowList contain credential IDs that are already
/// present on an authenticator, a platform typically invokes authenticatorGetAssertion
/// with the "up" option key set to false and optionally pinUvAuthParam one or more times.
pub(crate) fn do_credential_list_filtering_ctap2<Dev: FidoDevice>(
    dev: &mut Dev,
    cred_list: &[PublicKeyCredentialDescriptor],
    rp: &RelyingParty,
    pin_uv_auth_token: Option<PinUvAuthToken>,
) -> Result<Vec<PublicKeyCredentialDescriptor>, AuthenticatorError> {
    let info = dev
        .get_authenticator_info()
        .ok_or(HIDError::DeviceNotInitialized)?;
    let mut cred_list = cred_list.to_vec();
    // Step 1.0: Find out how long the exclude_list/allow_list is allowed to be
    //           If the token doesn't tell us, we assume a length of 1
    let mut chunk_size = match info.max_credential_count_in_list {
        // Length 0 is not allowed by the spec, so we assume the device can't be trusted, which means
        // falling back to a chunk size of 1 as the bare minimum.
        None | Some(0) => 1,
        Some(x) => x,
    };

    // Step 1.1: The device only supports keys up to a certain length.
    //           Filter out all keys that are longer, because they can't be
    //           from this device anyways.
    match info.max_credential_id_length {
        None => { /* no-op */ }
        // Length 0 is not allowed by the spec, so we assume the device can't be trusted, which means
        // falling back to a chunk size of 1 as the bare minimum.
        Some(0) => {
            chunk_size = 1;
        }
        Some(max_key_length) => {
            cred_list.retain(|k| k.id.len() <= max_key_length);
        }
    }

    let chunked_list = cred_list.chunks(chunk_size);

    // Step 2: If we have more than one chunk: Loop over all, doing GetAssertion
    //         and if one of them comes back with a success, use only that chunk.
    let mut final_list = Vec::new();
    for chunk in chunked_list {
        let mut silent_assert = GetAssertion::new(
            ClientDataHash(Sha256::digest("").into()),
            rp.clone(),
            chunk.to_vec(),
            GetAssertionOptions {
                user_verification: None, // defaults to Some(false) if puap is absent
                user_presence: Some(false),
            },
            GetAssertionExtensions::default(),
        );
        silent_assert.set_pin_uv_auth_param(pin_uv_auth_token.clone())?;
        match dev.send_msg(&silent_assert) {
            Ok(mut response) => {
                // This chunk contains a key_handle that is already known to the device.
                // Filter out all credentials the device returned. Those are valid.
                let credential_ids = response
                    .iter_mut()
                    .filter_map(|result| result.assertion.credentials.take())
                    .collect();
                // Replace credential_id_list with the valid credentials
                final_list = credential_ids;
                break;
            }
            Err(_) => {
                // No-op: Go to next chunk.
                // NOTE: while we expect a StatusCode::NoCredentials error here, some tokens return
                // other values.
                continue;
            }
        }
    }

    // Step 3: Now ExcludeList/AllowList is either empty or has one batch with a 'known' credential.
    //         Send it as a normal Request and expect a "CredentialExcluded"-error in case of
    //         MakeCredential or a Success in case of GetAssertion
    Ok(final_list)
}

pub(crate) fn silently_discover_credentials<Dev: FidoDevice>(
    dev: &mut Dev,
    cred_list: &[PublicKeyCredentialDescriptor],
    rp: &RelyingParty,
    client_data_hash: &ClientDataHash,
) -> Vec<PublicKeyCredentialDescriptor> {
    if dev.get_protocol() == FidoProtocol::CTAP2 {
        if let Ok(cred_list) = do_credential_list_filtering_ctap2(dev, cred_list, rp, None) {
            return cred_list;
        }
    } else if let Some(key_handle) =
        do_credential_list_filtering_ctap1(dev, cred_list, rp, client_data_hash)
    {
        return vec![key_handle];
    }
    vec![]
}
