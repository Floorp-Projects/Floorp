use ::ConnectError;
use ::{MidiInputConnection, MidiOutputConnection};

// TODO: maybe move to module `virtual` instead of `os::unix`?

/// Trait that is implemented by `MidiInput` on platforms that
/// support virtual ports (currently every platform but Windows).
pub trait VirtualInput<T: Send> where Self: Sized {
    /// Creates a virtual input port. Once it has been created,
    /// other applications can connect to this port and send MIDI
    /// messages which will be received by this port.
    fn create_virtual<F>(
        self, port_name: &str, callback: F, data: T
    ) -> Result<MidiInputConnection<T>, ConnectError<Self>>
    where F: FnMut(u64, &[u8], &mut T) + Send + 'static;
}

/// Trait that is implemented by `MidiOutput` on platforms that
/// support virtual ports (currently every platform but Windows).
pub trait VirtualOutput where Self: Sized {
    /// Creates a virtual output port. Once it has been created,
    /// other applications can connect to this port and will
    /// receive MIDI messages that are sent to this port.
    fn create_virtual(
        self, port_name: &str
    ) -> Result<MidiOutputConnection, ConnectError<Self>>;
}