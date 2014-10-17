use std::io::net::ip::IpAddr;
use regex::Regex;

use serialize::json;

use hyper;
use hyper::header::common::ContentLength;
use hyper::server::{Server, Incoming};
use hyper::uri::AbsolutePath;

use messagebuilder::{get_builder};
use marionette::{MarionetteSession, MarionetteConnection};

fn handle(mut incoming: Incoming) {
    let mut marionette = MarionetteConnection::new();
    marionette.connect();

    let builder = get_builder();
    for (mut req, mut resp) in incoming {
        println!("{}", req.uri);
        let body = req.read_to_string().unwrap();
        match req.uri {
            AbsolutePath(path) => {
                let message = builder.from_http(req.method, path[], body[]);
                //Should return a Result instead
                if message.is_some() {
                    let response = marionette.send_message(&message.unwrap());
                    if response.is_some() {
                        let body = response.unwrap().to_json().to_string();
                        resp.headers_mut().set(ContentLength(body.len()));
                        let mut stream = resp.start();
                        stream.write_str(body.as_slice());
                        stream.unwrap().end();
                    }
                }
            },
            _ => {}
        };
    }
}

pub fn start(ip_address: IpAddr, port: u16) {
    let server = Server::http(ip_address, port);
    server.listen(handle).unwrap();
}
