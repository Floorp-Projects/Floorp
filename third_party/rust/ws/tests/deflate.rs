#![cfg(feature = "permessage-deflate")]
extern crate env_logger;
extern crate url;
extern crate ws;

use ws::deflate::DeflateHandler;
use ws::{Builder, Message, Sender, Settings, WebSocket};

#[test]
fn round_trip() {
    const MESSAGE: &'static str = "this is the message that will be sent as a message";

    let mut name = "Client";

    let mut ws = WebSocket::new(|output: Sender| {
        if name == "Client" {
            output.send(MESSAGE).unwrap();
        }

        let handler = move |msg: Message| {
            if name == "Server" {
                output.send(msg)
            } else {
                assert!(msg.as_text().unwrap() == MESSAGE);
                output.shutdown()
            }
        };

        name = "Server";

        DeflateHandler::new(handler)
    }).unwrap();

    let url = url::Url::parse("ws://127.0.0.1:3012").unwrap();

    ws.connect(url).unwrap();

    ws.listen("127.0.0.1:3012").unwrap();
}

#[test]
fn fragment() {
    env_logger::init();
    const MESSAGE: &'static str = "Hello";

    let mut name = "Client";

    let mut ws = Builder::new()
        .with_settings(Settings {
            fragment_size: 4,
            ..Default::default()
        })
        .build(|output: Sender| {
            if name == "Client" {
                output.send(MESSAGE).unwrap();
            }

            let handler = move |msg: Message| {
                if name == "Server" {
                    output.send(msg)
                } else {
                    assert!(msg.as_text().unwrap() == MESSAGE);
                    output.shutdown()
                }
            };

            name = "Server";

            DeflateHandler::new(handler)
        })
        .unwrap();

    let url = url::Url::parse("ws://127.0.0.1:3024").unwrap();

    ws.connect(url).unwrap();

    ws.listen("127.0.0.1:3024").unwrap();
}
