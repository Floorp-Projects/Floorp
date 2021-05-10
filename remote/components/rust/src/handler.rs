// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

use std::cell::RefCell;
use std::ffi::{CStr, CString, NulError};
use std::slice;

use libc::c_char;
use log::*;
use nserror::{nsresult, NS_ERROR_FAILURE, NS_ERROR_ILLEGAL_VALUE, NS_ERROR_INVALID_ARG, NS_OK};
use nsstring::{nsACString, nsCString, nsString};
use xpcom::interfaces::{nsICommandLine, nsICommandLineHandler, nsIObserverService, nsISupports};
use xpcom::{xpcom, xpcom_method, RefPtr};

use crate::{
    RemoteAgent,
    RemoteAgentError::{self, *},
    RemoteAgentResult, DEFAULT_HOST, DEFAULT_PORT,
};

macro_rules! fatalln {
    ($($arg:tt)*) => ({
        let p = prog().unwrap_or("gecko".to_string());
        eprintln!("{}: {}", p, format_args!($($arg)*));
        panic!();
    })
}

#[no_mangle]
pub unsafe extern "C" fn new_remote_agent_handler(result: *mut *const nsICommandLineHandler) {
    if let Ok(handler) = RemoteAgentHandler::new() {
        RefPtr::new(handler.coerce::<nsICommandLineHandler>()).forget(&mut *result);
    } else {
        *result = std::ptr::null();
    }
}

#[derive(xpcom)]
#[xpimplements(nsICommandLineHandler)]
#[xpimplements(nsIObserver)]
#[refcnt = "atomic"]
struct InitRemoteAgentHandler {
    agent: RemoteAgent,
    observer: RefPtr<nsIObserverService>,
    address: RefCell<String>,
}

impl RemoteAgentHandler {
    pub fn new() -> Result<RefPtr<Self>, RemoteAgentError> {
        let agent = RemoteAgent::get()?;
        let observer = xpcom::services::get_ObserverService().ok_or(Unavailable)?;
        Ok(Self::allocate(InitRemoteAgentHandler {
            agent,
            observer,
            address: RefCell::new(String::new()),
        }))
    }

    xpcom_method!(handle => Handle(command_line: *const nsICommandLine));
    fn handle(&self, command_line: &nsICommandLine) -> Result<(), nsresult> {
        match self.handle_inner(&command_line) {
            Ok(_) => Ok(()),
            Err(err) => fatalln!("{}", err),
        }
    }

    fn handle_inner(&self, command_line: &nsICommandLine) -> RemoteAgentResult<()> {
        let flags = CommandLine::new(command_line);

        let remote_debugging_port = if flags.present("remote-debugging-port") {
            Some(flags.opt_u16("remote-debugging-port")?)
        } else {
            None
        };

        let addr = match remote_debugging_port {
            Some(Some(port)) => format!("{}:{}", DEFAULT_HOST, port),
            Some(None) => format!("{}:{}", DEFAULT_HOST, DEFAULT_PORT),
            None => return Ok(()),
        };

        *self.address.borrow_mut() = addr.to_string();

        // When remote-startup-requested fires, it takes care of
        // asking the remote agent to listen for incoming connections.
        // Because the remote agent starts asynchronously, we wait
        // until we receive remote-listening before we declare to the
        // world that we are ready to accept connections.
        self.add_observer("remote-listening")?;
        self.add_observer("remote-startup-requested")?;

        Ok(())
    }

    fn add_observer(&self, topic: &str) -> RemoteAgentResult<()> {
        let topic = CString::new(topic).unwrap();
        unsafe {
            self.observer
                .AddObserver(self.coerce(), topic.as_ptr(), false)
        }
        .to_result()?;
        Ok(())
    }

    xpcom_method!(help_info => GetHelpInfo() -> nsACString);
    fn help_info(&self) -> Result<nsCString, nsresult> {
        let help = format!(
            r#"  --remote-debugging-port [<port>] Start the Firefox remote agent,
                     which is a low-level debugging interface based on the CDP protocol.
                     Defaults to listen on {}:{}.
"#,
            DEFAULT_HOST, DEFAULT_PORT
        );
        Ok(nsCString::from(help))
    }

