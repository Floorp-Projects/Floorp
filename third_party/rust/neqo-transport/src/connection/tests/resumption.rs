// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use super::{
    connect, connect_with_rtt, default_client, default_server, exchange_ticket, get_tokens,
    new_client, resumed_server, send_something, AT_LEAST_PTO,
};
use crate::addr_valid::{AddressValidation, ValidateAddress};
use crate::{ConnectionParameters, Error, Version};

use std::cell::RefCell;
use std::mem;
use std::rc::Rc;
use std::time::Duration;
use test_fixture::{self, assertions, now};

#[test]
fn resume() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    let token = exchange_ticket(&mut client, &mut server, now());
    let mut client = default_client();
    client
        .enable_resumption(now(), token)
        .expect("should set token");
    let mut server = resumed_server(&client);
    connect(&mut client, &mut server);
    assert!(client.tls_info().unwrap().resumed());
    assert!(server.tls_info().unwrap().resumed());
}

#[test]
fn remember_smoothed_rtt() {
    const RTT1: Duration = Duration::from_millis(130);
    const RTT2: Duration = Duration::from_millis(70);

    let mut client = default_client();
    let mut server = default_server();

    let mut now = connect_with_rtt(&mut client, &mut server, now(), RTT1);
    assert_eq!(client.paths.rtt(), RTT1);

    // We can't use exchange_ticket here because it doesn't respect RTT.
    // Also, connect_with_rtt() ends with the server receiving a packet it
    // wants to acknowledge; so the ticket will include an ACK frame too.
    let validation = AddressValidation::new(now, ValidateAddress::NoToken).unwrap();
    let validation = Rc::new(RefCell::new(validation));
    server.set_validation(Rc::clone(&validation));
    server.send_ticket(now, &[]).expect("can send ticket");
    let ticket = server.process_output(now).dgram();
    assert!(ticket.is_some());
    now += RTT1 / 2;
    client.process_input(ticket.unwrap(), now);
    let token = get_tokens(&mut client).pop().unwrap();

    let mut client = default_client();
    client.enable_resumption(now, token).unwrap();
    assert_eq!(
        client.paths.rtt(),
        RTT1,
        "client should remember previous RTT"
    );
    let mut server = resumed_server(&client);

    connect_with_rtt(&mut client, &mut server, now, RTT2);
    assert_eq!(
        client.paths.rtt(),
        RTT2,
        "previous RTT should be completely erased"
    );
}

/// Check that a resumed connection uses a token on Initial packets.
#[test]
fn address_validation_token_resume() {
    const RTT: Duration = Duration::from_millis(10);

    let mut client = default_client();
    let mut server = default_server();
    let validation = AddressValidation::new(now(), ValidateAddress::Always).unwrap();
    let validation = Rc::new(RefCell::new(validation));
    server.set_validation(Rc::clone(&validation));
    let mut now = connect_with_rtt(&mut client, &mut server, now(), RTT);

    let token = exchange_ticket(&mut client, &mut server, now);
    let mut client = default_client();
    client.enable_resumption(now, token).unwrap();
    let mut server = resumed_server(&client);

    // Grab an Initial packet from the client.
    let dgram = client.process(None, now).dgram();
    assertions::assert_initial(dgram.as_ref().unwrap(), true);

    // Now try to complete the handshake after giving time for a client PTO.
    now += AT_LEAST_PTO;
    connect_with_rtt(&mut client, &mut server, now, RTT);
    assert!(client.crypto.tls.info().unwrap().resumed());
    assert!(server.crypto.tls.info().unwrap().resumed());
}

fn can_resume(token: impl AsRef<[u8]>, initial_has_token: bool) {
    let mut client = default_client();
    client.enable_resumption(now(), token).unwrap();
    let initial = client.process_output(now()).dgram();
    assertions::assert_initial(initial.as_ref().unwrap(), initial_has_token);
}

