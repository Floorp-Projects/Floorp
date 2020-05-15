//! The `hawk` crate provides support for [Hawk](https://github.com/hueniverse/hawk)
//! authentictation. It is a low-level crate, used by higher-level crates to integrate with various
//! Rust HTTP libraries.  For example `hyper-hawk` integrates Hawk with Hyper.
//!
//! # Examples
//!
//! ## Hawk Client
//!
//! A client can attach a Hawk Authorization header to requests by providing credentials to a
//! Request instance, which will generate the header.
//!
//! ```
//! use hawk::{RequestBuilder, Credentials, Key, SHA256, PayloadHasher};
//! use std::time::{Duration, UNIX_EPOCH};
//!
//! fn main() {
//!     // provide the Hawk id and key
//!     let credentials = Credentials {
//!         id: "test-client".to_string(),
//!         key: Key::new(vec![99u8; 32], SHA256).unwrap(),
//!     };
//!
//!     let payload_hash = PayloadHasher::hash("text/plain", SHA256, "request-body").unwrap();
//!
//!     // provide the details of the request to be authorized
//!      let request = RequestBuilder::new("POST", "example.com", 80, "/v1/users")
//!         .hash(&payload_hash[..])
//!         .request();
//!
//!     // Get the resulting header, including the calculated MAC; this involves a random
//!     // nonce, so the MAC will be different on every request.
//!     let header = request.make_header(&credentials).unwrap();
//!
//!     // the header would the be attached to the request
//!     assert_eq!(header.id.unwrap(), "test-client");
//!     assert_eq!(header.mac.unwrap().len(), 32);
//!     assert_eq!(header.hash.unwrap().len(), 32);
//! }
//! ```
//!
//! A client that wishes to use a bewit (URL parameter) can do so as follows:
//!
//! ```
//! use hawk::{RequestBuilder, Credentials, Key, SHA256, Bewit};
//! use std::time::Duration;
//! use std::borrow::Cow;
//!
//! let credentials = Credentials {
//!     id: "me".to_string(),
//!     key: Key::new("tok", SHA256).unwrap(),
//! };
//!
//! let client_req = RequestBuilder::new("GET", "mysite.com", 443, "/resource").request();
//! let client_bewit = client_req
//!     .make_bewit_with_ttl(&credentials, Duration::from_secs(10))
//!     .unwrap();
//! let request_path = format!("/resource?bewit={}", client_bewit.to_str());
//! // .. make the request
//! ```
//!
//! ## Hawk Server
//!
//! To act as a server, parse the Hawk Authorization header from the request, generate a new
//! Request instance, and use the request to validate the header.
//!
//! ```
//! use hawk::{RequestBuilder, Header, Key, SHA256};
//! use hawk::mac::Mac;
//! use std::time::{Duration, UNIX_EPOCH};
//!
//! fn main() {
//!    let mac = Mac::from(vec![7, 22, 226, 240, 84, 78, 49, 75, 115, 144, 70,
//!                             106, 102, 134, 144, 128, 225, 239, 95, 132, 202,
//!                             154, 213, 118, 19, 63, 183, 108, 215, 134, 118, 115]);
//!    // get the header (usually from the received request; constructed directly here)
//!    let hdr = Header::new(Some("dh37fgj492je"),
//!                          Some(UNIX_EPOCH + Duration::new(1353832234, 0)),
//!                          Some("j4h3g2"),
//!                          Some(mac),
//!                          Some("my-ext-value"),
//!                          Some(vec![1, 2, 3, 4]),
//!                          Some("my-app"),
//!                          Some("my-dlg")).unwrap();
//!
//!    // build a request object based on what we know
//!    let hash = vec![1, 2, 3, 4];
//!    let request = RequestBuilder::new("GET", "localhost", 443, "/resource")
//!        .hash(&hash[..])
//!        .request();
//!
//!    let key = Key::new(vec![99u8; 32], SHA256).unwrap();
//!    let one_week_in_secs = 7 * 24 * 60 * 60;
//!    if !request.validate_header(&hdr, &key, Duration::from_secs(5200 * one_week_in_secs)) {
//!        panic!("header validation failed. Is it 2117 already?");
//!    }
//! }
//! ```
//!
//! A server which validates bewits looks like this:
//!
//! ```
//! use hawk::{RequestBuilder, Credentials, Key, SHA256, Bewit};
//! use std::time::Duration;
//! use std::borrow::Cow;
//!
//! let credentials = Credentials {
//!     id: "me".to_string(),
//!     key: Key::new("tok", SHA256).unwrap(),
//! };
//!
//! // simulate the client generation of a bewit
//! let client_req = RequestBuilder::new("GET", "mysite.com", 443, "/resource").request();
//! let client_bewit = client_req
//!     .make_bewit_with_ttl(&credentials, Duration::from_secs(10))
//!     .unwrap();
//! let request_path = format!("/resource?bewit={}", client_bewit.to_str());
//!
//! let mut maybe_bewit = None;
//! let server_req = RequestBuilder::new("GET", "mysite.com", 443, &request_path)
//!     .extract_bewit(&mut maybe_bewit).unwrap()
//!     .request();
//! let bewit = maybe_bewit.unwrap();
//! assert_eq!(bewit.id(), "me");
//! assert!(server_req.validate_bewit(&bewit, &credentials.key));
//! ```
//!
//! ## Features
//!
//! By default, the `use_ring` feature is enabled, which means that this crate will
//! use `ring` for all cryptographic operations.
//!
//! Alternatively, one can configure the crate with the `use_openssl`
//! feature to use the `openssl` crate.
//!
//! If no features are enabled, you must provide a custom implementation of the
//! [`hawk::crypto::Cryptographer`] trait to the `set_cryptographer` function, or
//! the cryptographic operations will panic.
//!
//! Attempting to configure both the `use_ring` and `use_openssl` features will
//! result in a build error.

#[cfg(test)]
#[macro_use]
extern crate pretty_assertions;

mod header;
pub use crate::header::Header;

mod credentials;
pub use crate::credentials::{Credentials, DigestAlgorithm, Key};

mod request;
pub use crate::request::{Request, RequestBuilder};

mod response;
pub use crate::response::{Response, ResponseBuilder};

mod error;
pub use crate::error::*;

mod payload;
pub use crate::payload::PayloadHasher;

mod bewit;
pub use crate::bewit::Bewit;

pub mod mac;

pub mod crypto;

pub const SHA256: DigestAlgorithm = DigestAlgorithm::Sha256;
pub const SHA384: DigestAlgorithm = DigestAlgorithm::Sha384;
pub const SHA512: DigestAlgorithm = DigestAlgorithm::Sha512;
