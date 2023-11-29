use crate::transport::hid::HIDDevice;

pub use crate::transport::platform::device::Device;

use runloop::RunLoop;
use std::collections::{HashMap, HashSet};
use std::sync::mpsc::{channel, RecvTimeoutError, Sender};
use std::time::Duration;

pub type DeviceID = <Device as HIDDevice>::Id;
pub type DeviceBuildParameters = <Device as HIDDevice>::BuildParameters;

trait DeviceSelectorEventMarker {}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum BlinkResult {
    DeviceSelected,
    Cancelled,
}

#[derive(Debug, Clone, Copy, PartialEq, Eq)]
pub enum DeviceCommand {
    Blink,
    Cancel,
    Continue,
    Removed,
}

#[derive(Debug)]
pub enum DeviceSelectorEvent {
    Cancel,
    Timeout,
    DevicesAdded(Vec<DeviceID>),
    DeviceRemoved(DeviceID),
    NotAToken(DeviceID),
    ImAToken((DeviceID, Sender<DeviceCommand>)),
    SelectedToken(DeviceID),
}

pub struct DeviceSelector {
    /// How to send a message to the event loop
    sender: Sender<DeviceSelectorEvent>,
    /// Thread of the event loop
    runloop: RunLoop,
}

impl DeviceSelector {
    pub fn run() -> Self {
        let (selector_send, selector_rec) = channel();
        // let new_device_callback = Arc::new(new_device_cb);
        let runloop = RunLoop::new(move |alive| {
            let mut blinking = false;
            // Device was added, but we wait for its response, if it is a token or not
            // We save both a write-only copy of the device (for cancellation) and it's thread
            let mut waiting_for_response = HashSet::new();
            // Device IDs of devices that responded with "ImAToken" mapping to channels that are
            // waiting to receive a DeviceCommand
            let mut tokens = HashMap::new();
            while alive() {
                let d = Duration::from_secs(100);
                let res = match selector_rec.recv_timeout(d) {
                    Err(RecvTimeoutError::Disconnected) => {
                        break;
                    }
                    Err(RecvTimeoutError::Timeout) => DeviceSelectorEvent::Timeout,
                    Ok(res) => res,
                };

                match res {
                    DeviceSelectorEvent::Timeout | DeviceSelectorEvent::Cancel => {
                        /* TODO */
                        Self::cancel_all(tokens, None);
                        break;
                    }
                    DeviceSelectorEvent::SelectedToken(ref id) => {
                        Self::cancel_all(tokens, Some(id));
                        break; // We are done here. The selected device continues without us.
                    }
                    DeviceSelectorEvent::DevicesAdded(ids) => {
                        for id in ids {
                            debug!("Device added event: {:?}", id);
                            waiting_for_response.insert(id);
                        }
                        continue;
                    }
                    DeviceSelectorEvent::DeviceRemoved(ref id) => {
                        debug!("Device removed event: {:?}", id);
                        if !waiting_for_response.remove(id) {
                            // Note: We _could_ check here if we had multiple tokens and are already blinking
                            //       and the removal of this one leads to only one token left. So we could in theory
                            //       stop blinking and select it right away. At the moment, I think this is a
                            //       too surprising behavior and therefore, we let the remaining device keep on blinking
                            //       since the user could add yet another device, instead of using the remaining one.
                            tokens.iter().for_each(|(dev_id, tx)| {
                                if dev_id == id {
                                    let _ = tx.send(DeviceCommand::Removed);
                                }
                            });
                            tokens.retain(|dev_id, _| dev_id != id);
                            if tokens.is_empty() {
                                blinking = false;
                                continue;
                            }
                        }
                        // We are already blinking, so no need to run the code below this match
                        // that figures out if we should blink or not. In fact, currently, we do
                        // NOT want to run this code again, because if you have 2 blinking tokens
                        // and one got removed, we WANT the remaining one to continue blinking.
                        // This is a design choice, because I currently think it is the "less surprising"
                        // option to the user.
                        if blinking {
                            continue;
                        }
                    }
                    DeviceSelectorEvent::NotAToken(ref id) => {
                        debug!("Device not a token event: {:?}", id);
                        waiting_for_response.remove(id);
                    }
                    DeviceSelectorEvent::ImAToken((id, tx)) => {
                        let _ = waiting_for_response.remove(&id);
                        if blinking {
                            // We are already blinking, so this new device should blink too.
                            if tx.send(DeviceCommand::Blink).is_ok() {
                                tokens.insert(id, tx.clone());
                            }
                            continue;
                        } else {
                            tokens.insert(id, tx.clone());
                        }
                    }
                }

                // All known devices told us, whether they are tokens or not and we have at least one token
                if waiting_for_response.is_empty() && !tokens.is_empty() {
                    if tokens.len() == 1 {
                        let (dev_id, tx) = tokens.drain().next().unwrap(); // We just checked that it can't be empty
                        if tx.send(DeviceCommand::Continue).is_err() {
                            // Device thread died in the meantime (which shouldn't happen).
                            // Tokens is empty, so we just start over again
                            continue;
                        }
                        Self::cancel_all(tokens, Some(&dev_id));
                        break; // We are done here
                    } else {
                        blinking = true;

                        tokens.iter().for_each(|(_dev, tx)| {
                            // A send operation can only fail if the receiving end of a channel is disconnected, implying that the data could never be received.
                            // We ignore errors here for now, but should probably remove the device in such a case (even though it theoretically can't happen)
                            let _ = tx.send(DeviceCommand::Blink);
                        });
                    }
                }
            }
        });
        Self {
            runloop: runloop.unwrap(), // TODO
            sender: selector_send,
        }
    }

