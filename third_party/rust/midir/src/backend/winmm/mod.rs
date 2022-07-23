extern crate winapi;

use std::{mem, ptr, slice};
use std::ffi::OsString;
use std::os::windows::ffi::OsStringExt;
use std::sync::Mutex;
use std::io::{Write, stderr};
use std::thread::sleep;
use std::time::Duration;
use memalloc::{allocate, deallocate};
use std::mem::MaybeUninit;
use std::ptr::null_mut;

use self::winapi::shared::basetsd::{DWORD_PTR, UINT_PTR};
use self::winapi::shared::minwindef::{DWORD, UINT};

use self::winapi::um::mmeapi::{midiInAddBuffer, midiInClose, midiInGetDevCapsW, midiInGetNumDevs,
                               midiInOpen, midiInPrepareHeader, midiInReset, midiInStart,
                               midiInStop, midiInUnprepareHeader, midiOutClose,
                               midiOutGetDevCapsW, midiOutGetNumDevs, midiOutLongMsg, midiOutOpen,
                               midiOutPrepareHeader, midiOutReset, midiOutShortMsg,
                               midiOutUnprepareHeader};

use self::winapi::um::mmsystem::{CALLBACK_FUNCTION, CALLBACK_NULL, HMIDIIN, HMIDIOUT, LPMIDIHDR,
                                 MIDIERR_NOTREADY, MIDIERR_STILLPLAYING, MIDIHDR, MIDIINCAPSW,
                                 MIDIOUTCAPSW, MMSYSERR_BADDEVICEID, MMSYSERR_NOERROR, MMSYSERR_ALLOCATED};

use {Ignore, MidiMessage};
use errors::*;

mod handler;

const DRV_QUERYDEVICEINTERFACE: UINT = 0x80c;
const DRV_QUERYDEVICEINTERFACESIZE: UINT = 0x80d;

const RT_SYSEX_BUFFER_SIZE: usize = 1024;
const RT_SYSEX_BUFFER_COUNT: usize = 4;

// helper for string conversion
fn from_wide_ptr(ptr: *const u16, max_len: usize) -> OsString {
    unsafe {
        assert!(!ptr.is_null());
        let len = (0..max_len as isize).position(|i| *ptr.offset(i) == 0).unwrap();
        let slice = slice::from_raw_parts(ptr, len);
        OsString::from_wide(slice)
    }
}

#[derive(Debug)]
pub struct MidiInput {
    ignore_flags: Ignore
}

#[derive(Clone)]
pub struct MidiInputPort {
    name: String,
    interface_id: Box<[u16]>
}

impl PartialEq for MidiInputPort {
    fn eq(&self, other: &Self) -> bool {
        self.interface_id == other.interface_id
    }
}

pub struct MidiInputConnection<T> {
    handler_data: Box<HandlerData<T>>,
}

impl MidiInputPort {
    pub fn count() -> UINT {
        unsafe { midiInGetNumDevs() }
    }

    fn interface_id(port_number: UINT) -> Result<Box<[u16]>, PortInfoError> {
        let mut buffer_size: winapi::shared::minwindef::ULONG = 0;
        let result = unsafe { winapi::um::mmeapi::midiInMessage(port_number as HMIDIIN, DRV_QUERYDEVICEINTERFACESIZE, &mut buffer_size as *mut _ as DWORD_PTR, 0) };
        if result == MMSYSERR_BADDEVICEID {
            return Err(PortInfoError::PortNumberOutOfRange)
        } else if result != MMSYSERR_NOERROR {
            return Err(PortInfoError::CannotRetrievePortName)
        }
        let mut buffer = Vec::<u16>::with_capacity(buffer_size as usize / 2);
        unsafe {
            let result = winapi::um::mmeapi::midiInMessage(port_number as HMIDIIN, DRV_QUERYDEVICEINTERFACE, buffer.as_mut_ptr() as DWORD_PTR, buffer_size as DWORD_PTR);
            if result == MMSYSERR_BADDEVICEID {
                return Err(PortInfoError::PortNumberOutOfRange)
            } else if result != MMSYSERR_NOERROR {
                return Err(PortInfoError::CannotRetrievePortName)
            }
            buffer.set_len(buffer_size as usize / 2);
        }
        //println!("{}", from_wide_ptr(buffer.as_ptr(), buffer.len()).to_string_lossy().into_owned());
        Ok(buffer.into_boxed_slice())
    }
    
