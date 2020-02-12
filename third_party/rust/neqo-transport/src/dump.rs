// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Enable just this file for logging to just see packets.
// e.g. "RUST_LOG=neqo_transport::dump neqo-client ..."

use crate::connection::Connection;
use crate::frame::decode_frame;
use crate::packet::PacketHdr;
use neqo_common::{qdebug, Decoder};

#[allow(clippy::module_name_repetitions)]
pub fn dump_packet(conn: &Connection, dir: &str, hdr: &PacketHdr, payload: &[u8]) {
    let mut s = String::from("");
    let mut d = Decoder::from(payload);
    while d.remaining() > 0 {
        let f = match decode_frame(&mut d) {
            Ok(f) => f,
            Err(_) => {
                s.push_str(" [broken]...");
                break;
            }
        };
        if let Some(x) = f.dump() {
            s.push_str(&format!("\n  {} {}", dir, &x));
        }
    }
    qdebug!([conn], "pn={} type={:?}{}", hdr.pn, hdr.tipe, s);
}
