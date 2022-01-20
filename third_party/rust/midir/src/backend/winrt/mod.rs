extern crate winrt;

use std::sync::{Arc, Mutex};

use ::errors::*;
use ::Ignore;

use self::winrt::{AbiTransferable, HString, TryInto};

winrt::import!(
    dependencies
        os
    types
        windows::foundation::*
        windows::devices::midi::*
        windows::devices::enumeration::DeviceInformation
        windows::storage::streams::{Buffer, DataWriter}
);

use self::windows::foundation::*;
use self::windows::devices::midi::*;
use self::windows::devices::enumeration::DeviceInformation;
use self::windows::storage::streams::{Buffer, DataWriter};

#[derive(Clone, PartialEq)]
pub struct MidiInputPort {
    id: HString
}

unsafe impl Send for MidiInputPort {} // because HString doesn't ...

pub struct MidiInput {
    selector: HString,
    ignore_flags: Ignore
}

#[repr(C)]
pub struct abi_IMemoryBufferByteAccess {
    __base: [usize; 3],
    get_buffer: extern "system" fn(
        winrt::NonNullRawComPtr<IMemoryBufferByteAccess>,
        value: *mut *mut u8,
        capacity: *mut u32,
    ) -> winrt::ErrorCode,
}

unsafe impl winrt::ComInterface for IMemoryBufferByteAccess {
    type VTable = abi_IMemoryBufferByteAccess;
    fn iid() -> winrt::Guid {
      winrt::Guid::from_values(0x5b0d3235, 0x4dba, 0x4d44, [0x86, 0x5e, 0x8f, 0x1d, 0x0e, 0x4f, 0xd0, 0x4d])
    }
}

unsafe impl AbiTransferable for IMemoryBufferByteAccess {
    type Abi = winrt::RawComPtr<Self>;

    fn get_abi(&self) -> Self::Abi {
        self.ptr.get_abi()
    }

    fn set_abi(&mut self) -> *mut Self::Abi {
        self.ptr.set_abi()
    }
}

#[repr(transparent)]
#[derive(Default, Clone)]
pub struct IMemoryBufferByteAccess {
    ptr: winrt::ComPtr<IMemoryBufferByteAccess>,
}

impl IMemoryBufferByteAccess {
    pub unsafe fn get_buffer(&self) -> winrt::Result<&[u8]> {
        match self.get_abi() {
            None => panic!("The `this` pointer was null when calling method"),
            Some(ptr) => {
                let mut bufptr = std::ptr::null_mut();
                let mut capacity: u32 = 0;
                (ptr.vtable().get_buffer)(ptr, &mut bufptr, &mut capacity).ok()?;
                if capacity == 0 {
                    bufptr = 1 as *mut u8; // null pointer is not allowed
                }
                Ok(std::slice::from_raw_parts(bufptr, capacity as usize))
            }
        }
    }
}


unsafe impl Send for MidiInput {} // because HString doesn't ...

impl MidiInput {
    pub fn new(_client_name: &str) -> Result<Self, InitError> {
        let device_selector = MidiInPort::get_device_selector().map_err(|_| InitError)?;
        Ok(MidiInput { selector: device_selector, ignore_flags: Ignore::None })
    }

    pub fn ignore(&mut self, flags: Ignore) {
        self.ignore_flags = flags;
    }

    pub(crate) fn ports_internal(&self) -> Vec<::common::MidiInputPort> {
        let device_collection = DeviceInformation::find_all_async_aqs_filter(&self.selector).unwrap().get().expect("find_all_async failed");
        let count = device_collection.size().expect("get_size failed") as usize;
        let mut result = Vec::with_capacity(count as usize);
        for device_info in device_collection.into_iter() {
            let device_id = device_info.id().expect("get_id failed");
            result.push(::common::MidiInputPort {
                imp: MidiInputPort { id: device_id }
            });
        }
        result
    }
    
