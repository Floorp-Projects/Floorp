use crate::consts::{HIDCmd, CID_BROADCAST};

use crate::ctap2::commands::{CommandError, RequestCtap1, RequestCtap2, Retryable, StatusCode};
use crate::transport::errors::{ApduErrorStatus, HIDError};
use crate::transport::{FidoDevice, FidoDeviceIO, FidoProtocol};
use crate::u2ftypes::{U2FDeviceInfo, U2FHIDCont, U2FHIDInit, U2FHIDInitResp};
use crate::util::io_err;
use rand::{thread_rng, RngCore};
use std::cmp::Eq;
use std::fmt;
use std::hash::Hash;
use std::io;
use std::io::{Read, Write};
use std::thread;
use std::time::Duration;

pub trait HIDDevice: FidoDevice + Read + Write {
    type BuildParameters: Sized;
    type Id: fmt::Debug + PartialEq + Eq + Hash + Sized;

    // Open device, verify that it is indeed a CTAP device and potentially read initial values
    fn new(parameters: Self::BuildParameters) -> Result<Self, (HIDError, Self::Id)>;
    fn id(&self) -> Self::Id;

    fn get_device_info(&self) -> U2FDeviceInfo;
    fn set_device_info(&mut self, dev_info: U2FDeviceInfo);

    // Channel ID management
    fn get_cid(&self) -> &[u8; 4];
    fn set_cid(&mut self, cid: [u8; 4]);

    // HID report sizes
    fn in_rpt_size(&self) -> usize;
    fn out_rpt_size(&self) -> usize;

    fn get_property(&self, prop_name: &str) -> io::Result<String>;

    // Initialize on a protocol-level
    fn pre_init(&mut self) -> Result<(), HIDError> {
        if self.initialized() {
            return Ok(());
        }

        let mut nonce = [0u8; 8];
        thread_rng().fill_bytes(&mut nonce);

        // Send Init to broadcast address to create a new channel
        self.set_cid(CID_BROADCAST);
        let (cmd, raw) = HIDDevice::sendrecv(self, HIDCmd::Init, &nonce, &|| true)?;
        if cmd != HIDCmd::Init {
            return Err(HIDError::DeviceError);
        }

        let rsp = U2FHIDInitResp::read(&raw, &nonce)?;
        // Set the new Channel ID
        self.set_cid(rsp.cid);

        let vendor = self
            .get_property("Manufacturer")
            .unwrap_or_else(|_| String::from("Unknown Vendor"));
        let product = self
            .get_property("Product")
            .unwrap_or_else(|_| String::from("Unknown Device"));

        let info = U2FDeviceInfo {
            vendor_name: vendor.as_bytes().to_vec(),
            device_name: product.as_bytes().to_vec(),
            version_interface: rsp.version_interface,
            version_major: rsp.version_major,
            version_minor: rsp.version_minor,
            version_build: rsp.version_build,
            cap_flags: rsp.cap_flags,
        };
        debug!("{:?}: {:?}", self.id(), info);
        self.set_device_info(info);

        // A CTAPHID host SHALL accept a response size that is longer than the
        // anticipated size to allow for future extensions of the protocol, yet
        // maintaining backwards compatibility. Future versions will maintain
        // the response structure of the current version, but additional fields
        // may be added.

        Ok(())
    }

    fn sendrecv(
        &mut self,
        cmd: HIDCmd,
        send: &[u8],
        keep_alive: &dyn Fn() -> bool,
    ) -> io::Result<(HIDCmd, Vec<u8>)> {
        self.u2f_write(cmd.into(), send)?;
        debug!("sent to Device {:?} cmd={:?}: {:?}", self.id(), cmd, send);
        loop {
            let (cmd, data) = self.u2f_read()?;
            if cmd != HIDCmd::Keepalive {
                debug!(
                    "got from Device {:?} status={:?}: {:?}",
                    self.id(),
                    cmd,
                    data
                );
                return Ok((cmd, data));
            }
            // The authenticator might send us HIDCmd::Keepalive messages indefinitely, e.g. if
            // it's waiting for user presence. The keep_alive function is used to cancel the
            // transaction.
            if !keep_alive() {
                break;
            }
        }

        // If this is a CTAP2 device we can tell the authenticator to cancel the transaction on its
        // side as well. There's nothing to do for U2F/CTAP1 devices.
        if self.get_protocol() == FidoProtocol::CTAP2 {
            self.u2f_write(u8::from(HIDCmd::Cancel), &[])?;
        }
        // For CTAP2 devices we expect to read
        //  (HIDCmd::Cbor, [CTAP2_ERR_KEEPALIVE_CANCEL])
        // for U2F/CTAP1 we expect to read
        //  (HIDCmd::Keepalive, [status]).
        self.u2f_read()
    }

