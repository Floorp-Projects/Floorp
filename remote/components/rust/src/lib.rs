// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

extern crate http;
extern crate libc;
extern crate log;
extern crate nserror;
extern crate nsstring;
extern crate thiserror;
extern crate xpcom;

mod error;
mod handler;
mod remote_agent;

pub use crate::error::RemoteAgentError;
pub use crate::remote_agent::{RemoteAgent, RemoteAgentResult, DEFAULT_HOST, DEFAULT_PORT};
