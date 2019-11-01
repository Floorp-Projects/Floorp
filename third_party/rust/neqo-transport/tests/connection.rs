// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![deny(warnings)]

use neqo_common::Datagram;
use neqo_transport::State;
use test_fixture::{self, default_client, default_server, now};

#[test]
fn connect() {
    let (_client, _server) = test_fixture::connect();
}

#[test]
fn truncate_long_packet() {
    let mut client = default_client();
    let mut server = default_server();

    let dgram = client.process(None, now()).dgram();
    assert!(dgram.is_some());
    let dgram = server.process(dgram, now()).dgram();
    assert!(dgram.is_some());

    // This will truncate the Handshake packet from the server.
    let dupe = dgram.as_ref().unwrap().clone();
    let truncated = Datagram::new(dupe.source(), dupe.destination(), &dupe[..(dupe.len() - 1)]);
    let dupe_ack = client.process(Some(truncated), now()).dgram();
    assert!(dupe_ack.is_some());

    // Now feed in the untruncated packet.
    let dgram = client.process(dgram, now()).dgram();
    assert!(dgram.is_some()); // Throw this ACK away.
    assert!(test_fixture::maybe_authenticate(&mut client));
    let dgram = client.process(None, now()).dgram();
    assert!(dgram.is_some());

    assert_eq!(*client.state(), State::Connected);
    let dgram = server.process(dgram, now()).dgram();
    assert!(dgram.is_some());
    assert_eq!(*server.state(), State::Connected);
}
