use std::io::net::ip::IpAddr;
use std::sync::Mutex;

use hyper::header::common::ContentLength;
use hyper::method::Post;
use hyper::server::{Server, Handler, Request, Response};
use hyper::uri::AbsolutePath;

use response::WebDriverResponse;
use messagebuilder::{get_builder, MessageBuilder};
use marionette::MarionetteConnection;
use command::WebDriverMessage;
use common::WebDriverResult;

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
                DispatchMessage::HandleWebDriver(msg, resp_chan) => {
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
                    debug!("{}", resp);
                    match resp {
                        Ok(WebDriverResponse::DeleteSession) => {
                            debug!("Deleting session");
                            self.connection = None;
                        },
                        _ => {}
                    }
                    resp_chan.send(resp);
                },
                DispatchMessage::Quit => {
                    break;
                }
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
            Post => req.read_to_string().unwrap(),
            _ => "".to_string()
        };
        debug!("Got request {} {}", req.method, req.uri);
        match req.uri {
            AbsolutePath(path) => {
                let msg_result = {
                    // The fact that this locks for basically the whole request doesn't
                    // matter as long as we are only handling one request at a time.
                    let builder = self.builder.lock();
                    builder.from_http(req.method, path[], body[])
                };
                let (status, resp_body) = match msg_result {
                    Ok(message) => {
                        let (send_res, recv_res) = channel();
                        {
                            let c = self.chan.lock();
                            c.send(DispatchMessage::HandleWebDriver(message, send_res));
                        }
                        match recv_res.recv() {
                            Ok(response) => (200, response.to_json_string()),
                            Err(err) => (err.http_status(), err.to_json_string()),
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
                    *status_code = FromPrimitive::from_int(status).unwrap();
                }
                res.headers_mut().set(ContentLength(resp_body.len()));
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

    spawn(proc() {
        dispatcher.run(msg_recv);
    });
    let builder = get_builder();
    let handler = MarionetteHandler::new(builder, msg_send.clone());
    server.listen(handler).unwrap();
}
