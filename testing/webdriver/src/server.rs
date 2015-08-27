use std::io::{Write, Read};
use std::sync::Mutex;
use std::sync::mpsc::{channel, Receiver, Sender};
use std::thread;
use std::net::SocketAddr;

use hyper::header::{ContentLength, ContentType};
use hyper::method::Method;
use hyper::server::{Server, Handler, Request, Response};
use hyper::uri::RequestUri::AbsolutePath;
use hyper::status::StatusCode;

use command::{WebDriverMessage, WebDriverCommand};
use error::{WebDriverResult, WebDriverError, ErrorStatus};
use httpapi::WebDriverHttpApi;
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
                            self.delete_session();
                        },
                        Err(ref x) if x.delete_session() => {
                            self.delete_session();
                        }
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

    fn delete_session(&mut self) {
        debug!("Deleting session");
        self.handler.delete_session(&self.session);
        self.session = None;
    }

    fn check_session(&self, msg: &WebDriverMessage) -> WebDriverResult<()> {
        match msg.session_id {
            Some(ref msg_session_id) => {
                match self.session {
                    Some(ref existing_session) => {
                        if existing_session.id != *msg_session_id {
                            Err(WebDriverError::new(
                                ErrorStatus::InvalidSessionId,
                                &format!("Got unexpected session id {} expected {}",
                                         msg_session_id,
                                         existing_session.id)[..]))
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
                                    ErrorStatus::UnsupportedOperation,
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
                                ErrorStatus::InvalidSessionId,
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
    api: Mutex<WebDriverHttpApi>
}

impl HttpHandler {
    fn new(api: WebDriverHttpApi, chan: Sender<DispatchMessage>) -> HttpHandler {
        HttpHandler {
            chan: Mutex::new(chan),
            api: Mutex::new(api)
        }
    }
}

impl Handler for HttpHandler {
    fn handle(&self, req: Request, res: Response) {
        let mut req = req;
        let mut res = res;

        let mut body = String::new();
        if let Method::Post = req.method {
            req.read_to_string(&mut body).unwrap();
        }
        debug!("Got request {} {:?}", req.method, req.uri);
        match req.uri {
            AbsolutePath(path) => {
                let msg_result = {
                    // The fact that this locks for basically the whole request doesn't
                    // matter as long as we are only handling one request at a time.
                    match self.api.lock() {
                        Ok(ref api) => {
                            api.decode_request(req.method, &path[..], &body[..])
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
                                Ok(response) => (StatusCode::Ok, response.to_json_string()),
                                Err(err) => (err.http_status(), err.to_json_string()),
                            },
                            Err(e) => panic!("Error reading response: {:?}", e)
                        }
                    },
                    Err(err) => {
                        (err.http_status(), err.to_json_string())
                    }
                };
                debug!("Returning status {:?}", status);
                debug!("Returning body {}", resp_body);
                {
                    let resp_status = res.status_mut();
                    *resp_status = status;
                }
                res.headers_mut().set(ContentLength(resp_body.len() as u64));
                res.headers_mut().set(ContentType::json());
                let mut stream = res.start().unwrap();
                stream.write_all(&resp_body.as_bytes()).unwrap();
                stream.end().unwrap();
            },
            _ => {}
        }
    }
}

pub fn start<T: 'static+WebDriverHandler>(address: SocketAddr, handler: T) {
    let (msg_send, msg_recv) = channel();

    let api = WebDriverHttpApi::new();
    let http_handler = HttpHandler::new(api, msg_send);
    let server = Server::http(address).unwrap();

    let builder = thread::Builder::new().name("webdriver dispatcher".to_string());
    builder.spawn(move || {
        let mut dispatcher = Dispatcher::new(handler);
        dispatcher.run(msg_recv)
    }).unwrap();
    server.handle(http_handler).unwrap();
}
