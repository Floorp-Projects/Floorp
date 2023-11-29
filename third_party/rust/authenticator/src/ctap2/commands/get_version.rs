use super::{CommandError, CtapResponse, RequestCtap1, Retryable};
use crate::consts::U2F_VERSION;
use crate::transport::errors::{ApduErrorStatus, HIDError};
use crate::transport::{FidoDevice, VirtualFidoDevice};
use crate::u2ftypes::CTAP1RequestAPDU;

#[allow(non_camel_case_types)]
#[derive(Debug, Clone, PartialEq, Eq)]
pub enum U2FInfo {
    U2F_V2,
}

impl CtapResponse for U2FInfo {}

#[derive(Debug, Default)]
// TODO(baloo): if one does not issue U2F_VERSION before makecredentials or getassertion, token
//              will return error (ConditionsNotSatified), test this in unit tests
pub struct GetVersion {}

impl RequestCtap1 for GetVersion {
    type Output = U2FInfo;
    type AdditionalInfo = ();

    fn handle_response_ctap1<Dev: FidoDevice>(
        &self,
        _dev: &mut Dev,
        _status: Result<(), ApduErrorStatus>,
        input: &[u8],
        _add_info: &(),
    ) -> Result<Self::Output, Retryable<HIDError>> {
        if input.is_empty() {
            return Err(Retryable::Error(HIDError::Command(
                CommandError::InputTooSmall,
            )));
        }

        let expected = String::from("U2F_V2");
        let result = String::from_utf8_lossy(input);
        match result {
            ref data if data == &expected => Ok(U2FInfo::U2F_V2),
            _ => Err(Retryable::Error(HIDError::UnexpectedVersion)),
        }
    }

    fn ctap1_format(&self) -> Result<(Vec<u8>, ()), HIDError> {
        let flags = 0;

        let cmd = U2F_VERSION;
        let data = CTAP1RequestAPDU::serialize(cmd, flags, &[])?;
        Ok((data, ()))
    }

    fn send_to_virtual_device<Dev: VirtualFidoDevice>(
        &self,
        dev: &mut Dev,
    ) -> Result<Self::Output, HIDError> {
        dev.get_version(self)
    }
}

#[cfg(test)]
pub mod tests {
    use crate::consts::{Capability, HIDCmd, CID_BROADCAST, SW_NO_ERROR};
    use crate::transport::device_selector::Device;
    use crate::transport::{hid::HIDDevice, FidoDevice, FidoProtocol};
    use rand::{thread_rng, RngCore};

    #[test]
    fn test_get_version_ctap1_only() {
        let mut device = Device::new("commands/get_version").unwrap();
        device.downgrade_to_ctap1();
        assert_eq!(device.get_protocol(), FidoProtocol::CTAP1);
        let nonce = [0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01];

        // channel id
        let mut cid = [0u8; 4];
        thread_rng().fill_bytes(&mut cid);

        // init packet
        let mut msg = CID_BROADCAST.to_vec();
        msg.extend([HIDCmd::Init.into(), 0x00, 0x08]); // cmd + bcnt
        msg.extend_from_slice(&nonce);
        device.add_write(&msg, 0);

        // init_resp packet
        let mut msg = CID_BROADCAST.to_vec();
        msg.extend(vec![
            0x06, /* HIDCmd::Init without !TYPE_INIT */
            0x00, 0x11,
        ]); // cmd + bcnt
        msg.extend_from_slice(&nonce);
        msg.extend_from_slice(&cid); // new channel id

        // We are not setting CBOR, to signal that the device does not support CTAP1
        msg.extend([0x02, 0x04, 0x01, 0x08, 0x01]); // versions + flags (wink)
        device.add_read(&msg, 0);

        // ctap1 U2F_VERSION request
        let mut msg = cid.to_vec();
        msg.extend([HIDCmd::Msg.into(), 0x0, 0x7]); // cmd + bcnt
        msg.extend([0x0, 0x3, 0x0, 0x0, 0x0, 0x0, 0x0]);
        device.add_write(&msg, 0);

        // fido response
        let mut msg = cid.to_vec();
        msg.extend([HIDCmd::Msg.into(), 0x0, 0x08]); // cmd + bcnt
        msg.extend([0x55, 0x32, 0x46, 0x5f, 0x56, 0x32]); // 'U2F_V2'
        msg.extend(SW_NO_ERROR);
        device.add_read(&msg, 0);

        device.init().expect("Failed to init device");

        assert_eq!(device.get_cid(), &cid);

        let dev_info = device.get_device_info();
        assert_eq!(dev_info.cap_flags, Capability::WINK);

        let result = device.get_authenticator_info();
        assert!(result.is_none());
    }
}
