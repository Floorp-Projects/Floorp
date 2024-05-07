// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![allow(unused)]

use std::{cell::RefCell, mem, ops::Range, rc::Rc};

use neqo_common::{event::Provider, hex_with_len, qtrace, Datagram, Decoder, Role};
use neqo_crypto::{
    constants::{TLS_AES_128_GCM_SHA256, TLS_VERSION_1_3},
    hkdf,
    hp::HpKey,
    Aead, AllowZeroRtt, AuthenticationStatus, ResumptionToken,
};
use neqo_transport::{
    server::{ActiveConnectionRef, Server, ValidateAddress},
    Connection, ConnectionEvent, ConnectionParameters, State,
};
use test_fixture::{default_client, now, CountingConnectionIdGenerator};

/// Create a server.  This is different than the one in the fixture, which is a single connection.
pub fn new_server(params: ConnectionParameters) -> Server {
    Server::new(
        now(),
        test_fixture::DEFAULT_KEYS,
        test_fixture::DEFAULT_ALPN,
        test_fixture::anti_replay(),
        Box::new(AllowZeroRtt {}),
        Rc::new(RefCell::new(CountingConnectionIdGenerator::default())),
        params,
    )
    .expect("should create a server")
}

/// Create a server.  This is different than the one in the fixture, which is a single connection.
pub fn default_server() -> Server {
    new_server(ConnectionParameters::default())
}

// Check that there is at least one connection.  Returns a ref to the first confirmed connection.
pub fn connected_server(server: &mut Server) -> ActiveConnectionRef {
    let server_connections = server.active_connections();
    // Find confirmed connections.  There should only be one.
    let mut confirmed = server_connections
        .iter()
        .filter(|c: &&ActiveConnectionRef| *c.borrow().state() == State::Confirmed);
    let c = confirmed.next().expect("one confirmed");
    assert!(confirmed.next().is_none(), "only one confirmed");
    c.clone()
}

/// Connect.  This returns a reference to the server connection.
pub fn connect(client: &mut Connection, server: &mut Server) -> ActiveConnectionRef {
    server.set_validation(ValidateAddress::Never);

    assert_eq!(*client.state(), State::Init);
    let out = client.process(None, now()); // ClientHello
    assert!(out.as_dgram_ref().is_some());
    let out = server.process(out.as_dgram_ref(), now()); // ServerHello...
    assert!(out.as_dgram_ref().is_some());

    // Ingest the server Certificate.
    let out = client.process(out.as_dgram_ref(), now());
    assert!(out.as_dgram_ref().is_some()); // This should just be an ACK.
    let out = server.process(out.as_dgram_ref(), now());
    assert!(out.as_dgram_ref().is_none()); // So the server should have nothing to say.

    // Now mark the server as authenticated.
    client.authenticated(AuthenticationStatus::Ok, now());
    let out = client.process(None, now());
    assert!(out.as_dgram_ref().is_some());
    assert_eq!(*client.state(), State::Connected);
    let out = server.process(out.as_dgram_ref(), now());
    assert!(out.as_dgram_ref().is_some()); // ACK + HANDSHAKE_DONE + NST

    // Have the client process the HANDSHAKE_DONE.
    let out = client.process(out.as_dgram_ref(), now());
    assert!(out.as_dgram_ref().is_none());
    assert_eq!(*client.state(), State::Confirmed);

    connected_server(server)
}

/// Scrub through client events to find a resumption token.
pub fn find_ticket(client: &mut Connection) -> ResumptionToken {
    client
        .events()
        .find_map(|e| {
            if let ConnectionEvent::ResumptionToken(token) = e {
                Some(token)
            } else {
                None
            }
        })
        .unwrap()
}

/// Connect to the server and have it generate a ticket.
pub fn generate_ticket(server: &mut Server) -> ResumptionToken {
    let mut client = default_client();
    let mut server_conn = connect(&mut client, server);

    server_conn.borrow_mut().send_ticket(now(), &[]).unwrap();
    let out = server.process(None, now());
    client.process_input(out.as_dgram_ref().unwrap(), now()); // Consume ticket, ignore output.
    let ticket = find_ticket(&mut client);

    // Have the client close the connection and then let the server clean up.
    client.close(now(), 0, "got a ticket");
    let out = client.process_output(now());
    mem::drop(server.process(out.as_dgram_ref(), now()));
    // Calling active_connections clears the set of active connections.
    assert_eq!(server.active_connections().len(), 1);
    ticket
}
