use core_foundation::{
    base::{OSStatus, TCFType},
    string::CFString,
};

use coremidi_sys::{
    MIDIClientCreate, MIDIClientDispose, MIDIDestinationCreate, MIDIInputPortCreate,
    MIDINotification, MIDIOutputPortCreate, MIDIPacketList, MIDISourceCreate,
};

use std::{mem::MaybeUninit, ops::Deref, os::raw::c_void, panic::catch_unwind, ptr};

use crate::{
    callback::BoxedCallback,
    endpoints::{destinations::VirtualDestination, sources::VirtualSource, Endpoint},
    notifications::Notification,
    object::Object,
    packets::PacketList,
    ports::{InputPort, OutputPort, Port},
    result_from_status,
};

/// A [MIDI client](https://developer.apple.com/reference/coremidi/midiclientref).
///
/// An object maintaining per-client state.
///
/// A simple example to create a Client:
///
/// ```rust,no_run
/// let client = coremidi::Client::new("example-client").unwrap();
/// ```
#[derive(Debug)]
pub struct Client {
    // Order is important, object needs to be dropped first
    object: Object,
    callback: BoxedCallback<Notification>,
}

impl Client {
    /// Creates a new CoreMIDI client with support for notifications.
    /// See [MIDIClientCreate](https://developer.apple.com/reference/coremidi/1495360-midiclientcreate).
    ///
    /// The notification callback will be called on the [run loop](https://developer.apple.com/library/archive/documentation/Cocoa/Conceptual/Multithreading/RunLoopManagement/RunLoopManagement.html)
    /// that was current when this associated function is called.
    ///
    /// It follows that this particular run loop needs to be running in order to
    /// actually receive notifications. The run loop can be started after the
    /// client has been created if need be.
    pub fn new_with_notifications<F>(name: &str, callback: F) -> Result<Client, OSStatus>
    where
        F: FnMut(&Notification) + Send + 'static,
    {
        let client_name = CFString::new(name);
        let mut client_ref = MaybeUninit::uninit();
        let mut boxed_callback = BoxedCallback::new(callback);
        let status = unsafe {
            MIDIClientCreate(
                client_name.as_concrete_TypeRef(),
                Some(Self::notify_proc as extern "C" fn(_, _)),
                boxed_callback.raw_ptr(),
                client_ref.as_mut_ptr(),
            )
        };
        result_from_status(status, || {
            let client_ref = unsafe { client_ref.assume_init() };
            Client {
                object: Object(client_ref),
                callback: boxed_callback,
            }
        })
    }

    /// Creates a new CoreMIDI client.
    /// See [MIDIClientCreate](https://developer.apple.com/reference/coremidi/1495360-midiclientcreate).
    ///
    pub fn new(name: &str) -> Result<Client, OSStatus> {
        let client_name = CFString::new(name);
        let mut client_ref = MaybeUninit::uninit();
        let status = unsafe {
            MIDIClientCreate(
                client_name.as_concrete_TypeRef(),
                None,
                ptr::null_mut(),
                client_ref.as_mut_ptr(),
            )
        };
        result_from_status(status, || {
            let client_ref = unsafe { client_ref.assume_init() };
            Client {
                object: Object(client_ref),
                callback: BoxedCallback::null(),
            }
        })
    }

    /// Creates an output port through which the client may send outgoing MIDI messages to any MIDI destination.
    /// See [MIDIOutputPortCreate](https://developer.apple.com/reference/coremidi/1495166-midioutputportcreate).
    ///
    pub fn output_port(&self, name: &str) -> Result<OutputPort, OSStatus> {
        let port_name = CFString::new(name);
        let mut port_ref = MaybeUninit::uninit();
        let status = unsafe {
            MIDIOutputPortCreate(
                self.object.0,
                port_name.as_concrete_TypeRef(),
                port_ref.as_mut_ptr(),
            )
        };
        result_from_status(status, || {
            let port_ref = unsafe { port_ref.assume_init() };
            OutputPort {
                port: Port {
                    object: Object(port_ref),
                },
            }
        })
    }