    pub fn clone_sender(&self) -> Sender<DeviceSelectorEvent> {
        self.sender.clone()
    }

    fn cancel_all(tokens: HashMap<DeviceID, Sender<DeviceCommand>>, exclude: Option<&DeviceID>) {
        for (dev_id, tx) in tokens.iter() {
            if Some(dev_id) != exclude {
                let _ = tx.send(DeviceCommand::Cancel);
            }
        }
    }

    pub fn stop(&mut self) {
        // We ignore a possible error here, since we don't really care
        let _ = self.sender.send(DeviceSelectorEvent::Cancel);
        self.runloop.cancel();
    }
}

#[cfg(test)]
pub mod tests {
    use super::*;
    use crate::{
        consts::Capability,
        ctap2::commands::get_info::{AuthenticatorInfo, AuthenticatorOptions},
        transport::FidoDevice,
        u2ftypes::U2FDeviceInfo,
    };

    pub(crate) fn gen_info(id: String) -> U2FDeviceInfo {
        U2FDeviceInfo {
            vendor_name: String::from("ExampleVendor").into_bytes(),
            device_name: id.into_bytes(),
            version_interface: 1,
            version_major: 3,
            version_minor: 2,
            version_build: 1,
            cap_flags: Capability::WINK | Capability::CBOR | Capability::NMSG,
        }
    }

    pub(crate) fn make_device_simple_u2f(dev: &mut Device) {
        dev.set_device_info(gen_info(dev.id()));
        dev.set_cid([1, 2, 3, 4]); // Need to set something other than broadcast
        dev.downgrade_to_ctap1();
        dev.create_channel();
    }

    pub(crate) fn make_device_with_pin(dev: &mut Device) {
        dev.set_device_info(gen_info(dev.id()));
        dev.set_cid([1, 2, 3, 4]); // Need to set something other than broadcast
        dev.create_channel();
        let info = AuthenticatorInfo {
            options: AuthenticatorOptions {
                client_pin: Some(true),
                ..Default::default()
            },
            ..Default::default()
        };
        dev.set_authenticator_info(info);
    }

