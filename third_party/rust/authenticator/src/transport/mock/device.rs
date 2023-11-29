/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
use crate::consts::{Capability, HIDCmd, CID_BROADCAST};
use crate::crypto::SharedSecret;
use crate::ctap2::commands::get_info::AuthenticatorInfo;
use crate::ctap2::commands::{CtapResponse, RequestCtap1, RequestCtap2};
use crate::transport::device_selector::DeviceCommand;
use crate::transport::TestDevice;
use crate::transport::{hid::HIDDevice, FidoDevice, FidoProtocol, HIDError};
use crate::u2ftypes::{U2FDeviceInfo, U2FHIDInitResp};
use std::any::Any;
use std::collections::VecDeque;
use std::hash::{Hash, Hasher};
use std::io::{self, Read, Write};
use std::sync::mpsc::{channel, Receiver, Sender};

pub(crate) const IN_HID_RPT_SIZE: usize = 64;
const OUT_HID_RPT_SIZE: usize = 64;

#[derive(Debug)]
pub struct Device {
    pub id: String,
    pub cid: [u8; 4],
    pub reads: Vec<[u8; IN_HID_RPT_SIZE]>,
    pub writes: Vec<[u8; OUT_HID_RPT_SIZE + 1]>,
    pub dev_info: Option<U2FDeviceInfo>,
    pub authenticator_info: Option<AuthenticatorInfo>,
    pub sender: Option<Sender<DeviceCommand>>,
    pub receiver: Option<Receiver<DeviceCommand>>,
    pub protocol: FidoProtocol,
    skip_serialization: bool,
    pub upcoming_requests: VecDeque<Vec<u8>>,
    pub upcoming_responses: VecDeque<Result<Box<dyn Any>, HIDError>>,
}

impl Device {
    pub fn add_write(&mut self, packet: &[u8], fill_value: u8) {
        // Add one to deal with record index check
        let mut write = [fill_value; OUT_HID_RPT_SIZE + 1];
        // Make sure we start with a 0, for HID record index
        write[0] = 0;
        // Clone packet data in at 1, since front is padded with HID record index
        write[1..=packet.len()].clone_from_slice(packet);
        self.writes.push(write);
    }

    pub fn add_read(&mut self, packet: &[u8], fill_value: u8) {
        let mut read = [fill_value; IN_HID_RPT_SIZE];
        read[..packet.len()].clone_from_slice(packet);
        self.reads.push(read);
    }

    pub fn add_upcoming_ctap2_request(&mut self, msg: &impl RequestCtap2) {
        self.upcoming_requests
            .push_back(msg.wire_format().expect("Failed to serialize CTAP request"));
    }

    pub fn add_upcoming_ctap1_request(&mut self, msg: &impl RequestCtap1) {
        let (upcoming, _) = msg
            .ctap1_format()
            .expect("Failed to serialize CTAP request");
        self.upcoming_requests.push_back(upcoming);
    }

    pub fn add_upcoming_ctap_response(&mut self, msg: impl CtapResponse) {
        self.upcoming_responses.push_back(Ok(Box::new(msg)));
    }

    pub fn add_upcoming_ctap_error(&mut self, msg: HIDError) {
        self.upcoming_responses.push_back(Err(msg));
    }

    pub fn create_channel(&mut self) {
        let (tx, rx) = channel();
        self.sender = Some(tx);
        self.receiver = Some(rx);
    }

    pub fn new_skipping_serialization(id: &str) -> Result<Self, (HIDError, String)> {
        Ok(Device {
            id: id.to_string(),
            cid: CID_BROADCAST,
            reads: vec![],
            writes: vec![],
            dev_info: None,
            authenticator_info: None,
            sender: None,
            receiver: None,
            protocol: FidoProtocol::CTAP2,
            skip_serialization: true,
            upcoming_requests: VecDeque::new(),
            upcoming_responses: VecDeque::new(),
        })
    }
}

impl Write for Device {
    fn write(&mut self, bytes: &[u8]) -> io::Result<usize> {
        // Pop a vector from the expected writes, check for quality
        // against bytes array.
        assert!(
            !self.writes.is_empty(),
            "Ran out of expected write values! Wanted to write {:?}",
            bytes
        );
        let check = self.writes.remove(0);
        assert_eq!(check.len(), bytes.len());
        assert_eq!(&check, bytes);
        Ok(bytes.len())
    }

    // nop
    fn flush(&mut self) -> io::Result<()> {
        Ok(())
    }
}

impl Read for Device {
    fn read(&mut self, bytes: &mut [u8]) -> io::Result<usize> {
        assert!(!self.reads.is_empty(), "Ran out of read values!");
        let check = self.reads.remove(0);
        assert_eq!(check.len(), bytes.len());
        bytes.clone_from_slice(&check);
        Ok(check.len())
    }
}

impl Drop for Device {
    fn drop(&mut self) {
        if !std::thread::panicking() {
            assert!(self.reads.is_empty());
            assert!(self.writes.is_empty());
        }
    }
}

impl PartialEq for Device {
    fn eq(&self, other: &Device) -> bool {
        self.id == other.id
    }
}

