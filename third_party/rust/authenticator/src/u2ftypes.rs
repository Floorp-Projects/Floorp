/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::consts::*;
use crate::transport::hid::HIDDevice;
use crate::util::io_err;
use serde::Serialize;
use std::{cmp, fmt, io, str};

pub fn to_hex(data: &[u8], joiner: &str) -> String {
    let parts: Vec<String> = data.iter().map(|byte| format!("{byte:02x}")).collect();
    parts.join(joiner)
}

pub fn trace_hex(data: &[u8]) {
    if log_enabled!(log::Level::Trace) {
        trace!("USB send: {}", to_hex(data, ""));
    }
}

// Init structure for U2F Communications. Tells the receiver what channel
// communication is happening on, what command is running, and how much data to
// expect to receive over all.
//
// Spec at https://fidoalliance.org/specs/fido-u2f-v1.
// 0-nfc-bt-amendment-20150514/fido-u2f-hid-protocol.html#message--and-packet-structure
pub struct U2FHIDInit {}

impl U2FHIDInit {
    pub fn read<T: HIDDevice>(dev: &mut T) -> io::Result<(HIDCmd, Vec<u8>)> {
        let mut frame = vec![0u8; dev.in_rpt_size()];
        let mut count = dev.read(&mut frame)?;

        while dev.get_cid() != &frame[..4] {
            count = dev.read(&mut frame)?;
        }

        if count != dev.in_rpt_size() {
            return Err(io_err("invalid init packet"));
        }

        let cmd = HIDCmd::from(frame[4] | TYPE_INIT);

        let cap = (frame[5] as usize) << 8 | (frame[6] as usize);
        let mut data = Vec::with_capacity(cap);

        let len = if dev.in_rpt_size() >= INIT_HEADER_SIZE {
            cmp::min(cap, dev.in_rpt_size() - INIT_HEADER_SIZE)
        } else {
            cap
        };
        data.extend_from_slice(&frame[7..7 + len]);

        Ok((cmd, data))
    }

