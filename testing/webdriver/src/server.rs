use std::io::Read;
use std::marker::PhantomData;
use std::net::SocketAddr;
use std::sync::mpsc::{channel, Receiver, Sender};
use std::sync::Mutex;
use std::thread;

use hyper::header::ContentType;
use hyper::method::Method;
use hyper::Result;
use hyper::server::{Handler, Listening, Request, Response, Server};
use hyper::status::StatusCode;
use hyper::uri::RequestUri::AbsolutePath;

use command::{WebDriverMessage, WebDriverCommand};
use error::{WebDriverResult, WebDriverError, ErrorStatus};
use httpapi::{WebDriverHttpApi, WebDriverExtensionRoute, VoidWebDriverExtensionRoute};
use response::WebDriverResponse;

enum DispatchMessage<U: WebDriverExtensionRoute> {
    HandleWebDriver(WebDriverMessage<U>, Sender<WebDriverResult<WebDriverResponse>>),
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

pub trait WebDriverHandler<U: WebDriverExtensionRoute=VoidWebDriverExtensionRoute> : Send {
    fn handle_command(&mut self, session: &Option<Session>, msg: WebDriverMessage<U>) -> WebDriverResult<WebDriverResponse>;
    fn delete_session(&mut self, session: &Option<Session>);
}

struct Dispatcher<T: WebDriverHandler<U>,
                  U: WebDriverExtensionRoute> {
    handler: T,
    session: Option<Session>,
    extension_type: PhantomData<U>,
}

impl <T: WebDriverHandler<U>,
      U: WebDriverExtensionRoute> Dispatcher<T,U> {
    fn new(handler: T) -> Dispatcher<T,U> {
        Dispatcher {
            handler: handler,
            session: None,
            extension_type: PhantomData
        }
    }

    fn run(&mut self, msg_chan: Receiver<DispatchMessage<U>>) {
        loop {
            match msg_chan.recv() {
                Ok(DispatchMessage::HandleWebDriver(msg, resp_chan)) => {
                    let resp = match self.check_session(&msg) {
                        Ok(_) => self.handler.handle_command(&self.session, msg),
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

    fn check_session(&self, msg: &WebDriverMessage<U>) -> WebDriverResult<()> {
        match msg.session_id {
            Some(ref msg_session_id) => {
                match self.session {
                    Some(ref existing_session) => {
                        if existing_session.id != *msg_session_id {
                            Err(WebDriverError::new(
                                ErrorStatus::InvalidSessionId,
                                format!("Got unexpected session id {} expected {}",
                                        msg_session_id,
                                        existing_session.id)))
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
                            WebDriverCommand::Status => Ok(()),
                            WebDriverCommand::NewSession(_) => {
                                Err(WebDriverError::new(
                                    ErrorStatus::SessionNotCreated,
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
                            WebDriverCommand::NewSession(_) => Ok(()),

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

struct HttpHandler<U: WebDriverExtensionRoute> {
    chan: Mutex<Sender<DispatchMessage<U>>>,
    api: Mutex<WebDriverHttpApi<U>>
}

impl <U: WebDriverExtensionRoute> HttpHandler<U> {
    fn new(api: WebDriverHttpApi<U>, chan: Sender<DispatchMessage<U>>) -> HttpHandler<U> {
        HttpHandler {
            chan: Mutex::new(chan),
            api: Mutex::new(api)
        }
    }
}

impl <U: WebDriverExtensionRoute> Handler for HttpHandler<U> {
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
                res.headers_mut().set(ContentType::json());
                res.send(&resp_body.as_bytes()).unwrap();
            },
            _ => {}
        }
    }
}

pub fn start<T, U>(address: SocketAddr,
                   handler: T,
                   extension_routes: &[(Method, &str, U)])
                   -> Result<Listening>
    where T: 'static + WebDriverHandler<U>,
          U: 'static + WebDriverExtensionRoute
{
    let (msg_send, msg_recv) = channel();

    let api = WebDriverHttpApi::new(extension_routes);
    let http_handler = HttpHandler::new(api, msg_send);
    let mut server = try!(Server::http(address));
    server.keep_alive(None);

    let builder = thread::Builder::new().name("webdriver dispatcher".to_string());
    try!(builder.spawn(move || {
        let mut dispatcher = Dispatcher::new(handler);
        dispatcher.run(msg_recv);
    }));

    server.handle(http_handler)
}