    fn u2f_write(&mut self, cmd: u8, send: &[u8]) -> io::Result<()> {
        let mut count = U2FHIDInit::write(self, cmd, send)?;

        // Send continuation packets.
        let mut sequence = 0u8;
        while count < send.len() {
            count += U2FHIDCont::write(self, sequence, &send[count..])?;
            sequence += 1;
        }

        Ok(())
    }

    fn u2f_read(&mut self) -> io::Result<(HIDCmd, Vec<u8>)> {
        // Now we read. This happens in 2 chunks: The initial packet, which has
        // the size we expect overall, then continuation packets, which will
        // fill in data until we have everything.
        let (cmd, data) = {
            let (cmd, mut data) = U2FHIDInit::read(self)?;

            trace!("init frame data read: {:04X?}", &data);
            let mut sequence = 0u8;
            while data.len() < data.capacity() {
                let max = data.capacity() - data.len();
                data.extend_from_slice(&U2FHIDCont::read(self, sequence, max)?);
                sequence += 1;
            }
            (cmd, data)
        };
        trace!("u2f_read({:?}) cmd={:?}: {:04X?}", self.id(), cmd, &data);
        Ok((cmd, data))
    }
}

impl<T: HIDDevice> FidoDeviceIO for T {
    fn send_msg_cancellable<Out, Req: RequestCtap1<Output = Out> + RequestCtap2<Output = Out>>(
        &mut self,
        msg: &Req,
        keep_alive: &dyn Fn() -> bool,
    ) -> Result<Out, HIDError> {
        if !self.initialized() {
            return Err(HIDError::DeviceNotInitialized);
        }

        match self.get_protocol() {
            FidoProtocol::CTAP1 => self.send_ctap1_cancellable(msg, keep_alive),
            FidoProtocol::CTAP2 => self.send_cbor_cancellable(msg, keep_alive),
        }
    }

    fn send_cbor_cancellable<Req: RequestCtap2>(
        &mut self,
        msg: &Req,
        keep_alive: &dyn Fn() -> bool,
    ) -> Result<Req::Output, HIDError> {
        debug!("sending {:?} to {:?}", msg, self);

        let mut data = msg.wire_format()?;
        let mut buf: Vec<u8> = Vec::with_capacity(data.len() + 1);
        // CTAP2 command
        buf.push(msg.command() as u8);
        // payload
        buf.append(&mut data);
        let buf = buf;

        let (cmd, resp) = self.sendrecv(HIDCmd::Cbor, &buf, keep_alive)?;
        if cmd == HIDCmd::Cbor {
            Ok(msg.handle_response_ctap2(self, &resp)?)
        } else {
            Err(HIDError::UnexpectedCmd(cmd.into()))
        }
    }

    fn send_ctap1_cancellable<Req: RequestCtap1>(
        &mut self,
        msg: &Req,
        keep_alive: &dyn Fn() -> bool,
    ) -> Result<Req::Output, HIDError> {
        debug!("sending {:?} to {:?}", msg, self);
        let (data, add_info) = msg.ctap1_format()?;

        while keep_alive() {
            // sendrecv will not block with a CTAP1 device
            let (cmd, mut data) = self.sendrecv(HIDCmd::Msg, &data, &|| true)?;
            if cmd == HIDCmd::Msg {
                if data.len() < 2 {
                    return Err(io_err("Unexpected Response: shorter than expected").into());
                }
                let split_at = data.len() - 2;
                let status = data.split_off(split_at);
                // This will bubble up error if status != no error
                let status = ApduErrorStatus::from([status[0], status[1]]);

                match msg.handle_response_ctap1(self, status, &data, &add_info) {
                    Ok(out) => return Ok(out),
                    Err(Retryable::Retry) => {
                        // sleep 100ms then loop again
                        // TODO(baloo): meh, use tokio instead?
                        thread::sleep(Duration::from_millis(100));
                    }
                    Err(Retryable::Error(e)) => return Err(e),
                }
            } else {
                return Err(HIDError::UnexpectedCmd(cmd.into()));
            }
        }

        Err(HIDError::Command(CommandError::StatusCode(
            StatusCode::KeepaliveCancel,
            None,
        )))
    }
}
