/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use std::{cmp, fmt, io, str};

use crate::consts::*;
use crate::util::io_err;

pub fn to_hex(data: &[u8], joiner: &str) -> String {
    let parts: Vec<String> = data.iter().map(|byte| format!("{:02x}", byte)).collect();
    parts.join(joiner)
}

pub fn trace_hex(data: &[u8]) {
    if log_enabled!(log::Level::Trace) {
        trace!("USB send: {}", to_hex(data, ""));
    }
}

// Trait for representing U2F HID Devices. Requires getters/setters for the
// channel ID, created during device initialization.
pub trait U2FDevice {
    fn get_cid(&self) -> &[u8; 4];
    fn set_cid(&mut self, cid: [u8; 4]);

    fn in_rpt_size(&self) -> usize;
    fn in_init_data_size(&self) -> usize {
        self.in_rpt_size() - INIT_HEADER_SIZE
    }
    fn in_cont_data_size(&self) -> usize {
        self.in_rpt_size() - CONT_HEADER_SIZE
    }

    fn out_rpt_size(&self) -> usize;
    fn out_init_data_size(&self) -> usize {
        self.out_rpt_size() - INIT_HEADER_SIZE
    }
    fn out_cont_data_size(&self) -> usize {
        self.out_rpt_size() - CONT_HEADER_SIZE
    }

    fn get_property(&self, prop_name: &str) -> io::Result<String>;
    fn get_device_info(&self) -> U2FDeviceInfo;
    fn set_device_info(&mut self, dev_info: U2FDeviceInfo);
}

// Init structure for U2F Communications. Tells the receiver what channel
// communication is happening on, what command is running, and how much data to
// expect to receive over all.
//
// Spec at https://fidoalliance.org/specs/fido-u2f-v1.
// 0-nfc-bt-amendment-20150514/fido-u2f-hid-protocol.html#message--and-packet-structure
pub struct U2FHIDInit {}

impl U2FHIDInit {
    pub fn read<T>(dev: &mut T) -> io::Result<Vec<u8>>
    where
        T: U2FDevice + io::Read,
    {
        let mut frame = vec![0u8; dev.in_rpt_size()];
        let mut count = dev.read(&mut frame)?;

        while dev.get_cid() != &frame[..4] {
            count = dev.read(&mut frame)?;
        }

        if count != dev.in_rpt_size() {
            return Err(io_err("invalid init packet"));
        }

        let cap = (frame[5] as usize) << 8 | (frame[6] as usize);
        let mut data = Vec::with_capacity(cap);

        let len = cmp::min(cap, dev.in_init_data_size());
        data.extend_from_slice(&frame[7..7 + len]);

        Ok(data)
    }

    pub fn write<T>(dev: &mut T, cmd: u8, data: &[u8]) -> io::Result<usize>
    where
        T: U2FDevice + io::Write,
    {
        if data.len() > 0xffff {
            return Err(io_err("payload length > 2^16"));
        }

        let mut frame = vec![0u8; dev.out_rpt_size() + 1];
        frame[1..5].copy_from_slice(dev.get_cid());
        frame[5] = cmd;
        frame[6] = (data.len() >> 8) as u8;
        frame[7] = data.len() as u8;

        let count = cmp::min(data.len(), dev.out_init_data_size());
        frame[8..8 + count].copy_from_slice(&data[..count]);
        trace_hex(&frame);

        if dev.write(&frame)? != frame.len() {
            return Err(io_err("device write failed"));
        }

        Ok(count)
    }
}

// Continuation structure for U2F Communications. After an Init structure is
// sent, continuation structures are used to transmit all extra data that
// wouldn't fit in the initial packet. The sequence number increases with every
// packet, until all data is received.
//
// https://fidoalliance.org/specs/fido-u2f-v1.0-nfc-bt-amendment-20150514/fido-u2f-hid-protocol.
// html#message--and-packet-structure
pub struct U2FHIDCont {}

impl U2FHIDCont {
    pub fn read<T>(dev: &mut T, seq: u8, max: usize) -> io::Result<Vec<u8>>
    where
        T: U2FDevice + io::Read,
    {
        let mut frame = vec![0u8; dev.in_rpt_size()];
        let mut count = dev.read(&mut frame)?;

        while dev.get_cid() != &frame[..4] {
            count = dev.read(&mut frame)?;
        }

        if count != dev.in_rpt_size() {
            return Err(io_err("invalid cont packet"));
        }

        if seq != frame[4] {
            return Err(io_err("invalid sequence number"));
        }

        let max = cmp::min(max, dev.in_cont_data_size());
        Ok(frame[5..5 + max].to_vec())
    }

