// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// http://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or http://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]
#![cfg(feature = "fuzzing")]

use super::{connect_force_idle, default_client, default_server};
use crate::StreamType;
use neqo_crypto::FIXED_TAG_FUZZING;
use test_fixture::now;

#[test]
fn no_encryption() {
    const DATA_CLIENT: &[u8] = &[2; 40];
    const DATA_SERVER: &[u8] = &[3; 50];
    let mut client = default_client();
    let mut server = default_server();
    connect_force_idle(&mut client, &mut server);

    let stream_id = client.stream_create(StreamType::BiDi).unwrap();

    client.stream_send(stream_id, DATA_CLIENT).unwrap();
    let client_pkt = client.process_output(now()).dgram().unwrap();
    assert!(client_pkt[..client_pkt.len() - FIXED_TAG_FUZZING.len()].ends_with(DATA_CLIENT));

    server.process_input(client_pkt, now());
    let mut buf = vec![0; 100];
    let (len, _) = server.stream_recv(stream_id, &mut buf).unwrap();
    assert_eq!(len, DATA_CLIENT.len());
    assert_eq!(&buf[..len], DATA_CLIENT);
    server.stream_send(stream_id, DATA_SERVER).unwrap();
    let server_pkt = server.process_output(now()).dgram().unwrap();
    assert!(server_pkt[..server_pkt.len() - FIXED_TAG_FUZZING.len()].ends_with(DATA_SERVER));

    client.process_input(server_pkt, now());
    let (len, _) = client.stream_recv(stream_id, &mut buf).unwrap();
    assert_eq!(len, DATA_SERVER.len());
    assert_eq!(&buf[..len], DATA_SERVER);
}
