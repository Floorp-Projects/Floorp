#![allow(non_upper_case_globals, dead_code)]

use std::{ptr, slice, str};
use std::ffi::{CStr, CString};
use std::ops::Index;

use super::libc::{c_void, size_t};

use super::jack_sys::{
    jack_get_time,
    jack_client_t,
    jack_client_open,
    jack_client_close,
    jack_get_ports,
    jack_port_t,
    jack_port_register,
    jack_port_unregister,
    jack_port_name,
    jack_free,
    jack_activate,
    jack_deactivate,
    jack_nframes_t,
    jack_set_process_callback,
    jack_connect,
    jack_port_get_buffer,
    jack_midi_data_t,
    jack_midi_get_event_count,
    jack_midi_event_get,
    jack_midi_event_t,
    jack_midi_clear_buffer,
    jack_midi_event_reserve,
    jack_ringbuffer_t,
    jack_ringbuffer_create,
    jack_ringbuffer_read_space,
    jack_ringbuffer_read,
    jack_ringbuffer_free,
    jack_ringbuffer_write,
};

pub const JACK_DEFAULT_MIDI_TYPE: &[u8] = b"8 bit raw midi\0";

bitflags! {
    pub struct JackOpenOptions: u32 {
        const NoStartServer = 1;
        const UseExactName = 2;
        const ServerName = 4;
        const SessionID = 32;
    }
}

bitflags! {
    pub struct PortFlags: u32 {
        const PortIsInput = 1;
        const PortIsOutput = 2;
        const PortIsPhysical = 4;
        const PortCanMonitor = 8;
        const PortIsTerminal = 16;
    }
}

// TODO: hide this type
pub type ProcessCallback = extern "C" fn(nframes: jack_nframes_t, arg: *mut c_void) -> i32;

pub struct Client {
    p: *mut jack_client_t
}

unsafe impl Send for Client {}

impl Client {
    pub fn get_time() -> u64 {
        unsafe { jack_get_time() }
    }
    
    pub fn open(name: &str, options: JackOpenOptions) -> Result<Client, ()> {
        let c_name = CString::new(name).ok().expect("client name must not contain null bytes");
        let result = unsafe { jack_client_open(c_name.as_ptr(), options.bits(), ptr::null_mut()) };
        if result.is_null() {
            Err(())
        } else {
            Ok(Client { p: result })
        }
    }
    
    pub fn get_midi_ports(&self, flags: PortFlags) -> PortInfos {
        let ports_ptr = unsafe { jack_get_ports(self.p, ptr::null_mut(), JACK_DEFAULT_MIDI_TYPE.as_ptr() as *const i8, flags.bits() as u64) };
        let slice = if ports_ptr.is_null() {
            &[]
        } else {
            unsafe { 
                let count = (0isize..).find(|i| (*ports_ptr.offset(*i)).is_null()).unwrap() as usize;
                slice::from_raw_parts(ports_ptr, count)
            }
        };
        PortInfos { p: slice }
    }
    
    pub fn register_midi_port(&mut self, name: &str, flags: PortFlags) -> Result<MidiPort, ()> {
        let c_name = CString::new(name).ok().expect("port name must not contain null bytes");
        let result = unsafe { jack_port_register(self.p, c_name.as_ptr(), JACK_DEFAULT_MIDI_TYPE.as_ptr() as *const i8, flags.bits() as u64, 0) };
        if result.is_null() {
            Err(())
        } else {
            Ok(MidiPort { p: result })
        }
    }
    
    /// This can not be implemented in Drop, because it needs a reference
    /// to the client. But it consumes the MidiPort.
    pub fn unregister_midi_port(&mut self, client: MidiPort) {
        unsafe { jack_port_unregister(self.p, client.p) };
    }
    
    pub fn activate(&mut self) {
        unsafe { jack_activate(self.p) };
    }
    
    pub fn deactivate(&mut self) {
        unsafe { jack_deactivate(self.p) };
    }
    