impl Eq for Device {}

impl Hash for Device {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.id.hash(state);
    }
}

impl HIDDevice for Device {
    type Id = String;
    type BuildParameters = &'static str; // None used

    fn id(&self) -> Self::Id {
        self.id.clone()
    }

    fn new(id: Self::BuildParameters) -> Result<Self, (HIDError, Self::Id)> {
        Ok(Device {
            id: id.to_string(),
            cid: CID_BROADCAST,
            reads: vec![],
            writes: vec![],
            dev_info: None,
            authenticator_info: None,
            sender: None,
            receiver: None,
            protocol: FidoProtocol::CTAP2,
            skip_serialization: false,
            upcoming_requests: VecDeque::new(),
            upcoming_responses: VecDeque::new(),
        })
    }

    fn get_cid(&self) -> &[u8; 4] {
        &self.cid
    }

    fn set_cid(&mut self, cid: [u8; 4]) {
        self.cid = cid;
    }

    fn in_rpt_size(&self) -> usize {
        IN_HID_RPT_SIZE
    }

    fn out_rpt_size(&self) -> usize {
        OUT_HID_RPT_SIZE
    }

    fn get_property(&self, prop_name: &str) -> io::Result<String> {
        Ok(format!("{prop_name} not implemented"))
    }

    fn get_device_info(&self) -> U2FDeviceInfo {
        self.dev_info.clone().unwrap()
    }

    fn set_device_info(&mut self, dev_info: U2FDeviceInfo) {
        self.dev_info = Some(dev_info);
    }

    fn pre_init(&mut self) -> Result<(), HIDError> {
        if self.initialized() {
            return Ok(());
        }

        let nonce = [0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01];

        // Send Init to broadcast address to create a new channel
        self.set_cid(CID_BROADCAST);
        let (cmd, raw) = HIDDevice::sendrecv(self, HIDCmd::Init, &nonce, &|| true)?;
        if cmd != HIDCmd::Init {
            return Err(HIDError::DeviceError);
        }

        let rsp = U2FHIDInitResp::read(&raw, &nonce)?;

        // Set the new Channel ID
        self.set_cid(rsp.cid);

        let info = U2FDeviceInfo {
            vendor_name: "Test vendor".as_bytes().to_vec(),
            device_name: "Test device".as_bytes().to_vec(),
            version_interface: rsp.version_interface,
            version_major: rsp.version_major,
            version_minor: rsp.version_minor,
            version_build: rsp.version_build,
            cap_flags: rsp.cap_flags,
        };
        debug!("{:?}: {:?}", self.id(), info);
        self.set_device_info(info);

        Ok(())
    }
}

impl TestDevice for Device {
    fn skip_serialization(&self) -> bool {
        self.skip_serialization
    }

    fn send_ctap1_unserialized<Req: RequestCtap1>(
        &mut self,
        msg: &Req,
    ) -> Result<Req::Output, HIDError> {
        let expected = self
            .upcoming_requests
            .pop_front()
            .expect("No expected CTAP1 command left");
        let (incoming, _) = msg.ctap1_format().expect("Can't serialize CTAP1 request");
        assert_eq!(expected, incoming);
        let response = self
            .upcoming_responses
            .pop_front()
            .expect("No response given!");
        response.map(|x| {
            *x.downcast()
                .expect("Failed to downcast given CTAP response")
        })
    }

    fn send_ctap2_unserialized<Req: RequestCtap2>(
        &mut self,
        msg: &Req,
    ) -> Result<Req::Output, HIDError> {
        let expected = self
            .upcoming_requests
            .pop_front()
            .expect("No expected CTAP2 command left");
        let incoming = msg.wire_format().expect("Can't serialize CTAP2 request");
        assert_eq!(expected, incoming);
        let response = self
            .upcoming_responses
            .pop_front()
            .expect("No response given!");
        response.map(|x| {
            *x.downcast()
                .expect("Failed to downcast given CTAP response")
        })
    }
}

impl FidoDevice for Device {
    fn pre_init(&mut self) -> Result<(), HIDError> {
        HIDDevice::pre_init(self)
    }

    fn should_try_ctap2(&self) -> bool {
        HIDDevice::get_device_info(self)
            .cap_flags
            .contains(Capability::CBOR)
    }

    fn initialized(&self) -> bool {
        self.get_cid() != &CID_BROADCAST
    }

    fn is_u2f(&mut self) -> bool {
        self.sender.is_some()
    }

    fn get_shared_secret(&self) -> std::option::Option<&SharedSecret> {
        None
    }

    fn set_shared_secret(&mut self, _: SharedSecret) {
        // Nothing
    }

    fn get_authenticator_info(&self) -> Option<&AuthenticatorInfo> {
        self.authenticator_info.as_ref()
    }

    fn set_authenticator_info(&mut self, authenticator_info: AuthenticatorInfo) {
        self.authenticator_info = Some(authenticator_info);
    }

    fn get_protocol(&self) -> FidoProtocol {
        self.protocol
    }

    fn downgrade_to_ctap1(&mut self) {
        self.protocol = FidoProtocol::CTAP1;
    }
}