    fn name(port_number: UINT) -> Result<String, PortInfoError> {
        let mut device_caps: MaybeUninit<MIDIINCAPSW> = MaybeUninit::uninit();
        let result = unsafe { midiInGetDevCapsW(port_number as UINT_PTR, device_caps.as_mut_ptr(), mem::size_of::<MIDIINCAPSW>() as u32) };
        if result == MMSYSERR_BADDEVICEID {
            return Err(PortInfoError::PortNumberOutOfRange)
        } else if result != MMSYSERR_NOERROR {
            return Err(PortInfoError::CannotRetrievePortName)
        }
        let device_caps = unsafe { device_caps.assume_init() };
        let pname = device_caps.szPname;
        let output = from_wide_ptr(pname.as_ptr(), pname.len()).to_string_lossy().into_owned();
        Ok(output)
    }

    fn from_port_number(port_number: UINT) -> Result<Self, PortInfoError> {
        Ok(MidiInputPort {
            name: Self::name(port_number)?,
            interface_id: Self::interface_id(port_number)?
        })
    }

    fn current_port_number(&self) -> Option<UINT> {
        for i in 0..Self::count() {
            if let Ok(name) = Self::name(i) {
                if name != self.name { continue; }
                if let Ok(id) = Self::interface_id(i) {
                    if id == self.interface_id {
                        return Some(i);
                    }
                }
            }
        }
        None
    }
}

struct SysexBuffer([LPMIDIHDR; RT_SYSEX_BUFFER_COUNT]);
unsafe impl Send for SysexBuffer {}

struct MidiInHandle(Mutex<HMIDIIN>);
unsafe impl Send for MidiInHandle {}

/// This is all the data that is stored on the heap as long as a connection
/// is opened and passed to the callback handler.
///
/// It is important that `user_data` is the last field to not influence
/// offsets after monomorphization.
struct HandlerData<T> {
    message: MidiMessage,
    sysex_buffer: SysexBuffer,
    in_handle: Option<MidiInHandle>,
    ignore_flags: Ignore,
    callback: Box<dyn FnMut(u64, &[u8], &mut T) + Send + 'static>,
    user_data: Option<T>
}

impl MidiInput {
    pub fn new(_client_name: &str) -> Result<Self, InitError> {
        Ok(MidiInput { ignore_flags: Ignore::None })
    }
    
    pub fn ignore(&mut self, flags: Ignore) {
        self.ignore_flags = flags;
    }

    pub(crate) fn ports_internal(&self) -> Vec<::common::MidiInputPort> {
        let count = MidiInputPort::count();
        let mut result = Vec::with_capacity(count as usize);
        for i in 0..count {
            let port = match MidiInputPort::from_port_number(i) {
                Ok(p) => p,
                Err(_) => continue
            };
            result.push(::common::MidiInputPort {
                imp: port
            });
        }
        result
    }
    
    pub fn port_count(&self) -> usize {
        MidiInputPort::count() as usize
    }

    pub fn port_name(&self, port: &MidiInputPort) -> Result<String, PortInfoError> {
        Ok(port.name.clone())
    }
    
    pub fn connect<F, T: Send>(
        self, port: &MidiInputPort, _port_name: &str, callback: F, data: T
    ) -> Result<MidiInputConnection<T>, ConnectError<MidiInput>>
        where F: FnMut(u64, &[u8], &mut T) + Send + 'static {
        
        let port_number = match port.current_port_number() {
            Some(p) => p,
            None => return Err(ConnectError::new(ConnectErrorKind::InvalidPort, self))
        };

        let mut handler_data = Box::new(HandlerData {
            message: MidiMessage::new(),
            sysex_buffer: SysexBuffer([null_mut(); RT_SYSEX_BUFFER_COUNT]),
            in_handle: None,
            ignore_flags: self.ignore_flags,
            callback: Box::new(callback),
            user_data: Some(data)
        });
        