    pub fn port_count(&self) -> usize {
        let device_collection = DeviceInformation::find_all_async_aqs_filter(&self.selector).unwrap().get().expect("find_all_async failed");
        device_collection.size().expect("get_size failed") as usize
    }
    
    pub fn port_name(&self, port: &MidiInputPort) -> Result<String, PortInfoError> {
        let device_info_async = DeviceInformation::create_from_id_async(&port.id).map_err(|_| PortInfoError::InvalidPort)?;
        let device_info = device_info_async.get().map_err(|_| PortInfoError::InvalidPort)?;
        let device_name = device_info.name().map_err(|_| PortInfoError::CannotRetrievePortName)?;
        Ok(device_name.to_string())
    }

    fn handle_input<T>(args: &MidiMessageReceivedEventArgs, handler_data: &mut HandlerData<T>) {
        let ignore = handler_data.ignore_flags;
        let data = &mut handler_data.user_data.as_mut().unwrap();
        let timestamp;
        let byte_access: IMemoryBufferByteAccess;
        let message_bytes;
        let message = args.message().expect("get_message failed");
        timestamp = message.timestamp().expect("get_timestamp failed").duration as u64 / 10;
        let buffer = message.raw_data().expect("get_raw_data failed");
        let membuffer = Buffer::create_memory_buffer_over_ibuffer(&buffer).expect("create_memory_buffer_over_ibuffer failed");
        byte_access = membuffer.create_reference().expect("create_reference failed").try_into().unwrap();
        message_bytes = unsafe { byte_access.get_buffer().expect("get_buffer failed") }; // TODO: somehow make sure that the buffer is not invalidated while we're reading from it ...

        // The first byte in the message is the status
        let status = message_bytes[0];

        if !(status == 0xF0 && ignore.contains(Ignore::Sysex) ||
             status == 0xF1 && ignore.contains(Ignore::Time) ||
             status == 0xF8 && ignore.contains(Ignore::Time) ||
             status == 0xFE && ignore.contains(Ignore::ActiveSense))
        {
            (handler_data.callback)(timestamp, message_bytes, data);
        }
    }

    pub fn connect<F, T: Send + 'static>(
        self, port: &MidiInputPort, _port_name: &str, callback: F, data: T
    ) -> Result<MidiInputConnection<T>, ConnectError<MidiInput>>
        where F: FnMut(u64, &[u8], &mut T) + Send + 'static {
        
        let in_port = match MidiInPort::from_id_async(&port.id) {
            Ok(port_async) => match port_async.get() {
                Ok(port) => port,
                _ => return Err(ConnectError::new(ConnectErrorKind::InvalidPort, self))
            }
            Err(_) => return Err(ConnectError::new(ConnectErrorKind::InvalidPort, self))
        };
        
        let handler_data = Arc::new(Mutex::new(HandlerData {
            ignore_flags: self.ignore_flags,
            callback: Box::new(callback),
            user_data: Some(data)
        }));
        let handler_data2 = handler_data.clone();

        let handler = TypedEventHandler::new(move |_sender, args| {
            MidiInput::handle_input(args, &mut *handler_data2.lock().unwrap());
            Ok(())
        });
        
        let event_token = in_port.message_received(&handler).expect("add_message_received failed");

        Ok(MidiInputConnection { port: RtMidiInPort(in_port), event_token: event_token, handler_data: handler_data })
    }
}

struct RtMidiInPort(MidiInPort);
unsafe impl Send for RtMidiInPort {}

pub struct MidiInputConnection<T> {
    port: RtMidiInPort,
    event_token: EventRegistrationToken,
    // TODO: get rid of Arc & Mutex?
    //       synchronization is required because the borrow checker does not
    //       know that the callback we're in here is never called concurrently
    //       (always in sequence)
    handler_data: Arc<Mutex<HandlerData<T>>>
}


