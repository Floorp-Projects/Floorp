extern crate coremidi;

use std::sync::{Arc, Mutex};

use ::errors::*;
use ::Ignore;
use ::MidiMessage;

use self::coremidi::*;

mod external {
    #[link(name = "CoreAudio", kind = "framework")]
    extern "C" {
        pub fn AudioConvertHostTimeToNanos(inHostTime: u64) -> u64;
        pub fn AudioGetCurrentHostTime() -> u64;
    }
}

pub struct MidiInput {
    client: Client,
    ignore_flags: Ignore
}

#[derive(Clone)]
pub struct MidiInputPort {
    source: Arc<Source>
}

impl PartialEq for MidiInputPort {
    fn eq(&self, other: &Self) -> bool {
        if let (Some(id1), Some(id2)) = (self.source.unique_id(), other.source.unique_id()) {
            id1 == id2
        } else {
            // Acording to macos docs "The system assigns unique IDs to all objects.", so I think we can ignore this case
            false
        }
    }
}

impl MidiInput {
    pub fn new(client_name: &str) -> Result<Self, InitError> {
        match Client::new(client_name) {
            Ok(cl) => Ok(MidiInput { client: cl, ignore_flags: Ignore::None }),
            Err(_) => Err(InitError)
        }
    }

    pub(crate) fn ports_internal(&self) -> Vec<::common::MidiInputPort> {
        Sources.into_iter().map(|s| ::common::MidiInputPort {
            imp: MidiInputPort { source: Arc::new(s) }
        }).collect()
    }

    pub fn ignore(&mut self, flags: Ignore) {
        self.ignore_flags = flags;
    }
    
    pub fn port_count(&self) -> usize {
        Sources::count()
    }
    
    pub fn port_name(&self, port: &MidiInputPort) -> Result<String, PortInfoError> {
        match port.source.display_name() {
            Some(name) => Ok(name),
            None => Err(PortInfoError::CannotRetrievePortName)
        }
    }

    fn handle_input<T>(packets: &PacketList, handler_data: &mut HandlerData<T>) {
        let continue_sysex =  &mut handler_data.continue_sysex;
        let ignore = handler_data.ignore_flags;
        let message = &mut handler_data.message;
        let data = &mut handler_data.user_data.as_mut().unwrap();
        for p in packets.iter() {
            let pdata = p.data();
            if pdata.len() == 0 { continue; }

            let mut timestamp = p.timestamp();
            if timestamp == 0 { // this might happen for asnychronous sysex messages (?)
                timestamp = unsafe { external::AudioGetCurrentHostTime() };
            }

            if !*continue_sysex {
                message.timestamp = unsafe { external::AudioConvertHostTimeToNanos(timestamp) } as u64 / 1000;
            }

            let mut cur_byte = 0;
            if *continue_sysex {
                // We have a continuing, segmented sysex message.
                if !ignore.contains(Ignore::Sysex) {
                    // If we're not ignoring sysex messages, copy the entire packet.
                    message.bytes.extend_from_slice(pdata);
                }
                *continue_sysex = pdata[pdata.len() - 1] != 0xF7;

                if !ignore.contains(Ignore::Sysex) && !*continue_sysex {
                    // If we reached the end of the sysex, invoke the user callback
                    (handler_data.callback)(message.timestamp, &message.bytes, data);
                    message.bytes.clear();
                }
            } else {
                while cur_byte < pdata.len() {
                    // We are expecting that the next byte in the packet is a status byte.
                    let status = pdata[cur_byte];
                    if status & 0x80 == 0 { break; }
                    // Determine the number of bytes in the MIDI message.
                    let size;
                    if status < 0xC0 { size = 3; }
                    else if status < 0xE0 { size = 2; }
                    else if status < 0xF0 { size = 3; }
                    else if status == 0xF0 {
                        // A MIDI sysex
                        if ignore.contains(Ignore::Sysex) {
                            size = 0;
                            cur_byte = pdata.len();
                        } else {
                            size = pdata.len() - cur_byte;
                        }
                        *continue_sysex = pdata[pdata.len() - 1] != 0xF7;
                    }
                    else if status == 0xF1 {
                        // A MIDI time code message
                        if ignore.contains(Ignore::Time) {
                            size = 0;
                            cur_byte += 2;
                        } else {
                            size = 2;
                        }
                    }
                    else if status == 0xF2 { size = 3; }
                    else if status == 0xF3 { size = 2; }
                    else if status == 0xF8 && ignore.contains(Ignore::Time) {
                        // A MIDI timing tick message and we're ignoring it.
                        size = 0;
                        cur_byte += 1;
                    }
                    else if status == 0xFE && ignore.contains(Ignore::ActiveSense) {
                        // A MIDI active sensing message and we're ignoring it.
                        size = 0;
                        cur_byte += 1;
                    }
                    else { size = 1; }

                    // Copy the MIDI data to our vector.
                    if size > 0 {
                        let message_bytes = &pdata[cur_byte..(cur_byte + size)];
                        if !*continue_sysex {
                            // This is either a non-sysex message or a non-segmented sysex message
                            (handler_data.callback)(message.timestamp, message_bytes, data);
                            message.bytes.clear();
                        } else {
                            // This is the beginning of a segmented sysex message
                            message.bytes.extend_from_slice(message_bytes);
                        }
                        cur_byte += size;
                    }
                }
            }
        }
    }
    