        let mut in_handle: MaybeUninit<HMIDIIN> = MaybeUninit::uninit();
        let handler_data_ptr: *mut HandlerData<T> = &mut *handler_data;
        let result = unsafe { midiInOpen(in_handle.as_mut_ptr(),
                        port_number as UINT,
                        handler::handle_input::<T> as DWORD_PTR,
                        handler_data_ptr as DWORD_PTR,
                        CALLBACK_FUNCTION) };
        if result == MMSYSERR_ALLOCATED { 
            return Err(ConnectError::other("could not create Windows MM MIDI input port (MMSYSERR_ALLOCATED)", self));
        } else if result != MMSYSERR_NOERROR {
            return Err(ConnectError::other("could not create Windows MM MIDI input port", self));
        }
        let in_handle = unsafe { in_handle.assume_init() };

        // Allocate and init the sysex buffers.
        for i in 0..RT_SYSEX_BUFFER_COUNT {
            handler_data.sysex_buffer.0[i] = Box::into_raw(Box::new(MIDIHDR {
                lpData: unsafe { allocate(RT_SYSEX_BUFFER_SIZE/*, mem::align_of::<u8>()*/) } as *mut i8,
                dwBufferLength: RT_SYSEX_BUFFER_SIZE as u32,
                dwBytesRecorded: 0,
                dwUser: i as DWORD_PTR, // We use the dwUser parameter as buffer indicator
                dwFlags: 0,
                lpNext: ptr::null_mut(),
                reserved: 0,
                dwOffset: 0,
                dwReserved: unsafe { mem::zeroed() },
            }));
            
            // TODO: are those buffers ever freed if an error occurs here (altough these calls probably only fail with out-of-memory)?
            // TODO: close port in case of error?
            
            let result = unsafe { midiInPrepareHeader(in_handle, handler_data.sysex_buffer.0[i], mem::size_of::<MIDIHDR>() as u32) };
            if result != MMSYSERR_NOERROR {
                return Err(ConnectError::other("could not initialize Windows MM MIDI input port (PrepareHeader)", self));
            }
            
            // Register the buffer.
            let result = unsafe { midiInAddBuffer(in_handle, handler_data.sysex_buffer.0[i], mem::size_of::<MIDIHDR>() as u32) };
            if result != MMSYSERR_NOERROR {
                return Err(ConnectError::other("could not initialize Windows MM MIDI input port (AddBuffer)", self));
            }            
        }
        
        handler_data.in_handle = Some(MidiInHandle(Mutex::new(in_handle)));
        
        // We can safely access (a copy of) `in_handle` here, although
        // it has been copied into the Mutex already, because the callback
        // has not been called yet.
        let result = unsafe { midiInStart(in_handle) };
        if result != MMSYSERR_NOERROR {
            unsafe { midiInClose(in_handle) };
            return Err(ConnectError::other("could not start Windows MM MIDI input port", self));
        }
        
        Ok(MidiInputConnection {
            handler_data: handler_data
        })
    }
}

impl<T> MidiInputConnection<T> {
    pub fn close(mut self) -> (MidiInput, T) {
        self.close_internal();
        
        (MidiInput {
            ignore_flags: self.handler_data.ignore_flags,
        }, self.handler_data.user_data.take().unwrap())
    }
    
    fn close_internal(&mut self) {
        // for information about his lock, see https://groups.google.com/forum/#!topic/mididev/6OUjHutMpEo
        let in_handle_lock = self.handler_data.in_handle.as_ref().unwrap().0.lock().unwrap();
        
        // TODO: Call both reset and stop here? The difference seems to be that
        //       reset "returns all pending input buffers to the callback function"
        unsafe {
            midiInReset(*in_handle_lock);
            midiInStop(*in_handle_lock);
        }
        
        for i in 0..RT_SYSEX_BUFFER_COUNT {
            let result;
            unsafe {
                result = midiInUnprepareHeader(*in_handle_lock, self.handler_data.sysex_buffer.0[i], mem::size_of::<MIDIHDR>() as u32);
                deallocate((*self.handler_data.sysex_buffer.0[i]).lpData as *mut u8, RT_SYSEX_BUFFER_SIZE/*, mem::align_of::<u8>()*/);
                // recreate the Box so that it will be dropped/deallocated at the end of this scope
                let _ = Box::from_raw(self.handler_data.sysex_buffer.0[i]);
            }
            
            if result != MMSYSERR_NOERROR {
                let _ = writeln!(stderr(), "Warning: Ignoring error shutting down Windows MM input port (UnprepareHeader).");
            }
        }
        
        unsafe { midiInClose(*in_handle_lock) };
    }
}