#[test]
fn two_tickets_on_timer() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    // Send two tickets and then bundle those into a packet.
    server.send_ticket(now(), &[]).expect("send ticket1");
    server.send_ticket(now(), &[]).expect("send ticket2");
    let pkt = send_something(&mut server, now());

    // process() will return an ack first
    assert!(client.process(Some(pkt), now()).dgram().is_some());
    // We do not have a ResumptionToken event yet, because NEW_TOKEN was not sent.
    assert_eq!(get_tokens(&mut client).len(), 0);

    // We need to wait for release_resumption_token_timer to expire. The timer will be
    // set to 3 * PTO
    let mut now = now() + 3 * client.pto();
    mem::drop(client.process(None, now));
    let mut recv_tokens = get_tokens(&mut client);
    assert_eq!(recv_tokens.len(), 1);
    let token1 = recv_tokens.pop().unwrap();
    // Wai for anottheer 3 * PTO to get the nex okeen.
    now += 3 * client.pto();
    mem::drop(client.process(None, now));
    let mut recv_tokens = get_tokens(&mut client);
    assert_eq!(recv_tokens.len(), 1);
    let token2 = recv_tokens.pop().unwrap();
    // Wait for 3 * PTO, but now there are no more tokens.
    now += 3 * client.pto();
    mem::drop(client.process(None, now));
    assert_eq!(get_tokens(&mut client).len(), 0);
    assert_ne!(token1.as_ref(), token2.as_ref());

    can_resume(&token1, false);
    can_resume(&token2, false);
}

#[test]
fn two_tickets_with_new_token() {
    let mut client = default_client();
    let mut server = default_server();
    let validation = AddressValidation::new(now(), ValidateAddress::Always).unwrap();
    let validation = Rc::new(RefCell::new(validation));
    server.set_validation(Rc::clone(&validation));
    connect(&mut client, &mut server);

    // Send two tickets with tokens and then bundle those into a packet.
    server.send_ticket(now(), &[]).expect("send ticket1");
    server.send_ticket(now(), &[]).expect("send ticket2");
    let pkt = send_something(&mut server, now());

    client.process_input(pkt, now());
    let mut all_tokens = get_tokens(&mut client);
    assert_eq!(all_tokens.len(), 2);
    let token1 = all_tokens.pop().unwrap();
    let token2 = all_tokens.pop().unwrap();
    assert_ne!(token1.as_ref(), token2.as_ref());

    can_resume(&token1, true);
    can_resume(&token2, true);
}

/// By disabling address validation, the server won't send `NEW_TOKEN`, but
/// we can take the session ticket still.
#[test]
fn take_token() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);

    server.send_ticket(now(), &[]).unwrap();
    let dgram = server.process(None, now()).dgram();
    client.process_input(dgram.unwrap(), now());

    // There should be no ResumptionToken event here.
    let tokens = get_tokens(&mut client);
    assert_eq!(tokens.len(), 0);

    // But we should be able to get the token directly, and use it.
    let token = client.take_resumption_token(now()).unwrap();
    can_resume(&token, false);
}

/// If a version is selected and subsequently disabled, resumption fails.
#[test]
fn resume_disabled_version() {
    let mut client = new_client(
        ConnectionParameters::default().versions(Version::Version1, vec![Version::Version1]),
    );
    let mut server = default_server();
    connect(&mut client, &mut server);
    let token = exchange_ticket(&mut client, &mut server, now());

    let mut client = new_client(
        ConnectionParameters::default().versions(Version::Version2, vec![Version::Version2]),
    );
    assert_eq!(
        client.enable_resumption(now(), token).unwrap_err(),
        Error::DisabledVersion
    );
}

/// It's not possible to resume once a packet has been sent.
#[test]
fn resume_after_packet() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);
    let token = exchange_ticket(&mut client, &mut server, now());

    let mut client = default_client();
    mem::drop(client.process_output(now()).dgram().unwrap());
    assert_eq!(
        client.enable_resumption(now(), token).unwrap_err(),
        Error::ConnectionState
    );
}

/// It's not possible to resume at the server.
#[test]
fn resume_server() {
    let mut client = default_client();
    let mut server = default_server();
    connect(&mut client, &mut server);
    let token = exchange_ticket(&mut client, &mut server, now());

    let mut server = default_server();
    assert_eq!(
        server.enable_resumption(now(), token).unwrap_err(),
        Error::ConnectionState
    );
}
