use std::io::net::ip::IpAddr;
use regex::Regex;

use serialize::json;

use hyper;
use hyper::status::StatusCode;
use hyper::header::common::ContentLength;
use hyper::method::Post;
use hyper::server::{Server, Incoming};
use hyper::uri::AbsolutePath;

use common::WebDriverResult;
use response::WebDriverResponse;
use messagebuilder::{get_builder};
use marionette::MarionetteConnection;

fn handle(mut incoming: Incoming) {
    let mut marionette = MarionetteConnection::new();
    if marionette.connect().is_err() {
        fail!("Failed to connect to marionette. Start marionette client before proxy");
    };

    let builder = get_builder();
    for (mut req, mut resp) in incoming {
        println!("{}", req.uri);;
        let body = match req.method {
            Post => req.read_to_string().unwrap(),
            _ => "".to_string()
        };
        match req.uri {
            AbsolutePath(path) => {
                let (status, resp_data) = match builder.from_http(req.method, path[], body[]) {
                    Ok(message) => {
                        match marionette.send_message(&message) {
                            Ok(response) => {
                                if response.is_none() {
                                    continue;
                                }
                                (200, response.unwrap())
                            }
                            Err(err) => (err.http_status(), WebDriverResponse::from_err(&err))
                        }
                    },
                    Err(err) => {
                        (err.http_status(), WebDriverResponse::from_err(&err))
                    }
                };
                let body = resp_data.to_json().to_string();
                {
                    let mut status_code = resp.status_mut();
                    *status_code = FromPrimitive::from_int(status).unwrap();
                }
                resp.headers_mut().set(ContentLength(body.len()));
                let mut stream = resp.start();
                stream.write_str(body.as_slice());
                stream.unwrap().end();
            },
            _ => {}
        };
    }
}

pub fn start(ip_address: IpAddr, port: u16) {
    let server = Server::http(ip_address, port);
    server.listen(handle).unwrap();
}
