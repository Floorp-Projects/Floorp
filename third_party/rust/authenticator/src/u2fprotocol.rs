/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#![cfg_attr(feature = "cargo-clippy", allow(clippy::needless_lifetimes))]

extern crate std;

use rand::{thread_rng, RngCore};
use std::ffi::CString;
use std::io;
use std::io::{Read, Write};

use crate::consts::*;
use crate::u2ftypes::*;
use crate::util::io_err;

////////////////////////////////////////////////////////////////////////
// Device Commands
////////////////////////////////////////////////////////////////////////

pub fn u2f_init_device<T>(dev: &mut T) -> bool
where
    T: U2FDevice + Read + Write,
{
    let mut nonce = [0u8; 8];
    thread_rng().fill_bytes(&mut nonce);

    // Initialize the device and check its version.
    init_device(dev, &nonce).is_ok() && is_v2_device(dev).unwrap_or(false)
}

pub fn u2f_register<T>(dev: &mut T, challenge: &[u8], application: &[u8]) -> io::Result<Vec<u8>>
where
    T: U2FDevice + Read + Write,
{
    if challenge.len() != PARAMETER_SIZE || application.len() != PARAMETER_SIZE {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "Invalid parameter sizes",
        ));
    }

    let mut register_data = Vec::with_capacity(2 * PARAMETER_SIZE);
    register_data.extend(challenge);
    register_data.extend(application);

    let flags = U2F_REQUEST_USER_PRESENCE;
    let (resp, status) = send_ctap1(dev, U2F_REGISTER, flags, &register_data)?;
    status_word_to_result(status, resp)
}

pub fn u2f_sign<T>(
    dev: &mut T,
    challenge: &[u8],
    application: &[u8],
    key_handle: &[u8],
) -> io::Result<Vec<u8>>
where
    T: U2FDevice + Read + Write,
{
    if challenge.len() != PARAMETER_SIZE || application.len() != PARAMETER_SIZE {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "Invalid parameter sizes",
        ));
    }

    if key_handle.len() > 256 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "Key handle too large",
        ));
    }

    let mut sign_data = Vec::with_capacity(2 * PARAMETER_SIZE + 1 + key_handle.len());
    sign_data.extend(challenge);
    sign_data.extend(application);
    sign_data.push(key_handle.len() as u8);
    sign_data.extend(key_handle);

    let flags = U2F_REQUEST_USER_PRESENCE;
    let (resp, status) = send_ctap1(dev, U2F_AUTHENTICATE, flags, &sign_data)?;
    status_word_to_result(status, resp)
}

pub fn u2f_is_keyhandle_valid<T>(
    dev: &mut T,
    challenge: &[u8],
    application: &[u8],
    key_handle: &[u8],
) -> io::Result<bool>
where
    T: U2FDevice + Read + Write,
{
    if challenge.len() != PARAMETER_SIZE || application.len() != PARAMETER_SIZE {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "Invalid parameter sizes",
        ));
    }

    if key_handle.len() >= 256 {
        return Err(io::Error::new(
            io::ErrorKind::InvalidInput,
            "Key handle too large",
        ));
    }

    let mut sign_data = Vec::with_capacity(2 * PARAMETER_SIZE + 1 + key_handle.len());
    sign_data.extend(challenge);
    sign_data.extend(application);
    sign_data.push(key_handle.len() as u8);
    sign_data.extend(key_handle);

    let flags = U2F_CHECK_IS_REGISTERED;
    let (_, status) = send_ctap1(dev, U2F_AUTHENTICATE, flags, &sign_data)?;
    Ok(status == SW_CONDITIONS_NOT_SATISFIED)
}

////////////////////////////////////////////////////////////////////////
// Internal Device Commands
////////////////////////////////////////////////////////////////////////

fn init_device<T>(dev: &mut T, nonce: &[u8]) -> io::Result<()>
where
    T: U2FDevice + Read + Write,
{
    assert_eq!(nonce.len(), INIT_NONCE_SIZE);
    // Send Init to broadcast address to create a new channel
    let raw = sendrecv(dev, HIDCmd::Init, nonce)?;
    let rsp = U2FHIDInitResp::read(&raw, nonce)?;
    // Get the new Channel ID
    dev.set_cid(rsp.cid);

    let vendor = dev
        .get_property("Manufacturer")
        .unwrap_or_else(|_| String::from("Unknown Vendor"));
    let product = dev
        .get_property("Product")
        .unwrap_or_else(|_| String::from("Unknown Device"));

    dev.set_device_info(U2FDeviceInfo {
        vendor_name: vendor.as_bytes().to_vec(),
        device_name: product.as_bytes().to_vec(),
        version_interface: rsp.version_interface,
        version_major: rsp.version_major,
        version_minor: rsp.version_minor,
        version_build: rsp.version_build,
        cap_flags: rsp.cap_flags,
    });

    Ok(())
}