    pub fn connect<F, T: Send + 'static>(
        self, port: &MidiInputPort, port_name: &str, callback: F, data: T
    ) -> Result<MidiInputConnection<T>, ConnectError<MidiInput>>
        where F: FnMut(u64, &[u8], &mut T) + Send + 'static {
        let handler_data = Arc::new(Mutex::new(HandlerData {
            message: MidiMessage::new(),
            ignore_flags: self.ignore_flags,
            continue_sysex: false,
            callback: Box::new(callback),
            user_data: Some(data)
        }));
        let handler_data2 = handler_data.clone();
        let iport = match self.client.input_port(port_name, move |packets| {
            MidiInput::handle_input(packets, &mut *handler_data2.lock().unwrap())
        }) {
            Ok(p) => p,
            Err(_) => return Err(ConnectError::other("error creating MIDI input port", self))
        };
        if let Err(_) = iport.connect_source(&port.source) {
            return Err(ConnectError::other("error connecting MIDI input port", self));
        }
        Ok(MidiInputConnection {
            client: self.client,
            details: InputConnectionDetails::Explicit(iport),
            handler_data: handler_data
        })
    }

    pub fn create_virtual<F, T: Send + 'static>(
        self, port_name: &str, callback: F, data: T
    ) -> Result<MidiInputConnection<T>, ConnectError<MidiInput>>
    where F: FnMut(u64, &[u8], &mut T) + Send + 'static {

        let handler_data = Arc::new(Mutex::new(HandlerData {
            message: MidiMessage::new(),
            ignore_flags: self.ignore_flags,
            continue_sysex: false,
            callback: Box::new(callback),
            user_data: Some(data)
        }));
        let handler_data2 = handler_data.clone();
        let vrt = match self.client.virtual_destination(port_name, move |packets| {
            MidiInput::handle_input(packets, &mut *handler_data2.lock().unwrap())
        }) {
            Ok(p) => p,
            Err(_) => return Err(ConnectError::other("error creating MIDI input port", self))
        };
        Ok(MidiInputConnection {
            client: self.client,
            details: InputConnectionDetails::Virtual(vrt),
            handler_data: handler_data
        })
    }
}

enum InputConnectionDetails {
    Explicit(InputPort),
    Virtual(VirtualDestination)
}

pub struct MidiInputConnection<T> {
    client: Client,
    #[allow(dead_code)]
    details: InputConnectionDetails,
    // TODO: get rid of Arc & Mutex?
    //       synchronization is required because the borrow checker does not
    //       know that the callback we're in here is never called concurrently
    //       (always in sequence)
    handler_data: Arc<Mutex<HandlerData<T>>>
}

