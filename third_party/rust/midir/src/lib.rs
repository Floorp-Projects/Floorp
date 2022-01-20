extern crate memalloc;

#[cfg(feature = "jack")]
#[macro_use] extern crate bitflags;

#[repr(u8)]
#[derive(Debug, Clone, Copy, PartialEq, Eq)]
/// An enum that is used to specify what kind of MIDI messages should
/// be ignored when receiving messages.
pub enum Ignore {
    None = 0x00,
    Sysex = 0x01,
    Time = 0x02,
    SysexAndTime = 0x03,
    ActiveSense = 0x04,
    SysexAndActiveSense = 0x05,
    TimeAndActiveSense = 0x06,
    All = 0x07
}

impl std::ops::BitOr for Ignore {
    type Output = Ignore;
    #[inline(always)]
    fn bitor(self, rhs: Self) -> Self::Output {
        // this is safe because all combinations also exist as variants
        unsafe { std::mem::transmute(self as u8 | rhs as u8) }
    }
}

impl Ignore {
    #[inline(always)]
    pub fn contains(self, other: Ignore) -> bool {
        self as u8 & other as u8 != 0 
    }
}

/// A MIDI structure used internally by some backends to store incoming
/// messages. Each message represents one and only one MIDI message.
/// The timestamp is represented as the elapsed microseconds since
/// a point in time that is arbitrary, but does not change for the
/// lifetime of a given MidiInputConnection.
#[derive(Debug, Clone)]
struct MidiMessage {
    bytes: Vec<u8>,
    timestamp: u64
}

impl MidiMessage {
    fn new() -> MidiMessage {
        MidiMessage {
            bytes: vec![],
            timestamp: 0
        }
    }
}

pub mod os; // include platform-specific behaviour

mod errors;
pub use errors::*;

mod common;
pub use common::*;

mod backend;