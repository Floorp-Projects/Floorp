// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

use std::num;

use failure::Fail;
use http;
use nserror::{
    nsresult, NS_ERROR_ILLEGAL_VALUE, NS_ERROR_INVALID_ARG, NS_ERROR_LAUNCHED_CHILD_PROCESS,
    NS_ERROR_NOT_AVAILABLE,
};

#[derive(Debug, Fail)]
pub enum RemoteAgentError {
    #[fail(display = "expected address syntax [<host>]:<port>: {}", _0)]
    AddressSpec(http::uri::InvalidUri),

    #[fail(display = "may only be instantiated in parent process")]
    ChildProcess,

    #[fail(display = "invalid port: {}", _0)]
    InvalidPort(num::ParseIntError),

    #[fail(display = "listener restricted to loopback devices")]
    LoopbackRestricted,

    #[fail(display = "missing port number")]
    MissingPort,

    #[fail(display = "unavailable")]
    Unavailable,

    #[fail(display = "error result {}", _0)]
    XpCom(nsresult),
}

impl From<RemoteAgentError> for nsresult {
    fn from(err: RemoteAgentError) -> nsresult {
        use RemoteAgentError::*;
        match err {
            AddressSpec(_) | InvalidPort(_) => NS_ERROR_INVALID_ARG,
            ChildProcess => NS_ERROR_LAUNCHED_CHILD_PROCESS,
            LoopbackRestricted => NS_ERROR_ILLEGAL_VALUE,
            MissingPort => NS_ERROR_INVALID_ARG,
            Unavailable => NS_ERROR_NOT_AVAILABLE,
            XpCom(result) => result,
        }
    }
}

impl From<num::ParseIntError> for RemoteAgentError {
    fn from(err: num::ParseIntError) -> Self {
        RemoteAgentError::InvalidPort(err)
    }
}

impl From<http::uri::InvalidUri> for RemoteAgentError {
    fn from(err: http::uri::InvalidUri) -> Self {
        RemoteAgentError::AddressSpec(err)
    }
}

impl From<nsresult> for RemoteAgentError {
    fn from(result: nsresult) -> Self {
        use RemoteAgentError::*;
        match result {
            NS_ERROR_NOT_AVAILABLE => Unavailable,
            NS_ERROR_LAUNCHED_CHILD_PROCESS => ChildProcess,
            x => RemoteAgentError::XpCom(x),
        }
    }
}
