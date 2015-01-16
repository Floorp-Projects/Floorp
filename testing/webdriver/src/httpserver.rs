use std::io::net::ip::IpAddr;
use std::num::FromPrimitive;
use std::sync::Mutex;
use std::sync::mpsc::{channel, Receiver, Sender};
use std::thread::Thread;

use hyper::header::common::ContentLength;
use hyper::method::Method;
use hyper::server::{Server, Handler, Request, Response};
use hyper::uri::RequestUri::AbsolutePath;

use command::{WebDriverMessage, WebDriverCommand};
use error::{WebDriverResult, WebDriverError, ErrorStatus};
use messagebuilder::{get_builder, MessageBuilder};
use response::WebDriverResponse;

enum DispatchMessage {
    HandleWebDriver(WebDriverMessage, Sender<WebDriverResult<WebDriverResponse>>),
    Quit
}

#[derive(PartialEq, Clone)]
pub struct Session {
    id: String
}

impl Session {
    fn new(id: String) -> Session {
        Session {
            id: id
        }
    }
}

pub trait WebDriverHandler : Send {
    fn handle_command(&mut self, session: &Option<Session>, msg: &WebDriverMessage) -> WebDriverResult<WebDriverResponse>;
    fn delete_session(&mut self, session: &Option<Session>);
}

struct Dispatcher<T: WebDriverHandler> {
    handler: T,
    session: Option<Session>
}

impl<T: WebDriverHandler> Dispatcher<T> {
    fn new(handler: T) -> Dispatcher<T> {
        Dispatcher {
            handler: handler,
            session: None
        }
    }

    fn run(&mut self, msg_chan: Receiver<DispatchMessage>) {
        loop {
            match msg_chan.recv() {
                Ok(DispatchMessage::HandleWebDriver(msg, resp_chan)) => {
                    let resp = match self.check_session(&msg) {
                        Ok(_) => self.handler.handle_command(&self.session, &msg),
                        Err(e) => Err(e)
                    };

                    match resp {
                        Ok(WebDriverResponse::NewSession(ref new_session)) => {
                            self.session = Some(Session::new(new_session.sessionId.clone()));
                        },
                        Ok(WebDriverResponse::DeleteSession) => {
                            debug!("Deleting session");
                            self.handler.delete_session(&self.session);
                            self.session = None;
                        },
                        _ => {}
                    }

                    if resp_chan.send(resp).is_err() {
                        error!("Sending response to the main thread failed");
                    };
                },
                Ok(DispatchMessage::Quit) => {
                    break;
                },
                Err(_) => panic!("Error receiving message in handler")
            }
        }
    }

    fn check_session(&self, msg: &WebDriverMessage) -> WebDriverResult<()> {
        match msg.session_id {
            Some(ref msg_session_id) => {
                match self.session {
                    Some(ref existing_session) => {
                        if existing_session.id != *msg_session_id {
                            Err(WebDriverError::new(
                                ErrorStatus::InvalidSessionId,
                                format!("Got unexpected session id {} expected {}",
                                        msg_session_id,
                                        existing_session.id).as_slice()))
                        } else {
                            Ok(())
                        }
                    },
                    None => Ok(())
                }
            },
            None => {
                match self.session {
                    Some(_) => {
                        match msg.command {
                            WebDriverCommand::NewSession => {
                                Err(WebDriverError::new(
                                    ErrorStatus::UnknownError,
                                    "Session is already started"))
                            },
                            _ => {
                                //This should be impossible
                                error!("Got a message with no session id");
                                Err(WebDriverError::new(
                                    ErrorStatus::UnknownError,
                                    "Got a command with no session?!"))
                            }
                        }
                    },
                    None => {
                        match msg.command {
                            WebDriverCommand::NewSession => Ok(()),

                            _ => Err(WebDriverError::new(
                                ErrorStatus::SessionNotCreated,
                                "Tried to run a command before creating a session"))
                        }
                    }
                }
            }
        }
    }
}

struct HttpHandler {
    chan: Mutex<Sender<DispatchMessage>>,
    builder: Mutex<MessageBuilder>
}

impl HttpHandler {
    fn new(builder: MessageBuilder, chan: Sender<DispatchMessage>) -> HttpHandler {
        HttpHandler {
            chan: Mutex::new(chan),
            builder: Mutex::new(builder)
        }
    }
}

impl Handler for HttpHandler {
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

pub fn start<T: WebDriverHandler>(ip_address: IpAddr, port: u16, handler: T) {
    let server = Server::http(ip_address, port);

    let (msg_send, msg_recv) = channel();

    Thread::spawn(move || {
        let mut dispatcher = Dispatcher::new(handler);
        dispatcher.run(msg_recv)
    });
    let builder = get_builder();
    let http_handler = HttpHandler::new(builder, msg_send.clone());
    server.listen(http_handler).unwrap();
}