    pub fn write<T: HIDDevice>(dev: &mut T, cmd: u8, data: &[u8]) -> io::Result<usize> {
        if data.len() > 0xffff {
            return Err(io_err("payload length > 2^16"));
        }

        let mut frame = vec![0u8; dev.out_rpt_size() + 1];
        frame[1..5].copy_from_slice(dev.get_cid());
        frame[5] = cmd;
        frame[6] = (data.len() >> 8) as u8;
        frame[7] = data.len() as u8;

        let count = if dev.out_rpt_size() >= INIT_HEADER_SIZE {
            cmp::min(data.len(), dev.out_rpt_size() - INIT_HEADER_SIZE)
        } else {
            data.len()
        };
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
    pub fn read<T: HIDDevice>(dev: &mut T, seq: u8, max: usize) -> io::Result<Vec<u8>> {
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

        let max = if dev.in_rpt_size() >= CONT_HEADER_SIZE {
            cmp::min(max, dev.in_rpt_size() - CONT_HEADER_SIZE)
        } else {
            max
        };
        Ok(frame[5..5 + max].to_vec())
    }

    pub fn write<T: HIDDevice>(dev: &mut T, seq: u8, data: &[u8]) -> io::Result<usize> {
        let mut frame = vec![0u8; dev.out_rpt_size() + 1];
        frame[1..5].copy_from_slice(dev.get_cid());
        frame[5] = seq;

        let count = if dev.out_rpt_size() >= CONT_HEADER_SIZE {
            cmp::min(data.len(), dev.out_rpt_size() - CONT_HEADER_SIZE)
        } else {
            data.len()
        };
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
    pub cap_flags: Capability,
}

impl U2FHIDInitResp {
    pub fn read(data: &[u8], nonce: &[u8]) -> io::Result<U2FHIDInitResp> {
        assert_eq!(nonce.len(), INIT_NONCE_SIZE);

        if data.len() < INIT_NONCE_SIZE + 9 {
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
            cap_flags: Capability::from_bits_truncate(data[INIT_NONCE_SIZE + 8]),
        };

        Ok(rsp)
    }
}

/// CTAP1 (FIDO v1.x / U2F / "APDU-like") request framing format, used for
/// communication with authenticators over *all* transports.
///
/// This implementation follows the [FIDO v1.2 spec][fido12rawf], but only
/// implements extended APDUs (supported by USB HID, NFC and BLE transports).
///
/// # Technical details
///
/// FIDO v1.0 U2F framing [claims][fido10rawf] to be based on
/// [ISO/IEC 7816-4:2005][iso7816] (smart card) APDUs, but has several
/// differences, errors and omissions which make it incompatible.
///
/// FIDO v1.1 and v1.2 fixed *most* of these issues, but as a result is *not*
/// fully compatible with the FIDO v1.0 specification:
///
/// * FIDO v1.0 *only* defines extended APDUs, though
///   [v1.0 NFC implementors][fido10nfc] need to also handle short APDUs.
///
///   FIDO v1.1 and later define *both* short and extended APDUs, but defers to
///   transport-level guidance about which to use (where extended APDU support
///   is mandatory for all transports, and short APDU support is only mandatory
///   for NFC transports).
///
/// * FIDO v1.0 doesn't special-case N<sub>c</sub> (command data length) = 0
///   (ie: L<sub>c</sub> is *always* present).
///
/// * FIDO v1.0 declares extended L<sub>c</sub> as a 24-bit integer, rather than
///   16-bit with padding byte.
///
/// * FIDO v1.0 omits L<sub>e</sub> bytes entirely,
///   [except for short APDUs over NFC][fido10nfc].
///
/// Unfortunately, FIDO v2.x gives ambiguous compatibility guidance:
///
/// * [The FIDO v2.0 spec describes framing][fido20u2f] in
///   [FIDO v1.0 U2F Raw Message Format][fido10rawf], [cites][fido20u2fcite] the
///   FIDO v1.0 format by *name*, but actually links to the
///   [FIDO v1.2 format][fido12rawf].
///
/// * [The FIDO v2.1 spec also describes framing][fido21u2f] in
///   [FIDO v1.0 U2F Raw Message Format][fido10rawf], but [cites][fido21u2fcite]
///   the [FIDO v1.2 U2F Raw Message Format][fido12rawf] as a reference by name
///   and URL.
///
/// [fido10nfc]: https://fidoalliance.org/specs/fido-u2f-v1.0-nfc-bt-amendment-20150514/fido-u2f-nfc-protocol.html#framing
/// [fido10raw]: https://fidoalliance.org/specs/fido-u2f-v1.0-nfc-bt-amendment-20150514/fido-u2f-raw-message-formats.html
/// [fido10rawf]: https://fidoalliance.org/specs/fido-u2f-v1.0-nfc-bt-amendment-20150514/fido-u2f-raw-message-formats.html#u2f-message-framing
/// [fido12rawf]: https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#u2f-message-framing
/// [fido20u2f]: https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#u2f-framing
/// [fido20u2fcite]: https://fidoalliance.org/specs/fido-v2.0-ps-20190130/fido-client-to-authenticator-protocol-v2.0-ps-20190130.html#biblio-u2frawmsgs
/// [fido21u2f]: https://fidoalliance.org/specs/fido-v2.1-ps-20210615/fido-client-to-authenticator-protocol-v2.1-ps-errata-20220621.html#u2f-framing
/// [fido21u2fcite]: https://fidoalliance.org/specs/fido-v2.1-ps-20210615/fido-client-to-authenticator-protocol-v2.1-ps-errata-20220621.html#biblio-u2frawmsgs
/// [iso7816]: https://www.iso.org/standard/36134.html
pub struct CTAP1RequestAPDU {}

impl CTAP1RequestAPDU {
    /// Serializes a CTAP command into
    /// [FIDO v1.2 U2F Raw Message Format][fido12raw]. See
    /// [the struct documentation][Self] for implementation notes.
    ///
    /// # Arguments
    ///
    /// * `ins`: U2F command code, as documented in
    ///   [FIDO v1.2 U2F Raw Format][fido12cmd].
    /// * `p1`: Command parameter 1 / control byte.
    /// * `data`: Request data, as documented in
    ///   [FIDO v1.2 Raw Message Formats][fido12raw], of up to 65535 bytes.
    ///
    /// [fido12cmd]: https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html#command-and-parameter-values
    /// [fido12raw]: https://fidoalliance.org/specs/fido-u2f-v1.2-ps-20170411/fido-u2f-raw-message-formats-v1.2-ps-20170411.html
    pub fn serialize(ins: u8, p1: u8, data: &[u8]) -> io::Result<Vec<u8>> {
        if data.len() > 0xffff {
            return Err(io_err("payload length > 2^16"));
        }
        // Size of header + data.
        let data_size = if data.is_empty() { 0 } else { 2 + data.len() };
        let mut bytes = vec![0u8; U2FAPDUHEADER_SIZE + data_size];

        // bytes[0] (CLA): Always 0 in FIDO v1.x.
        bytes[1] = ins;
        bytes[2] = p1;
        // bytes[3] (P2): Always 0 in FIDO v1.x.

        // bytes[4] (Lc1/Le1): Always 0 for extended APDUs.
        if !data.is_empty() {
            bytes[5] = (data.len() >> 8) as u8; // Lc2
            bytes[6] = data.len() as u8; // Lc3

            bytes[7..7 + data.len()].copy_from_slice(data);
        }

        // Last two bytes (Le): Always 0 for Ne = 65536
        Ok(bytes)
    }
}

#[derive(Clone, Debug, Serialize)]
pub struct U2FDeviceInfo {
    pub vendor_name: Vec<u8>,
    pub device_name: Vec<u8>,
    pub version_interface: u8,
    pub version_major: u8,
    pub version_minor: u8,
    pub version_build: u8,
    pub cap_flags: Capability,
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
            to_hex(&[self.cap_flags.bits()], ":"),
        )
    }
}

////////////////////////////////////////////////////////////////////////
// Tests
////////////////////////////////////////////////////////////////////////

#[cfg(test)]
pub(crate) mod tests {
    use super::CTAP1RequestAPDU;

    #[test]
    fn test_ctap1_serialize() {
        // Command with no data, Lc should be omitted.
        assert_eq!(
            vec![0, 1, 2, 0, 0, 0, 0],
            CTAP1RequestAPDU::serialize(1, 2, &[]).unwrap()
        );

        // Command with data, Lc should be included.
        assert_eq!(
            vec![0, 1, 2, 0, 0, 0, 1, 42, 0, 0],
            CTAP1RequestAPDU::serialize(1, 2, &[42]).unwrap()
        );

        // Command with 300 bytes data, longer Lc.
        let d = [0xFF; 300];
        let mut expected = vec![0, 1, 2, 0, 0, 0x1, 0x2c];
        expected.extend_from_slice(&d);
        expected.extend_from_slice(&[0, 0]); // Lc
        assert_eq!(expected, CTAP1RequestAPDU::serialize(1, 2, &d).unwrap());

        // Command with 64k of data should error
        let big = [0xFF; 65536];
        assert!(CTAP1RequestAPDU::serialize(1, 2, &big).is_err());
    }
}