impl<T> Drop for MidiInputConnection<T> {
    fn drop(&mut self) {
        // If user_data has been emptied, we know that we already have closed the connection
        if self.handler_data.user_data.is_some() {
            self.close_internal()
        }
    }
}

#[derive(Debug)]
pub struct MidiOutput;

#[derive(Clone)]
pub struct MidiOutputPort {
    name: String,
    interface_id: Box<[u16]>
}

impl PartialEq for MidiOutputPort {
    fn eq(&self, other: &Self) -> bool {
        self.interface_id == other.interface_id
    }
}

pub struct MidiOutputConnection {
    out_handle: HMIDIOUT,
}

unsafe impl Send for MidiOutputConnection {}

impl MidiOutputPort {
    pub fn count() -> UINT {
        unsafe { midiOutGetNumDevs() }
    }

    fn interface_id(port_number: UINT) -> Result<Box<[u16]>, PortInfoError> {
        let mut buffer_size: winapi::shared::minwindef::ULONG = 0;
        let result = unsafe { winapi::um::mmeapi::midiOutMessage(port_number as HMIDIOUT, DRV_QUERYDEVICEINTERFACESIZE, &mut buffer_size as *mut _ as DWORD_PTR, 0) };
        if result == MMSYSERR_BADDEVICEID {
            return Err(PortInfoError::PortNumberOutOfRange)
        } else if result != MMSYSERR_NOERROR {
            return Err(PortInfoError::CannotRetrievePortName)
        }
        let mut buffer = Vec::<u16>::with_capacity(buffer_size as usize / 2);
        unsafe {
            let result = winapi::um::mmeapi::midiOutMessage(port_number as HMIDIOUT, DRV_QUERYDEVICEINTERFACE, buffer.as_mut_ptr() as DWORD_PTR, buffer_size as DWORD_PTR);
            if result == MMSYSERR_BADDEVICEID {
                return Err(PortInfoError::PortNumberOutOfRange)
            } else if result != MMSYSERR_NOERROR {
                return Err(PortInfoError::CannotRetrievePortName)
            }
            buffer.set_len(buffer_size as usize / 2);
        }
        //println!("{}", from_wide_ptr(buffer.as_ptr(), buffer.len()).to_string_lossy().into_owned());
        Ok(buffer.into_boxed_slice())
    }
    
    fn name(port_number: UINT) -> Result<String, PortInfoError> {
        let mut device_caps: MaybeUninit<MIDIOUTCAPSW> = MaybeUninit::uninit();
        let result = unsafe { midiOutGetDevCapsW(port_number as UINT_PTR, device_caps.as_mut_ptr(), mem::size_of::<MIDIOUTCAPSW>() as u32) };
        if result == MMSYSERR_BADDEVICEID {
            return Err(PortInfoError::PortNumberOutOfRange)
        } else if result != MMSYSERR_NOERROR {
            return Err(PortInfoError::CannotRetrievePortName)
        }
        let device_caps = unsafe { device_caps.assume_init() };
        let pname = device_caps.szPname;
        let output = from_wide_ptr(pname.as_ptr(), pname.len()).to_string_lossy().into_owned();
        Ok(output)
    }

    fn from_port_number(port_number: UINT) -> Result<Self, PortInfoError> {
        Ok(MidiOutputPort {
            name: Self::name(port_number)?,
            interface_id: Self::interface_id(port_number)?
        })
    }

    fn current_port_number(&self) -> Option<UINT> {
        for i in 0..Self::count() {
            if let Ok(name) = Self::name(i) {
                if name != self.name { continue; }
                if let Ok(id) = Self::interface_id(i) {
                    if id == self.interface_id {
                        return Some(i);
                    }
                }
            }
        }
        None
    }
}

impl MidiOutput {
    pub fn new(_client_name: &str) -> Result<Self, InitError> {
        Ok(MidiOutput)
    }

    pub(crate) fn ports_internal(&self) -> Vec<::common::MidiOutputPort> {
        let count = MidiOutputPort::count();
        let mut result = Vec::with_capacity(count as usize);
        for i in 0..count {
            let port = match MidiOutputPort::from_port_number(i) {
                Ok(p) => p,
                Err(_) => continue
            };
            result.push(::common::MidiOutputPort {
                imp: port
            });
        }
        result
    }
    
    pub fn port_count(&self) -> usize {
        MidiOutputPort::count() as usize
    }