    xpcom_method!(observe => Observe(_subject: *const nsISupports, topic: string, data: wstring));
    fn observe(
        &self,
        _subject: *const nsISupports,
        topic: string,
        data: wstring,
    ) -> Result<(), nsresult> {
        let topic = unsafe { CStr::from_ptr(topic) }.to_str().unwrap();

        match topic {
            "remote-startup-requested" => {
                if let Err(err) = self.agent.listen(&self.address.borrow()) {
                    fatalln!("unable to start remote agent: {}", err);
                }
            }

            "remote-listening" => {
                let output = unsafe { wstring_to_cstring(data) }.map_err(|_| NS_ERROR_FAILURE)?;
                eprintln!("{}", output.to_string_lossy());
            }

            s => warn!("unknown system notification: {}", s),
        }

        Ok(())
    }
}

// Rust wrapper for nsICommandLine.
struct CommandLine<'a> {
    inner: &'a nsICommandLine,
}

impl<'a> CommandLine<'a> {
    const CASE_SENSITIVE: bool = true;

    fn new(inner: &'a nsICommandLine) -> Self {
        Self { inner }
    }

    fn position(&self, name: &str) -> i32 {
        let flag = nsString::from(name);
        let mut result: i32 = 0;
        unsafe {
            self.inner
                .FindFlag(&*flag, Self::CASE_SENSITIVE, &mut result)
        }
        .to_result()
        .map_err(|err| error!("FindFlag: {}", err))
        .unwrap();

        result
    }

    fn present(&self, name: &str) -> bool {
        self.position(name) >= 0
    }

    // nsICommandLine.handleFlagWithParam has the following possible return values:
    //
    //   - an AString value representing the argument value if it exists
    //   - NS_ERROR_INVALID_ARG if the flag was defined, but without a value
    //   - a null pointer if the flag was not defined
    //   - possibly any other NS exception
    //
    // This means we need to treat NS_ERROR_INVALID_ARG with special care
    // because --remote-debugging-port can be used both with and without a value.
    fn opt_str(&self, name: &str) -> RemoteAgentResult<Option<String>> {
        if self.present(name) {
            let flag = nsString::from(name);
            let mut val = nsString::new();
            let result = unsafe {
                self.inner
                    .HandleFlagWithParam(&*flag, Self::CASE_SENSITIVE, &mut *val)
            }
            .to_result();

            match result {
                Ok(_) => Ok(Some(val.to_string())),
                Err(NS_ERROR_INVALID_ARG) => Ok(None),
                Err(err) => Err(RemoteAgentError::XpCom(err)),
            }
        } else {
            Err(RemoteAgentError::XpCom(NS_ERROR_ILLEGAL_VALUE))
        }
    }

    fn opt_u16(&self, name: &str) -> RemoteAgentResult<Option<u16>> {
        Ok(if let Some(s) = self.opt_str(name)? {
            Some(s.parse()?)
        } else {
            None
        })
    }
}

fn prog() -> Option<String> {
    std::env::current_exe()
        .ok()?
        .file_name()?
        .to_str()?
        .to_owned()
        .into()
}

// Arcane XPIDL types for raw character pointers
// to ASCII (7-bit) and UTF-16 strings, respectively.
// https://developer.mozilla.org/en-US/docs/Mozilla/Tech/XPCOM/Guide/Internal_strings#IDL
#[allow(non_camel_case_types)]
type string = *const c_char;
#[allow(non_camel_case_types)]
type wstring = *const i16;

// Convert wstring to a CString (via nsCString's UTF-16 to UTF-8 conversion).
// But first, say three Hail Marys.
unsafe fn wstring_to_cstring(ws: wstring) -> Result<CString, NulError> {
    let mut len: usize = 0;
    while (*(ws.offset(len as isize))) != 0 {
        len += 1;
    }
    let ss = slice::from_raw_parts(ws as *const u16, len);
    let mut s = nsCString::new();
    s.assign_utf16_to_utf8(ss);
    CString::new(s.as_str_unchecked())
}