impl<T> MidiInputConnection<T> {
    pub fn close(self) -> (MidiInput, T) {
        let _ = self.port.0.remove_message_received(self.event_token);
        let closable: IClosable = self.port.0.try_into().unwrap();
        let _ = closable.close();
        let device_selector = MidiInPort::get_device_selector().expect("get_device_selector failed"); // probably won't ever fail here, because it worked previously
        let mut handler_data_locked = self.handler_data.lock().unwrap();
        (MidiInput {
            selector: device_selector,
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
    ignore_flags: Ignore,
    callback: Box<dyn FnMut(u64, &[u8], &mut T) + Send>,
    user_data: Option<T>
}

#[derive(Clone, PartialEq)]
pub struct MidiOutputPort {
    id: HString
}

unsafe impl Send for MidiOutputPort {} // because HString doesn't ...

pub struct MidiOutput {
    selector: HString // TODO: change to FastHString?
}

unsafe impl Send for MidiOutput {} // because HString doesn't ...

impl MidiOutput {
    pub fn new(_client_name: &str) -> Result<Self, InitError> {
        let device_selector = MidiOutPort::get_device_selector().map_err(|_| InitError)?;
        Ok(MidiOutput { selector: device_selector })
    }

    pub(crate) fn ports_internal(&self) -> Vec<::common::MidiOutputPort> {
        let device_collection = DeviceInformation::find_all_async_aqs_filter(&self.selector).unwrap().get().expect("find_all_async failed");
        let count = device_collection.size().expect("get_size failed") as usize;
        let mut result = Vec::with_capacity(count as usize);
        for device_info in device_collection.into_iter() {
            let device_id = device_info.id().expect("get_id failed");
            result.push(::common::MidiOutputPort {
                imp: MidiOutputPort { id: device_id }
            });
        }
        result
    }
    
    pub fn port_count(&self) -> usize {
        let device_collection = DeviceInformation::find_all_async_aqs_filter(&self.selector).unwrap().get().expect("find_all_async failed");
        device_collection.size().expect("get_size failed") as usize
    }
    
    pub fn port_name(&self, port: &MidiOutputPort) -> Result<String, PortInfoError> {
        let device_info_async = DeviceInformation::create_from_id_async(&port.id).map_err(|_| PortInfoError::InvalidPort)?;
        let device_info = device_info_async.get().map_err(|_| PortInfoError::InvalidPort)?;
        let device_name = device_info.name().map_err(|_| PortInfoError::CannotRetrievePortName)?;
        Ok(device_name.to_string())
    }
    
    pub fn connect(self, port: &MidiOutputPort, _port_name: &str) -> Result<MidiOutputConnection, ConnectError<MidiOutput>> {        
        let out_port = match MidiOutPort::from_id_async(&port.id) {
            Ok(port_async) => match port_async.get() {
                Ok(port) => port,
                _ => return Err(ConnectError::new(ConnectErrorKind::InvalidPort, self))
            }
            Err(_) => return Err(ConnectError::new(ConnectErrorKind::InvalidPort, self))
        };
        Ok(MidiOutputConnection { port: out_port })
    }
}

pub struct MidiOutputConnection {
    port: IMidiOutPort
}

unsafe impl Send for MidiOutputConnection {}

impl MidiOutputConnection {
    pub fn close(self) -> MidiOutput {
        let closable: IClosable = self.port.try_into().unwrap();
        let _ = closable.close();
        let device_selector = MidiOutPort::get_device_selector().expect("get_device_selector failed"); // probably won't ever fail here, because it worked previously
        MidiOutput { selector: device_selector }
    }
    
    pub fn send(&mut self, message: &[u8]) -> Result<(), SendError> {
        let data_writer = DataWriter::new().unwrap();
        data_writer.write_bytes(message).map_err(|_| SendError::Other("write_bytes failed"))?;
        let buffer = data_writer.detach_buffer().map_err(|_| SendError::Other("detach_buffer failed"))?;
        self.port.send_buffer(&buffer).map_err(|_| SendError::Other("send_buffer failed"))?;
        Ok(())
    }
}