fn is_v2_device<T>(dev: &mut T) -> io::Result<bool>
where
    T: U2FDevice + Read + Write,
{
    let (data, status) = send_ctap1(dev, U2F_VERSION, 0x00, &[])?;
    let actual = CString::new(data)?;
    let expected = CString::new("U2F_V2")?;
    status_word_to_result(status, actual == expected)
}

////////////////////////////////////////////////////////////////////////
// Error Handling
////////////////////////////////////////////////////////////////////////

fn status_word_to_result<T>(status: [u8; 2], val: T) -> io::Result<T> {
    use self::io::ErrorKind::{InvalidData, InvalidInput};

    match status {
        SW_NO_ERROR => Ok(val),
        SW_WRONG_DATA => Err(io::Error::new(InvalidData, "wrong data")),
        SW_WRONG_LENGTH => Err(io::Error::new(InvalidInput, "wrong length")),
        SW_CONDITIONS_NOT_SATISFIED => Err(io_err("conditions not satisfied")),
        _ => Err(io_err(&format!("failed with status {status:?}"))),
    }
}

////////////////////////////////////////////////////////////////////////
// Device Communication Functions
////////////////////////////////////////////////////////////////////////

pub fn sendrecv<T>(dev: &mut T, cmd: HIDCmd, send: &[u8]) -> io::Result<Vec<u8>>
where
    T: U2FDevice + Read + Write,
{
    // Send initialization packet.
    let mut count = U2FHIDInit::write(dev, cmd.into(), send)?;

    // Send continuation packets.
    let mut sequence = 0u8;
    while count < send.len() {
        count += U2FHIDCont::write(dev, sequence, &send[count..])?;
        sequence += 1;
    }

    // Now we read. This happens in 2 chunks: The initial packet, which has the
    // size we expect overall, then continuation packets, which will fill in
    // data until we have everything.
    let (_, mut data) = U2FHIDInit::read(dev)?;

    let mut sequence = 0u8;
    while data.len() < data.capacity() {
        let max = data.capacity() - data.len();
        data.extend_from_slice(&U2FHIDCont::read(dev, sequence, max)?);
        sequence += 1;
    }

    Ok(data)
}

fn send_ctap1<T>(dev: &mut T, cmd: u8, p1: u8, send: &[u8]) -> io::Result<(Vec<u8>, [u8; 2])>
where
    T: U2FDevice + Read + Write,
{
    let apdu = CTAP1RequestAPDU::serialize(cmd, p1, send)?;
    let mut data = sendrecv(dev, HIDCmd::Msg, &apdu)?;

    if data.len() < 2 {
        return Err(io_err("unexpected response"));
    }

    let split_at = data.len() - 2;
    let status = data.split_off(split_at);
    Ok((data, [status[0], status[1]]))
}

////////////////////////////////////////////////////////////////////////
// Tests
////////////////////////////////////////////////////////////////////////

#[cfg(test)]
pub(crate) mod tests {
    use super::{init_device, is_v2_device, send_ctap1, sendrecv, U2FDevice};
    use crate::consts::{Capability, HIDCmd, CID_BROADCAST, SW_NO_ERROR};
    use crate::transport::device_selector::Device;
    use crate::transport::hid::HIDDevice;
    use crate::u2ftypes::U2FDeviceInfo;
    use rand::{thread_rng, RngCore};