    /// The code in the supplied function must be suitable for real-time
    /// execution. That means that it cannot call functions that might block
    /// for a long time. This includes all I/O functions (disk, TTY, network),
    /// malloc, free, printf, pthread_mutex_lock, sleep, wait, poll, select,
    /// pthread_join, pthread_cond_wait, etc, etc.
    pub fn set_process_callback(&mut self, callback: ProcessCallback, data: *mut c_void) {
        unsafe { jack_set_process_callback(self.p, Some(callback), data) };
    }
    
    pub fn connect(&mut self, source_port: &CStr, destination_port: &CStr) -> Result<(), ()> {
        let rc = unsafe { jack_connect(self.p, source_port.as_ptr(), destination_port.as_ptr()) };
        if rc == 0 {
            Ok(())
        } else {
            Err(()) // TODO: maybe handle EEXIST explicitly
        }
    }
}

impl Drop for Client {
    fn drop(&mut self) {
        unsafe { jack_client_close(self.p) };
    }
}

pub struct PortInfos<'a> {
    p: &'a[*const i8],
}

unsafe impl<'a> Send for PortInfos<'a> {}

impl<'a> PortInfos<'a> {
    pub fn count(&self) -> usize {
        self.p.len()
    }
    
    pub fn get_c_name(&self, index: usize) -> &CStr {
        let ptr = self.p[index];
        unsafe { CStr::from_ptr(ptr) }
    }
}

impl<'a> Index<usize> for PortInfos<'a> {
    type Output = str;
    
    fn index(&self, index: usize) -> &Self::Output { 
        let slice = self.get_c_name(index).to_bytes();
        str::from_utf8(slice).ok().expect("Error converting port name to UTF8")
    }
}

impl<'a> Drop for PortInfos<'a> {
    fn drop(&mut self) {
        if self.p.len() > 0 {
            unsafe { jack_free(self.p.as_ptr() as *mut _) }
        }
    }
}

pub struct MidiPort {
    p: *mut jack_port_t
}

unsafe impl Send for MidiPort {}

impl MidiPort {
    pub fn get_name(&self) -> &CStr {
        unsafe { CStr::from_ptr(jack_port_name(self.p)) }
    }
    
    pub fn get_midi_buffer(&self, nframes: jack_nframes_t) -> MidiBuffer {
        let buf = unsafe { jack_port_get_buffer(self.p, nframes) };
        MidiBuffer { p: buf }
    }
}

pub struct MidiBuffer {
    p: *mut c_void
}

impl MidiBuffer {
    pub fn get_event_count(&self) -> u32 {
        unsafe { jack_midi_get_event_count(self.p) }
    }
    
    pub unsafe fn get_event(&self, ev: *mut jack_midi_event_t, index: u32) {
        jack_midi_event_get(ev, self.p, index);
    }
    
    pub fn clear(&mut self) {
        unsafe { jack_midi_clear_buffer(self.p) }
    }
    
    pub fn event_reserve(&mut self, time: jack_nframes_t, data_size: usize) -> *mut jack_midi_data_t {
        unsafe { jack_midi_event_reserve(self.p, time, data_size as size_t) }
    }
}

pub struct Ringbuffer {
    p: *mut jack_ringbuffer_t
}

unsafe impl Send for Ringbuffer {}

impl Ringbuffer {
    pub fn new(size: usize) -> Ringbuffer {
        let result = unsafe { jack_ringbuffer_create(size as size_t) };
        Ringbuffer { p: result }
    }
    
    pub fn get_read_space(&self) -> usize {
        unsafe { jack_ringbuffer_read_space(self.p) as usize }
    }
    
    pub fn read(&mut self, destination: *mut u8, count: usize) -> usize {
        let bytes_read = unsafe { jack_ringbuffer_read(self.p, destination as *mut i8, count as size_t) };
        bytes_read as usize 
    }
    
    pub fn write(&mut self, source: &[u8]) -> usize {
        unsafe { jack_ringbuffer_write(self.p, source.as_ptr() as *const i8, source.len() as size_t) as usize }
    }
}

impl Drop for Ringbuffer{
    fn drop(&mut self) {
        unsafe { jack_ringbuffer_free(self.p) }
    }
}