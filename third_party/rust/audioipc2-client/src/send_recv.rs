// Copyright © 2017 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details.

use cubeb_backend::Error;
use std::os::raw::c_int;

#[doc(hidden)]
pub fn _err<E>(e: E) -> Error
where
    E: Into<Option<c_int>>,
{
    match e.into() {
        Some(e) => Error::from_raw(e),
        None => Error::error(),
    }
}

#[macro_export]
macro_rules! send_recv {
    ($rpc:expr, $smsg:ident => $rmsg:ident) => {{
        let resp = send_recv!(__send $rpc, $smsg);
        send_recv!(__recv resp, $rmsg)
    }};
    ($rpc:expr, $smsg:ident => $rmsg:ident()) => {{
        let resp = send_recv!(__send $rpc, $smsg);
        send_recv!(__recv resp, $rmsg __result)
    }};
    ($rpc:expr, $smsg:ident($($a:expr),*) => $rmsg:ident) => {{
        let resp = send_recv!(__send $rpc, $smsg, $($a),*);
        send_recv!(__recv resp, $rmsg)
    }};
    ($rpc:expr, $smsg:ident($($a:expr),*) => $rmsg:ident()) => {{
        let resp = send_recv!(__send $rpc, $smsg, $($a),*);
        send_recv!(__recv resp, $rmsg __result)
    }};
    //
    (__send $rpc:expr, $smsg:ident) => ({
        $rpc.call(ServerMessage::$smsg)
    });
    (__send $rpc:expr, $smsg:ident, $($a:expr),*) => ({
        $rpc.call(ServerMessage::$smsg($($a),*))
    });
    (__recv $resp:expr, $rmsg:ident) => ({
        match $resp {
            Ok(ClientMessage::$rmsg) => Ok(()),
            Ok(ClientMessage::Error(e)) => Err($crate::send_recv::_err(e)),
            Ok(m) => {
                debug!("received wrong message - got={:?}", m);
                Err($crate::send_recv::_err(None))
            },
            Err(e) => {
                debug!("received error from rpc - got={:?}", e);
                Err($crate::send_recv::_err(None))
            },
        }
    });
    (__recv $resp:expr, $rmsg:ident __result) => ({
        match $resp {
            Ok(ClientMessage::$rmsg(v)) => Ok(v),
            Ok(ClientMessage::Error(e)) => Err($crate::send_recv::_err(e)),
            Ok(m) => {
                debug!("received wrong message - got={:?}", m);
                Err($crate::send_recv::_err(None))
            },
            Err(e) => {
                debug!("received error - got={:?}", e);
                Err($crate::send_recv::_err(None))
            },
        }
    })
}
