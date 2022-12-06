use super::{u2ftypes, Pin, PinError};
use serde::ser::{Serialize, SerializeStruct};
use std::sync::mpsc::Sender;

#[derive(Debug)]
pub enum StatusUpdate {
    /// Device found
    DeviceAvailable { dev_info: u2ftypes::U2FDeviceInfo },
    /// Device got removed
    DeviceUnavailable { dev_info: u2ftypes::U2FDeviceInfo },
    /// We successfully finished the register or sign request
    Success { dev_info: u2ftypes::U2FDeviceInfo },
    /// Sent if a PIN is needed (or was wrong), or some other kind of PIN-related
    /// error occurred. The Sender is for sending back a PIN (if needed).
    PinError(PinError, Sender<Pin>),
    /// Sent, if multiple devices are found and the user has to select one
    SelectDeviceNotice,
    /// Sent, once a device was selected (either automatically or by user-interaction)
    /// and the register or signing process continues with this device
    DeviceSelected(u2ftypes::U2FDeviceInfo),
}

impl Serialize for StatusUpdate {
    fn serialize<S>(&self, serializer: S) -> std::result::Result<S::Ok, S::Error>
    where
        S: serde::Serializer,
    {
        let mut map = serializer.serialize_struct("StatusUpdate", 1)?;
        match &*self {
            StatusUpdate::DeviceAvailable { dev_info } => {
                map.serialize_field("DeviceAvailable", &dev_info)?
            }
            StatusUpdate::DeviceUnavailable { dev_info } => {
                map.serialize_field("DeviceUnavailable", &dev_info)?
            }
            StatusUpdate::Success { dev_info } => map.serialize_field("Success", &dev_info)?,
            StatusUpdate::PinError(e, _) => map.serialize_field("PinError", &e)?,
            StatusUpdate::SelectDeviceNotice => map.serialize_field("SelectDeviceNotice", &())?,
            StatusUpdate::DeviceSelected(dev_info) => {
                map.serialize_field("DeviceSelected", &dev_info)?
            }
        }
        map.end()
    }
}

pub(crate) fn send_status(status: &Sender<StatusUpdate>, msg: StatusUpdate) {
    match status.send(msg) {
        Ok(_) => {}
        Err(e) => error!("Couldn't send status: {:?}", e),
    };
}

#[cfg(test)]
pub mod tests {
    use crate::consts::U2F_AUTHENTICATE;

    use super::*;
    use crate::consts::Capability;
    use serde_json::to_string;
    use std::sync::mpsc::channel;

    #[test]
    fn serialize_select() {
        let st = StatusUpdate::SelectDeviceNotice;
        let json = to_string(&st).expect("Failed to serialize");
        assert_eq!(&json, r#"{"SelectDeviceNotice":null}"#);
    }

    #[test]
    fn serialize_invalid_pin() {
        let (tx, _rx) = channel();
        let st = StatusUpdate::PinError(PinError::InvalidPin(Some(3)), tx.clone());
        let json = to_string(&st).expect("Failed to serialize");
        assert_eq!(&json, r#"{"PinError":{"InvalidPin":3}}"#);

        let st = StatusUpdate::PinError(PinError::InvalidPin(None), tx);
        let json = to_string(&st).expect("Failed to serialize");
        assert_eq!(&json, r#"{"PinError":{"InvalidPin":null}}"#);
    }

    #[test]
    fn serialize_success() {
        let cap = Capability::WINK | Capability::CBOR;
        let dev = u2ftypes::U2FDeviceInfo {
            vendor_name: String::from("ABC").into_bytes(),
            device_name: String::from("DEF").into_bytes(),
            version_interface: 2,
            version_major: 5,
            version_minor: 4,
            version_build: 3,
            cap_flags: cap,
        };
        let st = StatusUpdate::Success { dev_info: dev };
        let json = to_string(&st).expect("Failed to serialize");
        assert_eq!(
            &json,
            r#"{"Success":{"vendor_name":[65,66,67],"device_name":[68,69,70],"version_interface":2,"version_major":5,"version_minor":4,"version_build":3,"cap_flags":{"bits":5}}}"#
        );
    }
}
