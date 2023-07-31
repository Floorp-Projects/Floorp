#![cfg_attr(feature = "deny-warnings", deny(warnings))]
#![warn(clippy::pedantic)]

use neqo_crypto::constants::{HandshakeMessage, TLS_HS_CLIENT_HELLO, TLS_HS_ENCRYPTED_EXTENSIONS};
use neqo_crypto::ext::{ExtensionHandler, ExtensionHandlerResult, ExtensionWriterResult};
use neqo_crypto::{Client, Server};
use std::cell::RefCell;
use std::rc::Rc;
use test_fixture::fixture_init;

mod handshake;
use crate::handshake::connect;

struct NoopExtensionHandler;
impl ExtensionHandler for NoopExtensionHandler {}

// This test just handshakes.  It doesn't really do anything about capturing the
#[test]
fn noop_extension_handler() {
    fixture_init();
    let mut client = Client::new("server.example", true).expect("should create client");
    let mut server = Server::new(&["key"]).expect("should create server");

    client
        .extension_handler(0xffff, Rc::new(RefCell::new(NoopExtensionHandler)))
        .expect("installed");
    server
        .extension_handler(0xffff, Rc::new(RefCell::new(NoopExtensionHandler)))
        .expect("installed");

    connect(&mut client, &mut server);
}

#[derive(Debug, Default)]
struct SimpleExtensionHandler {
    written: bool,
    handled: bool,
}

impl SimpleExtensionHandler {
    #[allow(dead_code)]
    pub fn negotiated(&self) -> bool {
        self.written && self.handled
    }
}

impl ExtensionHandler for SimpleExtensionHandler {
    fn write(&mut self, msg: HandshakeMessage, d: &mut [u8]) -> ExtensionWriterResult {
        match msg {
            TLS_HS_CLIENT_HELLO | TLS_HS_ENCRYPTED_EXTENSIONS => {
                self.written = true;
                d[0] = 77;
                ExtensionWriterResult::Write(1)
            }
            _ => ExtensionWriterResult::Skip,
        }
    }

    fn handle(&mut self, msg: HandshakeMessage, d: &[u8]) -> ExtensionHandlerResult {
        match msg {
            TLS_HS_CLIENT_HELLO | TLS_HS_ENCRYPTED_EXTENSIONS => {
                self.handled = true;
                if d.len() != 1 {
                    ExtensionHandlerResult::Alert(50) // decode_error
                } else if d[0] == 77 {
                    ExtensionHandlerResult::Ok
                } else {
                    ExtensionHandlerResult::Alert(47) // illegal_parameter
                }
            }
            _ => ExtensionHandlerResult::Alert(110), // unsupported_extension
        }
    }
}

#[test]
fn simple_extension() {
    fixture_init();
    let mut client = Client::new("server.example", true).expect("should create client");
    let mut server = Server::new(&["key"]).expect("should create server");

    let client_handler = Rc::new(RefCell::new(SimpleExtensionHandler::default()));
    let ch = Rc::clone(&client_handler);
    client
        .extension_handler(0xffff, ch)
        .expect("client handler installed");
    let server_handler = Rc::new(RefCell::new(SimpleExtensionHandler::default()));
    let sh = Rc::clone(&server_handler);
    server
        .extension_handler(0xffff, sh)
        .expect("server handler installed");

    connect(&mut client, &mut server);

    assert!(client_handler.borrow().negotiated());
    assert!(server_handler.borrow().negotiated());
}
