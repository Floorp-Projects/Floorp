pub mod destinations;
pub mod sources;

use std::ops::Deref;

use core_foundation_sys::base::OSStatus;
use coremidi_sys::MIDIFlushOutput;

use crate::object::Object;

/// A MIDI source or source, owned by an entity.
/// See [MIDIEndpointRef](https://developer.apple.com/reference/coremidi/midiendpointref).
///
/// You don't need to create an endpoint directly, instead you can create system sources and sources or virtual ones from a client.
///
#[derive(Debug)]
pub struct Endpoint {
    pub(crate) object: Object,
}

impl Endpoint {
    /// Unschedules previously-sent packets.
    /// See [MIDIFlushOutput](https://developer.apple.com/reference/coremidi/1495312-midiflushoutput).
    ///
    pub fn flush(&self) -> Result<(), OSStatus> {
        let status = unsafe { MIDIFlushOutput(self.object.0) };
        if status == 0 {
            Ok(())
        } else {
            Err(status)
        }
    }
}

impl AsRef<Object> for Endpoint {
    fn as_ref(&self) -> &Object {
        &self.object
    }
}

impl Deref for Endpoint {
    type Target = Object;

    fn deref(&self) -> &Object {
        &self.object
    }
}
