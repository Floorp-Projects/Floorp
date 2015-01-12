use std::io::net::ip::IpAddr;
use std::num::FromPrimitive;
use std::sync::Mutex;
use std::sync::mpsc::{channel, Receiver, Sender};
use std::thread::Thread;

use hyper::header::common::ContentLength;
use hyper::method::Method;
use hyper::server::{Server, Handler, Request, Response};
use hyper::uri::RequestUri::AbsolutePath;

use command::WebDriverMessage;
use common::WebDriverResult;
use marionette::MarionetteConnection;
use messagebuilder::{get_builder, MessageBuilder};
use response::WebDriverResponse;

enum DispatchMessage {
    HandleWebDriver(WebDriverMessage, Sender<WebDriverResult<WebDriverResponse>>),
    Quit
}

struct Dispatcher {
    connection: Option<MarionetteConnection>
}

impl Dispatcher {
    fn new() -> Dispatcher {
        Dispatcher {
            connection: None
        }
    }

    fn run(&mut self, msg_chan: Receiver<DispatchMessage>) {
        loop {
            match msg_chan.recv() {
                Ok(DispatchMessage::HandleWebDriver(msg, resp_chan)) => {
                    match msg.session_id {
                        Some(ref x) => {
                            match self.connection {
                                Some(ref conn) => {
                                    if conn.session.session_id != *x {
                                        error!("Got unexpected session id {} expected {}",
                                               x, conn.session.session_id);
                                        continue
                                    }
                                },
                                None => {
                                    match self.create_connection(Some(x.clone())) {
                                        Err(msg) => {
                                            error!("{}", msg);
                                            continue
                                        },
                                        Ok(_) => {}
                                    }
                                }
                            }
                        },
                        None => {
                            if self.connection.is_some() {
                                error!("Missing session id for established connection");
                                continue;
                            }
                            match self.create_connection(None) {
                                Err(msg) => {
                                    error!("{}", msg);
                                    continue
                                },
                                Ok(_) => {}
                            }
                        }
                    };
                    let resp = {
                        let mut connection = self.connection.as_mut().unwrap();
                        connection.send_message(&msg)
                    };
                    debug!("{:?}", resp);
                    match resp {
                        Ok(WebDriverResponse::DeleteSession) => {
                            debug!("Deleting session");
                            self.connection = None;
                        },
                        _ => {}
                    }
                    resp_chan.send(resp);
                },
                Ok(DispatchMessage::Quit) => {
                    break;
                },
                Err(_) => panic!("Error receiving message in handler")
            }
        }
    }

    fn create_connection(&mut self, session_id: Option<String>) -> Result<(), String> {
        let mut connection = MarionetteConnection::new(session_id);
        if connection.connect().is_err() {
            return Err("Failed to start marionette connection".to_string());
        }
        self.connection = Some(connection);
        Ok(())
    }
}

struct MarionetteHandler {
    chan: Mutex<Sender<DispatchMessage>>,
    builder: Mutex<MessageBuilder>
}

impl MarionetteHandler {
    fn new(builder: MessageBuilder, chan: Sender<DispatchMessage>) -> MarionetteHandler {
        MarionetteHandler {
            chan: Mutex::new(chan),
            builder: Mutex::new(builder)
        }
    }
}

impl Handler for MarionetteHandler {
    fn handle(&self, req: Request, res: Response) {
        let mut req = req;
        let mut res = res;

        let body = match req.method {
            Method::Post => req.read_to_string().unwrap(),
            _ => "".to_string()
        };
        debug!("Got request {} {:?}", req.method, req.uri);
        match req.uri {
            AbsolutePath(path) => {
                let msg_result = {
                    // The fact that this locks for basically the whole request doesn't
                    // matter as long as we are only handling one request at a time.
                    match self.builder.lock() {
                        Ok(ref builder) => {
                            builder.from_http(req.method, path.as_slice(), body.as_slice())
                        },
                        Err(_) => return
                    }
                };
                let (status, resp_body) = match msg_result {
                    Ok(message) => {
                        let (send_res, recv_res) = channel();
                        match self.chan.lock() {
                            Ok(ref c) => {
                                let res = c.send(DispatchMessage::HandleWebDriver(message,
                                                                                  send_res));
                                match res {
                                    Ok(x) => x,
                                    Err(_) => {
                                        error!("Something terrible happened");
                                        return
                                    }
                                }
                            },
                            Err(_) => {
                                error!("Something terrible happened");
                                return
                            }
                        }
                        match recv_res.recv() {
                            Ok(data) => match data {
                                Ok(response) => (200, response.to_json_string()),
                                Err(err) => (err.http_status(), err.to_json_string()),
                            },
                            Err(_) => panic!("Error reading response")
                        }
                    },
                    Err(err) => {
                        (err.http_status(), err.to_json_string())
                    }
                };
                if status != 200 {
                    error!("Returning status code {}", status);
                    error!("Returning body {}", resp_body);
                } else {
                    debug!("Returning status code {}", status);
                    debug!("Returning body {}", resp_body);
                }
                {
                    let status_code = res.status_mut();
                    *status_code = FromPrimitive::from_u32(status).unwrap();
                }
                res.headers_mut().set(ContentLength(resp_body.len() as u64));
                let mut stream = res.start();
                stream.write_str(resp_body.as_slice()).unwrap();
                stream.unwrap().end().unwrap();
            },
            _ => {}
        }
    }
}

pub fn start(ip_address: IpAddr, port: u16) {
    let server = Server::http(ip_address, port);
    let mut dispatcher = Dispatcher::new();

    let (msg_send, msg_recv) = channel();

    Thread::spawn(move || {
        dispatcher.run(msg_recv);
    });
    let builder = get_builder();
    let handler = MarionetteHandler::new(builder, msg_send.clone());
    server.listen(handler).unwrap();
}
