// Rather than grapple with #[cfg(...)] for every variable and import.
#![cfg(all(feature = "http", feature = "bhttp"))]

use bhttp::{Error, Message, Mode};
use std::{io::Cursor, mem::drop};

const CHUNKED_HTTP: &[u8] = b"HTTP/1.1 200 OK\r\n\
                              Transfer-Encoding: camel, chunked\r\n\
                              \r\n\
                              4\r\n\
                              This\r\n\
                              6\r\n \
                              conte\r\n\
                              13;chunk-extension=foo\r\n\
                              nt contains CRLF.\r\n\
                              \r\n\
                              0\r\n\
                              Trailer: text\r\n\
                              \r\n";
const TRANSFER_ENCODING: &[u8] = b"transfer-encoding";
const CHUNKED_KNOWN: &[u8] = &[
    0x01, 0x40, 0xc8, 0x00, 0x1d, 0x54, 0x68, 0x69, 0x73, 0x20, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e,
    0x74, 0x20, 0x63, 0x6f, 0x6e, 0x74, 0x61, 0x69, 0x6e, 0x73, 0x20, 0x43, 0x52, 0x4c, 0x46, 0x2e,
    0x0d, 0x0a, 0x0d, 0x07, 0x74, 0x72, 0x61, 0x69, 0x6c, 0x65, 0x72, 0x04, 0x74, 0x65, 0x78, 0x74,
];
const CHUNKED_INDEFINITE: &[u8] = &[
    0x03, 0x40, 0xc8, 0x00, 0x1d, 0x54, 0x68, 0x69, 0x73, 0x20, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e,
    0x74, 0x20, 0x63, 0x6f, 0x6e, 0x74, 0x61, 0x69, 0x6e, 0x73, 0x20, 0x43, 0x52, 0x4c, 0x46, 0x2e,
    0x0d, 0x0a, 0x00, 0x07, 0x74, 0x72, 0x61, 0x69, 0x6c, 0x65, 0x72, 0x04, 0x74, 0x65, 0x78, 0x74,
    0x00,
];

const REQUEST: &[u8] = b"GET /hello.txt HTTP/1.1\r\n\
                         user-agent: curl/7.16.3 libcurl/7.16.3 OpenSSL/0.9.7l zlib/1.2.3\r\n\
                         host: www.example.com\r\n\
                         accept-language: en, mi\r\n\
                         \r\n";
const REQUEST_KNOWN: &[u8] = &[
    0x00, 0x03, 0x47, 0x45, 0x54, 0x05, 0x68, 0x74, 0x74, 0x70, 0x73, 0x00, 0x0a, 0x2f, 0x68, 0x65,
    0x6c, 0x6c, 0x6f, 0x2e, 0x74, 0x78, 0x74, 0x40, 0x6c, 0x0a, 0x75, 0x73, 0x65, 0x72, 0x2d, 0x61,
    0x67, 0x65, 0x6e, 0x74, 0x34, 0x63, 0x75, 0x72, 0x6c, 0x2f, 0x37, 0x2e, 0x31, 0x36, 0x2e, 0x33,
    0x20, 0x6c, 0x69, 0x62, 0x63, 0x75, 0x72, 0x6c, 0x2f, 0x37, 0x2e, 0x31, 0x36, 0x2e, 0x33, 0x20,
    0x4f, 0x70, 0x65, 0x6e, 0x53, 0x53, 0x4c, 0x2f, 0x30, 0x2e, 0x39, 0x2e, 0x37, 0x6c, 0x20, 0x7a,
    0x6c, 0x69, 0x62, 0x2f, 0x31, 0x2e, 0x32, 0x2e, 0x33, 0x04, 0x68, 0x6f, 0x73, 0x74, 0x0f, 0x77,
    0x77, 0x77, 0x2e, 0x65, 0x78, 0x61, 0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x0f, 0x61,
    0x63, 0x63, 0x65, 0x70, 0x74, 0x2d, 0x6c, 0x61, 0x6e, 0x67, 0x75, 0x61, 0x67, 0x65, 0x06, 0x65,
    0x6e, 0x2c, 0x20, 0x6d, 0x69, 0x00, 0x00,
];

#[test]
fn chunked_read() {
    drop(Message::read_http(&mut Cursor::new(CHUNKED_HTTP)).unwrap());
}

#[test]
fn chunked_read_known() {
    drop(Message::read_bhttp(&mut Cursor::new(CHUNKED_KNOWN)).unwrap());
}

#[test]
fn chunked_read_indefinite() {
    drop(Message::read_bhttp(&mut Cursor::new(CHUNKED_INDEFINITE)).unwrap());
}

#[test]
fn chunked_to_known() {
    let m = Message::read_http(&mut Cursor::new(CHUNKED_HTTP)).unwrap();
    assert!(m.header().get(TRANSFER_ENCODING).is_none());

    let mut buf = Vec::new();
    m.write_bhttp(Mode::KnownLength, &mut buf).unwrap();
    println!("result: {}", hex::encode(&buf));
    assert_eq!(&buf[..], CHUNKED_KNOWN);
}

#[test]
fn chunked_to_indefinite() {
    let m = Message::read_http(&mut Cursor::new(CHUNKED_HTTP)).unwrap();
    assert!(m.header().get(TRANSFER_ENCODING).is_none());

    let mut buf = Vec::new();
    m.write_bhttp(Mode::IndefiniteLength, &mut buf).unwrap();
    println!("result: {}", hex::encode(&buf));
    assert_eq!(&buf[..], CHUNKED_INDEFINITE);
}