    #[test]
    fn test_init_device() {
        let mut device = Device::new("u2fprotocol").unwrap();
        let nonce = vec![0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01];

        // channel id
        let mut cid = [0u8; 4];
        thread_rng().fill_bytes(&mut cid);

        // init packet
        let mut msg = CID_BROADCAST.to_vec();
        msg.extend(vec![HIDCmd::Init.into(), 0x00, 0x08]); // cmd + bcnt
        msg.extend_from_slice(&nonce);
        device.add_write(&msg, 0);

        // init_resp packet
        let mut msg = CID_BROADCAST.to_vec();
        msg.extend(vec![HIDCmd::Init.into(), 0x00, 0x11]); // cmd + bcnt
        msg.extend_from_slice(&nonce);
        msg.extend_from_slice(&cid); // new channel id
        msg.extend(vec![0x02, 0x04, 0x01, 0x08, 0x01]); // versions + flags
        device.add_read(&msg, 0);

        init_device(&mut device, &nonce).unwrap();
        assert_eq!(device.get_cid(), &cid);

        let dev_info = device.get_device_info();
        assert_eq!(dev_info.version_interface, 0x02);
        assert_eq!(dev_info.version_major, 0x04);
        assert_eq!(dev_info.version_minor, 0x01);
        assert_eq!(dev_info.version_build, 0x08);
        assert_eq!(dev_info.cap_flags, Capability::WINK); // 0x01
    }

    #[test]
    fn test_get_version() {
        let mut device = Device::new("u2fprotocol").unwrap();
        // channel id
        let mut cid = [0u8; 4];
        thread_rng().fill_bytes(&mut cid);

        device.set_cid(cid);

        let info = U2FDeviceInfo {
            vendor_name: Vec::new(),
            device_name: Vec::new(),
            version_interface: 0x02,
            version_major: 0x04,
            version_minor: 0x01,
            version_build: 0x08,
            cap_flags: Capability::WINK,
        };
        device.set_device_info(info);

        // ctap1.0 U2F_VERSION request
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

        let res = is_v2_device(&mut device).expect("Failed to get version");
        assert!(res);
    }

    #[test]
    fn test_sendrecv_multiple() {
        let mut device = Device::new("u2fprotocol").unwrap();
        let cid = [0x01, 0x02, 0x03, 0x04];
        device.set_cid(cid);

        // init packet
        let mut msg = cid.to_vec();
        msg.extend(vec![HIDCmd::Ping.into(), 0x00, 0xe4]); // cmd + length = 228
                                                           // write msg, append [1u8; 57], 171 bytes remain
        device.add_write(&msg, 1);
        device.add_read(&msg, 1);

        // cont packet
        let mut msg = cid.to_vec();
        msg.push(0x00); // seq = 0
                        // write msg, append [1u8; 59], 112 bytes remaining
        device.add_write(&msg, 1);
        device.add_read(&msg, 1);

        // cont packet
        let mut msg = cid.to_vec();
        msg.push(0x01); // seq = 1
                        // write msg, append [1u8; 59], 53 bytes remaining
        device.add_write(&msg, 1);
        device.add_read(&msg, 1);

        // cont packet
        let mut msg = cid.to_vec();
        msg.push(0x02); // seq = 2
        msg.extend_from_slice(&[1u8; 53]);
        // write msg, append remaining 53 bytes.
        device.add_write(&msg, 0);
        device.add_read(&msg, 0);

        let data = [1u8; 228];
        let d = sendrecv(&mut device, HIDCmd::Ping, &data).unwrap();
        assert_eq!(d.len(), 228);
        assert_eq!(d, &data[..]);
    }

    #[test]
    fn test_sendapdu() {
        let cid = [0x01, 0x02, 0x03, 0x04];
        let data = [0x01, 0x02, 0x03, 0x04, 0x05];
        let mut device = Device::new("u2fprotocol").unwrap();
        device.set_cid(cid);

        let mut msg = cid.to_vec();
        // sendrecv header
        msg.extend(vec![HIDCmd::Msg.into(), 0x00, 0x0e]); // len = 14
                                                          // apdu header
        msg.extend(vec![
            0x00,
            HIDCmd::Ping.into(),
            0xaa,
            0x00,
            0x00,
            0x00,
            0x05,
        ]);
        // apdu data
        msg.extend_from_slice(&data);
        device.add_write(&msg, 0);

        // Send data back
        let mut msg = cid.to_vec();
        msg.extend(vec![HIDCmd::Msg.into(), 0x00, 0x07]);
        msg.extend_from_slice(&data);
        msg.extend_from_slice(&SW_NO_ERROR);
        device.add_read(&msg, 0);

        let (result, status) = send_ctap1(&mut device, HIDCmd::Ping.into(), 0xaa, &data).unwrap();
        assert_eq!(result, &data);
        assert_eq!(status, SW_NO_ERROR);
    }

    #[test]
    fn test_get_property() {
        let device = Device::new("u2fprotocol").unwrap();

        assert_eq!(device.get_property("a").unwrap(), "a not implemented");
    }
}
