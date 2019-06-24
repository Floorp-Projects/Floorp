extern crate tungstenite;

use std::thread::spawn;
use std::net::TcpListener;

use tungstenite::accept_hdr;
use tungstenite::handshake::server::{Request, ErrorResponse};
use tungstenite::http::StatusCode;

fn main() {
    let server = TcpListener::bind("127.0.0.1:3012").unwrap();
    for stream in server.incoming() {
        spawn(move || {
            let callback = |_req: &Request| {
                Err(ErrorResponse {
                    error_code: StatusCode::FORBIDDEN,
                    headers: None,
                    body: Some("Access denied".into()),
                })
            };
            accept_hdr(stream.unwrap(), callback).unwrap_err();
        });
    }
}
