//! Lightweight, flexible WebSockets for Rust.
#![deny(
    missing_docs,
    missing_copy_implementations,
    missing_debug_implementations,
    trivial_casts, trivial_numeric_casts,
    unstable_features,
    unused_must_use,
    unused_mut,
    unused_imports,
    unused_import_braces)]

#[macro_use] extern crate log;
extern crate base64;
extern crate byteorder;
extern crate bytes;
extern crate httparse;
extern crate input_buffer;
extern crate rand;
extern crate sha1;
extern crate url;
extern crate utf8;
#[cfg(feature="tls")] extern crate native_tls;

pub extern crate http;

pub mod error;
pub mod protocol;
pub mod client;
pub mod server;
pub mod handshake;
pub mod stream;
pub mod util;

pub use client::{connect, client};
pub use server::{accept, accept_hdr};
pub use error::{Error, Result};
pub use protocol::{WebSocket, Message};
pub use handshake::HandshakeError;
pub use handshake::client::ClientHandshake;
pub use handshake::server::ServerHandshake;
