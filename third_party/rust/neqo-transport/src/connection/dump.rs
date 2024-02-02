// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Enable just this file for logging to just see packets.
// e.g. "RUST_LOG=neqo_transport::dump neqo-client ..."

use std::fmt::Write;

use neqo_common::{qdebug, Decoder};

use crate::{
    connection::Connection,
    frame::Frame,
    packet::{PacketNumber, PacketType},
    path::PathRef,
};

#[allow(clippy::module_name_repetitions)]
pub fn dump_packet(
    conn: &Connection,
    path: &PathRef,
    dir: &str,
    pt: PacketType,
    pn: PacketNumber,
    payload: &[u8],
) {
    if !log::log_enabled!(log::Level::Debug) {
        return;
    }

    let mut s = String::from("");
    let mut d = Decoder::from(payload);
    while d.remaining() > 0 {
        let Ok(f) = Frame::decode(&mut d) else {
            s.push_str(" [broken]...");
            break;
        };
        if let Some(x) = f.dump() {
            write!(&mut s, "\n  {} {}", dir, &x).unwrap();
        }
    }
    qdebug!([conn], "pn={} type={:?} {}{}", pn, pt, path.borrow(), s);
}