    fn send_i_am_token(dev: &Device, selector: &DeviceSelector) {
        selector
            .sender
            .send(DeviceSelectorEvent::ImAToken((
                dev.id(),
                dev.sender.clone().unwrap(),
            )))
            .unwrap();
    }

    fn send_no_token(dev: &Device, selector: &DeviceSelector) {
        selector
            .sender
            .send(DeviceSelectorEvent::NotAToken(dev.id()))
            .unwrap()
    }

    fn remove_device(dev: &Device, selector: &DeviceSelector) {
        selector
            .sender
            .send(DeviceSelectorEvent::DeviceRemoved(dev.id()))
            .unwrap();
        assert_eq!(
            dev.receiver.as_ref().unwrap().recv().unwrap(),
            DeviceCommand::Removed
        );
    }

    fn add_devices<'a, T>(iter: T, selector: &DeviceSelector)
    where
        T: Iterator<Item = &'a Device>,
    {
        selector
            .sender
            .send(DeviceSelectorEvent::DevicesAdded(
                iter.map(|f| f.id()).collect(),
            ))
            .unwrap();
    }

    #[test]
    fn test_device_selector_one_token_no_late_adds() {
        let mut devices = vec![
            Device::new("device selector 1").unwrap(),
            Device::new("device selector 2").unwrap(),
            Device::new("device selector 3").unwrap(),
            Device::new("device selector 4").unwrap(),
        ];

        // Make those actual tokens. The rest is interpreted as non-u2f-devices
        make_device_with_pin(&mut devices[2]);
        let selector = DeviceSelector::run();

        // Adding all
        add_devices(devices.iter(), &selector);
        devices.iter_mut().for_each(|d| {
            if !d.is_u2f() {
                send_no_token(d, &selector);
            }
        });

        send_i_am_token(&devices[2], &selector);

        assert_eq!(
            devices[2].receiver.as_ref().unwrap().recv().unwrap(),
            DeviceCommand::Continue
        );
    }

    // This test is mostly for testing stop() and clone_sender()
    #[test]
    fn test_device_selector_stop() {
        let device = Device::new("device selector 1").unwrap();

        let mut selector = DeviceSelector::run();

        // Adding all
        selector
            .clone_sender()
            .send(DeviceSelectorEvent::DevicesAdded(vec![device.id()]))
            .unwrap();

        selector
            .clone_sender()
            .send(DeviceSelectorEvent::NotAToken(device.id()))
            .unwrap();
        selector.stop();
    }

    #[test]
    fn test_device_selector_all_pins_with_late_add() {
        let mut devices = vec![
            Device::new("device selector 1").unwrap(),
            Device::new("device selector 2").unwrap(),
            Device::new("device selector 3").unwrap(),
            Device::new("device selector 4").unwrap(),
            Device::new("device selector 5").unwrap(),
            Device::new("device selector 6").unwrap(),
        ];

        // Make those actual tokens. The rest is interpreted as non-u2f-devices
        make_device_with_pin(&mut devices[2]);
        make_device_with_pin(&mut devices[4]);
        make_device_with_pin(&mut devices[5]);

        let selector = DeviceSelector::run();

        // Adding all, except the last one (we simulate that this one is not yet plugged in)
        add_devices(devices.iter().take(5), &selector);

        // Interleave tokens and non-tokens
        send_i_am_token(&devices[2], &selector);

        devices.iter_mut().for_each(|d| {
            if !d.is_u2f() {
                send_no_token(d, &selector);
            }
        });

        send_i_am_token(&devices[4], &selector);

        // We added 2 devices that are tokens. They should get the blink-command now
        assert_eq!(
            devices[2].receiver.as_ref().unwrap().recv().unwrap(),
            DeviceCommand::Blink
        );
        assert_eq!(
            devices[4].receiver.as_ref().unwrap().recv().unwrap(),
            DeviceCommand::Blink
        );

        // Plug in late device
        send_i_am_token(&devices[5], &selector);
        assert_eq!(
            devices[5].receiver.as_ref().unwrap().recv().unwrap(),
            DeviceCommand::Blink
        );
    }

