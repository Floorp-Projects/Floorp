//! Web MIDI Backend.
//! 
//! Reference:
//! * [W3C Editor's Draft](https://webaudio.github.io/web-midi-api/)
//! * [MDN web docs](https://developer.mozilla.org/en-US/docs/Web/API/MIDIAccess)

extern crate js_sys;
extern crate wasm_bindgen;
extern crate web_sys;

use self::js_sys::{Map, Promise, Uint8Array};
use self::wasm_bindgen::prelude::*;
use self::wasm_bindgen::JsCast;
use self::web_sys::{MidiAccess, MidiOptions, MidiMessageEvent};

use std::cell::RefCell;
use std::sync::{Arc, Mutex};

use ::errors::*;
use ::Ignore;



thread_local! {
    static STATIC : RefCell<Static> = RefCell::new(Static::new());
}

struct Static {
    pub access:         Option<MidiAccess>,
    pub request:        Option<Promise>,
    pub ever_requested: bool,

    pub on_ok:          Closure<dyn FnMut(JsValue)>,
    pub on_err:         Closure<dyn FnMut(JsValue)>,
}

impl Static {
    pub fn new() -> Self {
        let mut s = Self {
            access:         None,
            request:        None,
            ever_requested: false,

            on_ok: Closure::wrap(Box::new(|access| {
                STATIC.with(|s|{
                    let mut s = s.borrow_mut();
                    let access : MidiAccess = access.dyn_into().unwrap();
                    s.request = None;
                    s.access = Some(access);
                });
            })),
            on_err: Closure::wrap(Box::new(|_error| {
                STATIC.with(|s|{
                    let mut s = s.borrow_mut();
                    s.request = None;
                });
            })),
        };
        // Some notes on sysex behavior:
        //  1) Some devices (but not all!) may work without sysex
        //  2) Chrome will only prompt the end user to grant permission if they requested sysex permissions for now...
        //      but that's changing soon for "security reasons" (reduced fingerprinting? poorly tested drivers?):
        //      https://www.chromestatus.com/feature/5138066234671104
        //
        //  I've chosen to hardcode sysex=true here, since that'll be compatible with more devices, *and* should change
        //  less behavior when Chrome's changes land.
        s.request_midi_access(true);
        s
    }

    fn request_midi_access(&mut self, sysex: bool) {
        self.ever_requested = true;
        if self.access.is_some() { return; } // Already have access
        if self.request.is_some() { return; } // Mid-request already
        let window = if let Some(w) = web_sys::window() { w } else { return; };

        let _request = match window.navigator().request_midi_access_with_options(MidiOptions::new().sysex(sysex)) {
            Ok(p) => { self.request = Some(p.then2(&self.on_ok, &self.on_err)); },
            Err(_) => { return; } // node.js? brower doesn't support webmidi? other?
        };
    }
}

#[derive(Clone, PartialEq)]
pub struct MidiInputPort {
    input: web_sys::MidiInput,
}

pub struct MidiInput {
    ignore_flags: Ignore
}

impl MidiInput {
    pub fn new(_client_name: &str) -> Result<Self, InitError> {
        STATIC.with(|_|{});
        Ok(MidiInput { ignore_flags: Ignore::None })
    }

    pub(crate) fn ports_internal(&self) -> Vec<::common::MidiInputPort> {
        STATIC.with(|s|{
            let mut v = Vec::new();
            let s = s.borrow();
            if let Some(access) = s.access.as_ref() {
                let inputs : Map = access.inputs().unchecked_into();
                inputs.for_each(&mut |value, _|{
                    v.push(::common::MidiInputPort {
                        imp: MidiInputPort { input: value.dyn_into().unwrap() }
                    });
                });
            }
            v
        })
    }

    pub fn ignore(&mut self, flags: Ignore) {
        self.ignore_flags = flags;
    }

    pub fn port_count(&self) -> usize {
        STATIC.with(|s| {
            let s = s.borrow();
            s.access.as_ref().map(|access| access.inputs().unchecked_into::<Map>().size() as usize).unwrap_or(0)
        })
    }

    pub fn port_name(&self, port: &MidiInputPort) -> Result<String, PortInfoError> {
        Ok(port.input.name().unwrap_or_else(|| port.input.id()))
    }