impl<T> MidiInputConnection<T> {
    pub fn close(self) -> (MidiInput, T) {
        let mut handler_data_locked = self.handler_data.lock().unwrap();
        (MidiInput {
            client: self.client,
            ignore_flags: handler_data_locked.ignore_flags
        }, handler_data_locked.user_data.take().unwrap())
    }
}

/// This is all the data that is stored on the heap as long as a connection
/// is opened and passed to the callback handler.
///
/// It is important that `user_data` is the last field to not influence
/// offsets after monomorphization.
struct HandlerData<T> {
    message: MidiMessage,
    ignore_flags: Ignore,
    continue_sysex: bool,
    callback: Box<dyn FnMut(u64, &[u8], &mut T) + Send>,
    user_data: Option<T>
}

pub struct MidiOutput {
    client: Client
}

#[derive(Clone)]
pub struct MidiOutputPort {
    dest: Arc<Destination>
}

impl PartialEq for MidiOutputPort {
    fn eq(&self, other: &Self) -> bool {
        if let (Some(id1), Some(id2)) = (self.dest.unique_id(), other.dest.unique_id()) {
            id1 == id2
        } else {
            // Acording to macos docs "The system assigns unique IDs to all objects.", so I think we can ignore this case
            false
        }
    }
}

impl MidiOutput {
    pub fn new(client_name: &str) -> Result<Self, InitError> {
        match Client::new(client_name) {
            Ok(cl) => Ok(MidiOutput { client: cl }),
            Err(_) => Err(InitError)
        }
    }

    pub(crate) fn ports_internal(&self) -> Vec<::common::MidiOutputPort> {
        Destinations.into_iter().map(|d| ::common::MidiOutputPort {
            imp: MidiOutputPort { dest: Arc::new(d) }
        }).collect()
    }
    
    pub fn port_count(&self) -> usize {
        Destinations::count()
    }
    
    pub fn port_name(&self, port: &MidiOutputPort) -> Result<String, PortInfoError> {
        match port.dest.display_name() {
            Some(name) => Ok(name),
            None => Err(PortInfoError::CannotRetrievePortName)
        }
    }
    
    pub fn connect(self, port: &MidiOutputPort, port_name: &str) -> Result<MidiOutputConnection, ConnectError<MidiOutput>> {
        let oport = match self.client.output_port(port_name) {
            Ok(p) => p,
            Err(_) => return Err(ConnectError::other("error creating MIDI output port", self))
        };
        Ok(MidiOutputConnection {
            client: self.client,
            details: OutputConnectionDetails::Explicit(oport, port.dest.clone())
        })
    }

    pub fn create_virtual(self, port_name: &str) -> Result<MidiOutputConnection, ConnectError<MidiOutput>> {
        let vrt = match self.client.virtual_source(port_name) {
            Ok(p) => p,
            Err(_) => return Err(ConnectError::other("error creating virtual MIDI source", self))
        };
        Ok(MidiOutputConnection {
            client: self.client,
            details: OutputConnectionDetails::Virtual(vrt)
        })
    }
}

enum OutputConnectionDetails {
    Explicit(OutputPort, Arc<Destination>),
    Virtual(VirtualSource)
}

pub struct MidiOutputConnection {
    client: Client,
    details: OutputConnectionDetails
}

impl MidiOutputConnection {
    pub fn close(self) -> MidiOutput {
        MidiOutput { client: self.client }
    }
    
    pub fn send(&mut self, message: &[u8]) -> Result<(), SendError> {
        let packets = PacketBuffer::new(0, message);
        match self.details {
            OutputConnectionDetails::Explicit(ref port, ref dest) => {
                port.send(&dest, &packets).map_err(|_| SendError::Other("error sending MIDI message to port"))
            },
            OutputConnectionDetails::Virtual(ref vrt) => {
                vrt.received(&packets).map_err(|_| SendError::Other("error sending MIDI to virtual destinations"))
            }
        }
        
    }
}