#[test]
fn convert_request() {
    let m = Message::read_http(&mut Cursor::new(REQUEST)).unwrap();
    let mut buf = Vec::new();
    m.write_bhttp(Mode::KnownLength, &mut buf).unwrap();
    println!("result: {}", hex::encode(&buf));
    assert_eq!(&buf[..], REQUEST_KNOWN);
}

#[test]
fn padded_to_http() {
    let mut padded = Vec::from(REQUEST_KNOWN);
    padded.resize(padded.len() + 100, 0);
    let m = Message::read_bhttp(&mut Cursor::new(&padded[..])).unwrap();
    let mut buf = Vec::new();
    m.write_http(&mut buf).unwrap();
    assert_eq!(&buf[..], REQUEST);
}

#[test]
fn truncated_to_http() {
    let mut padded = Vec::from(REQUEST_KNOWN);
    assert_eq!(2, padded.iter().rev().take_while(|&x| *x == 0).count());
    padded.truncate(padded.len() - 2);

    let m = Message::read_bhttp(&mut Cursor::new(&padded[..])).unwrap();
    let mut buf = Vec::new();
    m.write_http(&mut buf).unwrap();
    assert_eq!(&buf[..], REQUEST);
}

#[test]
fn tiny_request() {
    const REQUEST: &[u8] = &[
        0x00, 0x03, 0x47, 0x45, 0x54, 0x05, 0x68, 0x74, 0x74, 0x70, 0x73, 0x0b, 0x65, 0x78, 0x61,
        0x6d, 0x70, 0x6c, 0x65, 0x2e, 0x63, 0x6f, 0x6d, 0x01, 0x2f,
    ];
    let m = Message::read_bhttp(&mut Cursor::new(REQUEST)).unwrap();
    assert_eq!(m.control().method().unwrap(), b"GET");
    assert_eq!(m.control().scheme().unwrap(), b"https");
    assert_eq!(m.control().authority().unwrap(), b"example.com");
    assert_eq!(m.control().path().unwrap(), b"/");
    assert!(m.control().status().is_none());
    assert!(m.header().is_empty());
    assert!(m.content().is_empty());
    assert!(m.trailer().is_empty());
}

#[test]
fn tiny_response() {
    const RESPONSE: &[u8] = &[0x01, 0x40, 0xc8];
    let m = Message::read_bhttp(&mut Cursor::new(RESPONSE)).unwrap();
    assert!(m.informational().is_empty());
    assert_eq!(m.control().status().unwrap(), 200);
    assert!(m.control().method().is_none());
    assert!(m.control().scheme().is_none());
    assert!(m.control().authority().is_none());
    assert!(m.control().path().is_none());
    assert!(m.header().is_empty());
    assert!(m.content().is_empty());
    assert!(m.trailer().is_empty());
}

#[test]
fn connect_request() {
    const REQUEST: &[u8] = b"CONNECT test.example HTTP/1.1\r\n\
                             Host: example.com\r\n\
                             \r\n";
    let err = Message::read_http(&mut Cursor::new(REQUEST)).unwrap_err();
    assert!(matches!(err, Error::ConnectUnsupported));
}

/// Verify that Connection and Proxy-Connection are stripped out properly.
#[test]
fn connection_header() {
    const REQUEST: &[u8] = b"POST test.example HTTP/1.1\r\n\
                             Host: example.com\r\n\
                             other: test\r\n\
                             Connection: sample\r\n\
                             Connection: other, garbage\r\n\
                             sample: test2\r\n\
                             px: test3\r\n\
                             proXy-connection: px\r\n\
                             \r\n";

    let m = Message::read_http(&mut Cursor::new(REQUEST)).unwrap();
    assert!(m.header().get(b"other").is_none());
    assert!(m.header().get(b"sample").is_none());
    assert!(m.header().get(b"garbage").is_none());
    assert!(m.header().get(b"connection").is_none());
    assert!(m.header().get(b"proxy-connection").is_none());
    assert!(m.header().get(b"px").is_none());
}

/// Verify that hop-by-hop headers (other than transfer-encoding) are stripped out properly.
#[test]
fn hop_by_hop() {
    const REQUEST: &[u8] = b"POST test.example HTTP/1.1\r\n\
                             Host: example.com\r\n\
                             keep-alive: 1\r\n\
                             te: trailers\r\n\
                             trailer: te\r\n\
                             upgrade: h2c\r\n\
                             \r\n";

    let m = Message::read_http(&mut Cursor::new(REQUEST)).unwrap();
    assert!(m.header().get(b"keep-alive").is_none());
    assert!(m.header().get(b"te").is_none());
    assert!(m.header().get(b"trailer").is_none());
    assert!(m.header().get(b"transfer-encoding").is_none());
    assert!(m.header().get(b"upgrade").is_none());
}

/// Verify that very bad chunked encoding produces a result.
#[test]
fn bad_chunked() {
    const REQUEST: &[u8] = b"POST test.example HTTP/1.1\r\n\
                             Transfer-Encoding: chunked\r\n\
                             \r\n";

    let e = Message::read_http(&mut Cursor::new(REQUEST)).unwrap_err();
    assert!(matches!(e, Error::Truncated));
}

/// If a length field overruns the buffer, stop.
#[test]
fn oversized() {
    const REQUEST: &[u8] = &[0x00, 0x01];
    let e = Message::read_bhttp(&mut Cursor::new(REQUEST)).unwrap_err();
    assert!(matches!(e, Error::Truncated));
}

/// If a length field overruns the buffer, stop before over-allocating.
#[test]
fn oversized_max() {
    const REQUEST: &[u8] = &[0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff];
    let e = Message::read_bhttp(&mut Cursor::new(REQUEST)).unwrap_err();
    assert!(matches!(e, Error::Truncated));
}
