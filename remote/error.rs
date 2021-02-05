// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

use std::num;

use http;
use nserror::{
    nsresult, NS_ERROR_ILLEGAL_VALUE, NS_ERROR_INVALID_ARG, NS_ERROR_LAUNCHED_CHILD_PROCESS,
    NS_ERROR_NOT_AVAILABLE,
};
use thiserror::Error;

#[derive(Debug, Error)]
pub enum RemoteAgentError {
    #[error("expected address syntax [<host>]:<port>: {0}")]
    AddressSpec(#[from] http::uri::InvalidUri),

    #[error("may only be instantiated in parent process")]
    ChildProcess,

    #[error("invalid port: {0}")]
    InvalidPort(#[from] num::ParseIntError),

    #[error("listener restricted to loopback devices")]
    LoopbackRestricted,

    #[error("missing port number")]
    MissingPort,

    #[error("unavailable")]
    Unavailable,

    #[error("error result {0}")]
    XpCom(#[source] nsresult),
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