    pub fn write<T>(dev: &mut T, seq: u8, data: &[u8]) -> io::Result<usize>
    where
        T: U2FDevice + io::Write,
    {
        let mut frame = vec![0u8; dev.out_rpt_size() + 1];
        frame[1..5].copy_from_slice(dev.get_cid());
        frame[5] = seq;

        let count = cmp::min(data.len(), dev.out_cont_data_size());
        frame[6..6 + count].copy_from_slice(&data[..count]);
        trace_hex(&frame);

        if dev.write(&frame)? != frame.len() {
            return Err(io_err("device write failed"));
        }

        Ok(count)
    }
}

// Reply sent after initialization command. Contains information about U2F USB
// Key versioning, as well as the communication channel to be used for all
// further requests.
//
// https://fidoalliance.org/specs/fido-u2f-v1.0-nfc-bt-amendment-20150514/fido-u2f-hid-protocol.
// html#u2fhid_init
pub struct U2FHIDInitResp {
    pub cid: [u8; 4],
    pub version_interface: u8,
    pub version_major: u8,
    pub version_minor: u8,
    pub version_build: u8,
    pub cap_flags: u8,
}

impl U2FHIDInitResp {
    pub fn read(data: &[u8], nonce: &[u8]) -> io::Result<U2FHIDInitResp> {
        assert_eq!(nonce.len(), INIT_NONCE_SIZE);

        if data.len() != INIT_NONCE_SIZE + 9 {
            return Err(io_err("invalid init response"));
        }

        if nonce != &data[..INIT_NONCE_SIZE] {
            return Err(io_err("invalid nonce"));
        }

        let rsp = U2FHIDInitResp {
            cid: [
                data[INIT_NONCE_SIZE],
                data[INIT_NONCE_SIZE + 1],
                data[INIT_NONCE_SIZE + 2],
                data[INIT_NONCE_SIZE + 3],
            ],
            version_interface: data[INIT_NONCE_SIZE + 4],
            version_major: data[INIT_NONCE_SIZE + 5],
            version_minor: data[INIT_NONCE_SIZE + 6],
            version_build: data[INIT_NONCE_SIZE + 7],
            cap_flags: data[INIT_NONCE_SIZE + 8],
        };

        Ok(rsp)
    }
}

// https://en.wikipedia.org/wiki/Smart_card_application_protocol_data_unit
// https://fidoalliance.org/specs/fido-u2f-v1.
// 0-nfc-bt-amendment-20150514/fido-u2f-raw-message-formats.html#u2f-message-framing
pub struct U2FAPDUHeader {}

impl U2FAPDUHeader {
    pub fn serialize(ins: u8, p1: u8, data: &[u8]) -> io::Result<Vec<u8>> {
        if data.len() > 0xffff {
            return Err(io_err("payload length > 2^16"));
        }

        // Size of header + data + 2 zero bytes for maximum return size.
        let mut bytes = vec![0u8; U2FAPDUHEADER_SIZE + data.len() + 2];
        // cla is always 0 for our requirements
        bytes[1] = ins;
        bytes[2] = p1;
        // p2 is always 0, at least, for our requirements.
        // lc[0] should always be 0.
        bytes[5] = (data.len() >> 8) as u8;
        bytes[6] = data.len() as u8;
        bytes[7..7 + data.len()].copy_from_slice(data);

        Ok(bytes)
    }
}

#[derive(Clone, Debug)]
pub struct U2FDeviceInfo {
    pub vendor_name: Vec<u8>,
    pub device_name: Vec<u8>,
    pub version_interface: u8,
    pub version_major: u8,
    pub version_minor: u8,
    pub version_build: u8,
    pub cap_flags: u8,
}

impl fmt::Display for U2FDeviceInfo {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        write!(
            f,
            "Vendor: {}, Device: {}, Interface: {}, Firmware: v{}.{}.{}, Capabilities: {}",
            str::from_utf8(&self.vendor_name).unwrap(),
            str::from_utf8(&self.device_name).unwrap(),
            &self.version_interface,
            &self.version_major,
            &self.version_minor,
            &self.version_build,
            to_hex(&[self.cap_flags], ":"),
        )
    }
}