    /// Creates an input port through which the client may receive incoming MIDI messages from any MIDI source.
    /// See [MIDIInputPortCreate](https://developer.apple.com/reference/coremidi/1495225-midiinputportcreate).
    ///
    pub fn input_port<F>(&self, name: &str, callback: F) -> Result<InputPort, OSStatus>
    where
        F: FnMut(&PacketList) + Send + 'static,
    {
        let port_name = CFString::new(name);
        let mut port_ref = MaybeUninit::uninit();
        let mut box_callback = BoxedCallback::new(callback);
        let status = unsafe {
            MIDIInputPortCreate(
                self.object.0,
                port_name.as_concrete_TypeRef(),
                Some(Self::read_proc as extern "C" fn(_, _, _)),
                box_callback.raw_ptr(),
                port_ref.as_mut_ptr(),
            )
        };
        result_from_status(status, || {
            let port_ref = unsafe { port_ref.assume_init() };
            InputPort {
                port: Port {
                    object: Object(port_ref),
                },
                callback: box_callback,
            }
        })
    }

    /// Creates a virtual source in the client.
    /// See [MIDISourceCreate](https://developer.apple.com/reference/coremidi/1495212-midisourcecreate).
    ///
    pub fn virtual_source(&self, name: &str) -> Result<VirtualSource, OSStatus> {
        let virtual_source_name = CFString::new(name);
        let mut virtual_source = MaybeUninit::uninit();
        let status = unsafe {
            MIDISourceCreate(
                self.object.0,
                virtual_source_name.as_concrete_TypeRef(),
                virtual_source.as_mut_ptr(),
            )
        };
        result_from_status(status, || {
            let virtual_source = unsafe { virtual_source.assume_init() };
            VirtualSource {
                endpoint: Endpoint {
                    object: Object(virtual_source),
                },
            }
        })
    }

    /// Creates a virtual destination in the client.
    /// See [MIDIDestinationCreate](https://developer.apple.com/reference/coremidi/1495347-mididestinationcreate).
    ///
    pub fn virtual_destination<F>(
        &self,
        name: &str,
        callback: F,
    ) -> Result<VirtualDestination, OSStatus>
    where
        F: FnMut(&PacketList) + Send + 'static,
    {
        let virtual_destination_name = CFString::new(name);
        let mut virtual_destination = MaybeUninit::uninit();
        let mut boxed_callback = BoxedCallback::new(callback);
        let status = unsafe {
            MIDIDestinationCreate(
                self.object.0,
                virtual_destination_name.as_concrete_TypeRef(),
                Some(Self::read_proc as extern "C" fn(_, _, _)),
                boxed_callback.raw_ptr(),
                virtual_destination.as_mut_ptr(),
            )
        };
        result_from_status(status, || {
            let virtual_destination = unsafe { virtual_destination.assume_init() };
            VirtualDestination {
                endpoint: Endpoint {
                    object: Object(virtual_destination),
                },
                callback: boxed_callback,
            }
        })
    }

    extern "C" fn notify_proc(notification_ptr: *const MIDINotification, ref_con: *mut c_void) {
        let _ = catch_unwind(|| unsafe {
            if let Ok(notification) = Notification::from(&*notification_ptr) {
                BoxedCallback::call_from_raw_ptr(ref_con, &notification)
            }
        });
    }

    extern "C" fn read_proc(
        pktlist: *const MIDIPacketList,
        read_proc_ref_con: *mut c_void,
        _src_conn_ref_con: *mut c_void,
    ) {
        let _ = catch_unwind(|| unsafe {
            let packet_list = &*(pktlist as *const PacketList);
            BoxedCallback::call_from_raw_ptr(read_proc_ref_con, packet_list);
        });
    }
}

impl Deref for Client {
    type Target = Object;

    fn deref(&self) -> &Object {
        &self.object
    }
}

impl Drop for Client {
    fn drop(&mut self) {
        unsafe { MIDIClientDispose(self.object.0) };
    }
}
