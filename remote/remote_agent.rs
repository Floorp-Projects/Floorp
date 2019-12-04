// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

//! Rust interface for the Gecko remote agent.

use std::str::FromStr;

use log::*;
use nserror::NS_ERROR_ILLEGAL_VALUE;
use nsstring::{nsAString, nsString};
use xpcom::interfaces::nsIRemoteAgent;
use xpcom::RefPtr;

use crate::error::RemoteAgentError::{self, *};

pub const DEFAULT_HOST: &'static str = "localhost";
pub const DEFAULT_PORT: u16 = 9222;

pub type RemoteAgentResult<T> = Result<T, RemoteAgentError>;

pub struct RemoteAgent {
    inner: RefPtr<nsIRemoteAgent>,
}

impl RemoteAgent {
    pub fn get() -> RemoteAgentResult<RemoteAgent> {
        let inner = xpcom::services::get_RemoteAgent().ok_or(Unavailable)?;
        Ok(RemoteAgent { inner })
    }

    pub fn listen(&self, spec: &str) -> RemoteAgentResult<()> {
        let addr = http::uri::Authority::from_str(spec)?;
        let host = if addr.host().is_empty() {
            DEFAULT_HOST
        } else {
            addr.host()
        }
        .to_string();
        let port = addr.port_u16().unwrap_or(DEFAULT_PORT);

        let url = nsString::from(&format!("http://{}:{}/", host, port));
        unsafe { self.inner.Listen(&*url as &nsAString) }
            .to_result()
            .map_err(|err| {
                // TODO(ato): https://bugzil.la/1600139
                match err {
                    NS_ERROR_ILLEGAL_VALUE => RemoteAgentError::LoopbackRestricted,
                    nsresult => RemoteAgentError::from(nsresult),
                }
            })?;

        Ok(())
    }

    pub fn listening(&self) -> RemoteAgentResult<bool> {
        let mut listening = false;
        unsafe { self.inner.GetListening(&mut listening) }.to_result()?;
        Ok(listening)
    }

    pub fn close(&self) -> RemoteAgentResult<()> {
        unsafe { self.inner.Close() }.to_result()?;
        Ok(())
    }
}

impl Drop for RemoteAgent {
    fn drop(&mut self) {
        // it should always be safe to call nsIRemoteAgent.close()
        if let Err(e) = self.close() {
            error!("unable to close remote agent listener: {}", e);
        }
    }
}
