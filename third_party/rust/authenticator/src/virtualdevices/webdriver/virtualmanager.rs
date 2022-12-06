/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use runloop::RunLoop;
use std::net::{IpAddr, Ipv4Addr, SocketAddr};
use std::ops::Deref;
use std::sync::mpsc::Sender;
use std::sync::{Arc, Mutex};
use std::vec;
use std::{io, string, thread};

use crate::authenticatorservice::{AuthenticatorTransport, RegisterArgs, SignArgs};
use crate::errors;
use crate::statecallback::StateCallback;
use crate::virtualdevices::webdriver::{testtoken, web_api};

pub struct VirtualManagerState {
    pub authenticator_counter: u64,
    pub tokens: vec::Vec<testtoken::TestToken>,
}

impl VirtualManagerState {
    pub fn new() -> Arc<Mutex<VirtualManagerState>> {
        Arc::new(Mutex::new(VirtualManagerState {
            authenticator_counter: 0,
            tokens: vec![],
        }))
    }
}

pub struct VirtualManager {
    addr: SocketAddr,
    state: Arc<Mutex<VirtualManagerState>>,
    rloop: Option<RunLoop>,
}

impl VirtualManager {
    pub fn new() -> io::Result<Self> {
        let addr = SocketAddr::new(IpAddr::V4(Ipv4Addr::LOCALHOST), 8080);
        let state = VirtualManagerState::new();
        let stateclone = state.clone();

        let builder = thread::Builder::new().name("WebDriver Command Server".into());
        builder.spawn(move || {
            web_api::serve(stateclone, addr);
        })?;

        Ok(Self {
            addr,
            state,
            rloop: None,
        })
    }

    pub fn url(&self) -> string::String {
        format!("http://{}/webauthn/authenticator", &self.addr)
    }
}

impl AuthenticatorTransport for VirtualManager {
    fn register(
        &mut self,
        timeout: u64,
        _ctap_args: RegisterArgs,
        _status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::RegisterResult>>,
    ) -> crate::Result<()> {
        if self.rloop.is_some() {
            error!("WebDriver state error, prior operation never cancelled.");
            return Err(errors::AuthenticatorError::U2FToken(
                errors::U2FTokenError::Unknown,
            ));
        }

        let state = self.state.clone();
        let rloop = try_or!(
            RunLoop::new_with_timeout(
                move |alive| {
                    while alive() {
                        let state_obj = state.lock().unwrap();

                        for token in state_obj.tokens.deref() {
                            if token.is_user_consenting {
                                let register_result = token.register();
                                thread::spawn(move || {
                                    callback.call(register_result);
                                });
                                return;
                            }
                        }
                    }
                },
                timeout
            ),
            |_| Err(errors::AuthenticatorError::Platform)
        );

        self.rloop = Some(rloop);
        Ok(())
    }

    fn sign(
        &mut self,
        timeout: u64,
        _ctap_args: SignArgs,
        _status: Sender<crate::StatusUpdate>,
        callback: StateCallback<crate::Result<crate::SignResult>>,
    ) -> crate::Result<()> {
        if self.rloop.is_some() {
            error!("WebDriver state error, prior operation never cancelled.");
            return Err(errors::AuthenticatorError::U2FToken(
                errors::U2FTokenError::Unknown,
            ));
        }

        let state = self.state.clone();
        let rloop = try_or!(
            RunLoop::new_with_timeout(
                move |alive| {
                    while alive() {
                        let state_obj = state.lock().unwrap();

                        for token in state_obj.tokens.deref() {
                            if token.is_user_consenting {
                                let sign_result = token.sign();
                                thread::spawn(move || {
                                    callback.call(sign_result);
                                });
                                return;
                            }
                        }
                    }
                },
                timeout
            ),
            |_| Err(errors::AuthenticatorError::Platform)
        );

        self.rloop = Some(rloop);
        Ok(())
    }

    fn cancel(&mut self) -> crate::Result<()> {
        if let Some(r) = self.rloop.take() {
            debug!("WebDriver operation cancelled.");
            r.cancel();
        }
        Ok(())
    }
}