    pub fn port_name(&self, port: &MidiOutputPort) -> Result<String, PortInfoError> {
        Ok(port.name.clone())
    }
    
    pub fn connect(self, port: &MidiOutputPort, _port_name: &str) -> Result<MidiOutputConnection, ConnectError<MidiOutput>> {
        let port_number = match port.current_port_number() {
            Some(p) => p,
            None => return Err(ConnectError::new(ConnectErrorKind::InvalidPort, self))
        };
        let mut out_handle: MaybeUninit<HMIDIOUT> = MaybeUninit::uninit();
        let result = unsafe { midiOutOpen(out_handle.as_mut_ptr(), port_number as UINT, 0, 0, CALLBACK_NULL) };
        if result == MMSYSERR_ALLOCATED {
            return Err(ConnectError::other("could not create Windows MM MIDI output port (MMSYSERR_ALLOCATED)", self));
        } else if result != MMSYSERR_NOERROR {
            return Err(ConnectError::other("could not create Windows MM MIDI output port", self));
        }
        Ok(MidiOutputConnection {
            out_handle: unsafe { out_handle.assume_init() },
        })
    }
}

impl MidiOutputConnection {
    pub fn close(self) -> MidiOutput {
        // The actual closing is done by the implementation of Drop
        MidiOutput // In this API this is a noop
    }
    
    pub fn send(&mut self, message: &[u8]) -> Result<(), SendError> {
        let nbytes = message.len();
        if nbytes == 0 {
            return Err(SendError::InvalidData("message to be sent must not be empty"));
        }
        
        if message[0] == 0xF0 { // Sysex message
            // Allocate buffer for sysex data and copy message
            let mut buffer = message.to_vec();
        
            // Create and prepare MIDIHDR structure.
            let mut sysex = MIDIHDR {
                lpData: buffer.as_mut_ptr() as *mut i8,
                dwBufferLength: nbytes as u32,
                dwBytesRecorded: 0,
                dwUser: 0,
                dwFlags: 0,
                lpNext: ptr::null_mut(),
                reserved: 0,
                dwOffset: 0,
                dwReserved: unsafe { mem::zeroed() },
            };
            
            let result = unsafe { midiOutPrepareHeader(self.out_handle, &mut sysex, mem::size_of::<MIDIHDR>() as u32) };
            
            if result != MMSYSERR_NOERROR {
                return Err(SendError::Other("preparation for sending sysex message failed (OutPrepareHeader)"));
            }
            
            // Send the message.
            loop {
                let result = unsafe { midiOutLongMsg(self.out_handle, &mut sysex, mem::size_of::<MIDIHDR>() as u32) };
                if result == MIDIERR_NOTREADY {
                    sleep(Duration::from_millis(1));
                    continue;
                } else {
                    if result != MMSYSERR_NOERROR {
                        return Err(SendError::Other("sending sysex message failed"));
                    }
                    break;
                }
            }
            
            loop {
                let result = unsafe { midiOutUnprepareHeader(self.out_handle, &mut sysex, mem::size_of::<MIDIHDR>() as u32) };
                if result == MIDIERR_STILLPLAYING {
                    sleep(Duration::from_millis(1));
                    continue;
                } else { break; }
            }
        } else { // Channel or system message.
            // Make sure the message size isn't too big.
            if nbytes > 3 {
                return Err(SendError::InvalidData("non-sysex message must not be longer than 3 bytes"));
            }
            
            // Pack MIDI bytes into double word.
            let packet: DWORD = 0;
            let ptr = &packet as *const u32 as *mut u8;
            for i in 0..nbytes {
                unsafe { *ptr.offset(i as isize) = message[i] };
            }
            
            // Send the message immediately.
            loop {
                let result = unsafe { midiOutShortMsg(self.out_handle, packet) };
                if result == MIDIERR_NOTREADY {
                    sleep(Duration::from_millis(1));
                    continue;
                } else {
                    if result != MMSYSERR_NOERROR {
                        return Err(SendError::Other("sending non-sysex message failed"));
                    }
                    break;
                }
            }
        }
        
        Ok(())
    }
}

impl Drop for MidiOutputConnection {
    fn drop(&mut self) {
        unsafe {
            midiOutReset(self.out_handle);
            midiOutClose(self.out_handle);
        }
    }
}