    #[test]
    fn test_device_selector_no_pins_late_mixed_adds() {
        // Multiple tokes, none of them support a PIN
        let mut devices = vec![
            Device::new("device selector 1").unwrap(),
            Device::new("device selector 2").unwrap(),
            Device::new("device selector 3").unwrap(),
            Device::new("device selector 4").unwrap(),
            Device::new("device selector 5").unwrap(),
            Device::new("device selector 6").unwrap(),
            Device::new("device selector 7").unwrap(),
        ];

        // Make those actual tokens. The rest is interpreted as non-u2f-devices
        make_device_simple_u2f(&mut devices[2]);
        make_device_simple_u2f(&mut devices[4]);
        make_device_simple_u2f(&mut devices[5]);

        let selector = DeviceSelector::run();

        // Adding all, except the last one (we simulate that this one is not yet plugged in)
        add_devices(devices.iter().take(5), &selector);

        // Interleave tokens and non-tokens
        send_i_am_token(&devices[2], &selector);

        devices.iter_mut().for_each(|d| {
            if !d.is_u2f() {
                send_no_token(d, &selector);
            }
        });

        send_i_am_token(&devices[4], &selector);

        // We added 2 devices that are tokens. They should get the blink-command now
        assert_eq!(
            devices[2].receiver.as_ref().unwrap().recv().unwrap(),
            DeviceCommand::Blink
        );
        assert_eq!(
            devices[4].receiver.as_ref().unwrap().recv().unwrap(),
            DeviceCommand::Blink
        );

        // Plug in late device
        send_i_am_token(&devices[5], &selector);
        assert_eq!(
            devices[5].receiver.as_ref().unwrap().recv().unwrap(),
            DeviceCommand::Blink
        );
        // Remove device again
        remove_device(&devices[5], &selector);

        // Now we add a token that has a PIN, it should not get "Continue" but "Blink"
        make_device_with_pin(&mut devices[6]);
        send_i_am_token(&devices[6], &selector);
        assert_eq!(
            devices[6].receiver.as_ref().unwrap().recv().unwrap(),
            DeviceCommand::Blink
        );
    }

    #[test]
    fn test_device_selector_mixed_pins_remove_all() {
        // Multiple tokes, none of them support a PIN, so we should get Continue-commands
        // for all of them
        let mut devices = vec![
            Device::new("device selector 1").unwrap(),
            Device::new("device selector 2").unwrap(),
            Device::new("device selector 3").unwrap(),
            Device::new("device selector 4").unwrap(),
            Device::new("device selector 5").unwrap(),
            Device::new("device selector 6").unwrap(),
        ];

        // Make those actual tokens. The rest is interpreted as non-u2f-devices
        make_device_with_pin(&mut devices[2]);
        make_device_with_pin(&mut devices[4]);
        make_device_with_pin(&mut devices[5]);

        let selector = DeviceSelector::run();

        // Adding all, except the last one (we simulate that this one is not yet plugged in)
        add_devices(devices.iter(), &selector);

        devices.iter_mut().for_each(|d| {
            if d.is_u2f() {
                send_i_am_token(d, &selector);
            } else {
                send_no_token(d, &selector);
            }
        });

        for idx in [2, 4, 5] {
            assert_eq!(
                devices[idx].receiver.as_ref().unwrap().recv().unwrap(),
                DeviceCommand::Blink
            );
        }

        // Remove all tokens
        for idx in [2, 4, 5] {
            remove_device(&devices[idx], &selector);
        }

        // Adding one again
        send_i_am_token(&devices[4], &selector);

        // This should now get a "Continue" instead of "Blinking", because it's the only device
        assert_eq!(
            devices[4].receiver.as_ref().unwrap().recv().unwrap(),
            DeviceCommand::Continue
        );
    }
}