    pub fn connect<F, T: Send + 'static>(
        self, port: &MidiInputPort, _port_name: &str, mut callback: F, data: T
    ) -> Result<MidiInputConnection<T>, ConnectError<MidiInput>>
        where F: FnMut(u64, &[u8], &mut T) + Send + 'static
    {
        let input = port.input.clone();
        let _ = input.open(); // NOTE: asyncronous!

        let ignore_flags = self.ignore_flags;
        let user_data = Arc::new(Mutex::new(Some(data)));

        let closure = {
            let user_data = user_data.clone();

            let closure = Closure::wrap(Box::new(move |event: MidiMessageEvent| {
                let time = (event.time_stamp() * 1000.0) as u64; // ms -> us
                let buffer = event.data().unwrap();

                let status = buffer[0];
                if !(status == 0xF0 && ignore_flags.contains(Ignore::Sysex) ||
                        status == 0xF1 && ignore_flags.contains(Ignore::Time) ||
                        status == 0xF8 && ignore_flags.contains(Ignore::Time) ||
                        status == 0xFE && ignore_flags.contains(Ignore::ActiveSense))
                {
                    callback(time, &buffer[..], user_data.lock().unwrap().as_mut().unwrap());
                }
            }) as Box<dyn FnMut(MidiMessageEvent)>);

            input.set_onmidimessage(Some(closure.as_ref().unchecked_ref()));

            closure
        };

        Ok(MidiInputConnection { ignore_flags, input, user_data, closure })
    }
}

pub struct MidiInputConnection<T> {
    ignore_flags:   Ignore,
    input:          web_sys::MidiInput,
    user_data:      Arc<Mutex<Option<T>>>,
    #[allow(dead_code)] // Must be kept alive until we decide to unregister from input
    closure:        Closure<dyn FnMut(MidiMessageEvent)>,
}

impl<T> MidiInputConnection<T> {
    pub fn close(self) -> (MidiInput, T) {
        let Self { ignore_flags, input, user_data, .. } = self;

        input.set_onmidimessage(None);
        let mut user_data = user_data.lock().unwrap();

        (
            MidiInput { ignore_flags },
            user_data.take().unwrap()
        )
    }
}

#[derive(Clone, PartialEq)]
pub struct MidiOutputPort {
    output: web_sys::MidiOutput,
}

pub struct MidiOutput {
}

impl MidiOutput {
    pub fn new(_client_name: &str) -> Result<Self, InitError> {
        STATIC.with(|_|{});
        Ok(MidiOutput {})
    }

    pub(crate) fn ports_internal(&self) -> Vec<::common::MidiOutputPort> {
        STATIC.with(|s|{
            let mut v = Vec::new();
            let s = s.borrow();
            if let Some(access) = s.access.as_ref() {
                access.outputs().unchecked_into::<Map>().for_each(&mut |value, _|{
                    v.push(::common::MidiOutputPort {
                        imp: MidiOutputPort { output: value.dyn_into().unwrap() }
                    });
                });
            }
            v
        })
    }

    pub fn port_count(&self) -> usize {
        STATIC.with(|s|{
            let s = s.borrow();
            s.access.as_ref().map(|access| access.outputs().unchecked_into::<Map>().size() as usize).unwrap_or(0)
        })
    }

    pub fn port_name(&self, port: &MidiOutputPort) -> Result<String, PortInfoError> {
        Ok(port.output.name().unwrap_or_else(|| port.output.id()))
    }

    pub fn connect(self, port: &MidiOutputPort, _port_name: &str) -> Result<MidiOutputConnection, ConnectError<MidiOutput>> {
        let _ = port.output.open(); // NOTE: asyncronous!
        Ok(MidiOutputConnection{
            output: port.output.clone()
        })
    }
}

pub struct MidiOutputConnection {
    output: web_sys::MidiOutput,
}

impl MidiOutputConnection {
    pub fn close(self) -> MidiOutput {
        let _ = self.output.close(); // NOTE: asyncronous!
        MidiOutput {}
    }
    
    pub fn send(&mut self, message: &[u8]) -> Result<(), SendError> {
        self.output.send(unsafe { Uint8Array::view(message) }.as_ref()).map_err(|_| SendError::Other("JavaScript exception"))
    }
}
