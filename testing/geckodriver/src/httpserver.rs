use std::io::net::ip::IpAddr;
use std::sync::Mutex;
use std::collections::HashMap;
use serialize::json::ToJson;

use hyper::header::common::ContentLength;
use hyper::method::Post;
use hyper::server::{Server, Handler, Request, Response};
use hyper::uri::AbsolutePath;

use response::WebDriverResponse;
use messagebuilder::{get_builder};
use marionette::MarionetteConnection;
use command::WebDriverMessage;
use common::WebDriverResult;

enum DispatchMessage {
    HandleWebDriver(WebDriverMessage, Sender<Option<WebDriverResult<WebDriverResponse>>>),
    Quit
}

struct Dispatcher {
    connections: HashMap<String, MarionetteConnection>
}

impl Dispatcher {
    fn new() -> Dispatcher {
        Dispatcher {
            connections: HashMap::new()
        }
    }

    fn run(&mut self, msg_chan: Receiver<DispatchMessage>) {
        loop {
            match msg_chan.recv() {
                DispatchMessage::HandleWebDriver(msg, resp_chan) => {
                    let opt_session_id = msg.session_id.clone();
                    if opt_session_id.is_some() {
                        let session_id = opt_session_id.unwrap();
                        let mut connection = match self.connections.get_mut(&session_id) {
                            Some(x) => x,
                            None => break
                        };
                        let resp = connection.send_message(&msg);
                        resp_chan.send(resp);
                        return;
                    }
                    let mut connection = MarionetteConnection::new();
                    if connection.connect().is_err() {
                        error!("Failed to start marionette connection");
                        return
                    }
                    let resp = connection.send_message(&msg);
                    self.connections.insert(connection.session.session_id.clone(),
                                            connection);
                    resp_chan.send(resp);
                },
                DispatchMessage::Quit => {
                    break;
                }
            }
        }
    }
}

struct MarionetteHandler {
    chan: Mutex<Sender<DispatchMessage>>
}

impl MarionetteHandler {
    fn new(chan: Sender<DispatchMessage>) -> MarionetteHandler {
        MarionetteHandler {
            chan: Mutex::new(chan)
        }
    }
}

impl Handler for MarionetteHandler {
    fn handle(&self, req: Request, res: Response) {
        let builder = get_builder();
        println!("{}", req.uri);;

        let mut req = req;
        let mut res = res;

        let body = match req.method {
            Post => req.read_to_string().unwrap(),
            _ => "".to_string()
        };
        println!("Got request {} {}", req.method, req.uri);
        match req.uri {
            AbsolutePath(path) => {
                let (status, resp_data) = match builder.from_http(req.method, path[], body[]) {
                    Ok(message) => {
                        let (send_res, recv_res) = channel();
                        {
                            let c = self.chan.lock();
                            c.send(DispatchMessage::HandleWebDriver(message, send_res));
                        }
                        match recv_res.recv() {
                            Some(x) => {
                                match x {
                                    Ok(response) => {
                                        (200, response.to_json())
                                    }
                                    Err(err) => (err.http_status(), err.to_json())
                                }
                            },
                            None => return
                        }
                    },
                    Err(err) => {
                        (err.http_status(), err.to_json())
                    }
                };
                let body = format!("{}\n", resp_data.to_string());
                {
                    let status_code = res.status_mut();
                    *status_code = FromPrimitive::from_int(status).unwrap();
                }
                res.headers_mut().set(ContentLength(body.len()));
                let mut stream = res.start();
                stream.write_str(body.as_slice());
                stream.unwrap().end();
            },
            _ => {}
        }
    }
}

pub fn start(ip_address: IpAddr, port: u16) {
    let server = Server::http(ip_address, port);
    let mut dispatcher = Dispatcher::new();

    let (msg_send, msg_recv) = channel();

    spawn(proc() {
        dispatcher.run(msg_recv);
    });
    let handler = MarionetteHandler::new(msg_send.clone());
    server.listen(handler).unwrap();
}
