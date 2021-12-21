use std::ops::Deref;

use crate::object::Object;

/// A [MIDI object](https://developer.apple.com/reference/coremidi/midideviceref).
///
/// A MIDI device or external device, containing entities.
///
#[derive(Debug, PartialEq)]
pub struct Device {
    pub(crate) object: Object,
}

impl Deref for Device {
    type Target = Object;

    fn deref(&self) -> &Object {
        &self.object
    }
}
