use serde_json;
use std::marker::PhantomData;
use std::net::{SocketAddr, TcpListener as StdTcpListener};
use std::sync::mpsc::{channel, Receiver, Sender};
use std::sync::{Arc, Mutex};
use std::thread;

use futures::{future, Future, Stream};
use hyper::{self, Body, Method, Request, Response, StatusCode};
use hyper::service::Service;
use hyper::server::conn::Http;
use http;
use tokio::runtime::current_thread::Runtime;
use tokio::reactor::Handle;
use tokio::net::TcpListener;

use command::{WebDriverCommand, WebDriverMessage};
use error::{ErrorStatus, WebDriverError, WebDriverResult};
use httpapi::{VoidWebDriverExtensionRoute, WebDriverExtensionRoute, WebDriverHttpApi};
use response::{CloseWindowResponse, WebDriverResponse};

enum DispatchMessage<U: WebDriverExtensionRoute> {
    HandleWebDriver(
        WebDriverMessage<U>,
        Sender<WebDriverResult<WebDriverResponse>>,
    ),
    Quit,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Session {
    pub id: String,
}

impl Session {
    fn new(id: String) -> Session {
        Session { id }
    }
}

pub trait WebDriverHandler<U: WebDriverExtensionRoute = VoidWebDriverExtensionRoute>: Send {
    fn handle_command(
        &mut self,
        session: &Option<Session>,
        msg: WebDriverMessage<U>,
    ) -> WebDriverResult<WebDriverResponse>;
    fn delete_session(&mut self, session: &Option<Session>);
}

#[derive(Debug)]
struct Dispatcher<T: WebDriverHandler<U>, U: WebDriverExtensionRoute> {
    handler: T,
    session: Option<Session>,
    extension_type: PhantomData<U>,
}

impl<T: WebDriverHandler<U>, U: WebDriverExtensionRoute> Dispatcher<T, U> {
    fn new(handler: T) -> Dispatcher<T, U> {
        Dispatcher {
            handler,
            session: None,
            extension_type: PhantomData,
        }
    }

    fn run(&mut self, msg_chan: &Receiver<DispatchMessage<U>>) {
        loop {
            match msg_chan.recv() {
                Ok(DispatchMessage::HandleWebDriver(msg, resp_chan)) => {
                    let resp = match self.check_session(&msg) {
                        Ok(_) => self.handler.handle_command(&self.session, msg),
                        Err(e) => Err(e),
                    };

                    match resp {
                        Ok(WebDriverResponse::NewSession(ref new_session)) => {
                            self.session = Some(Session::new(new_session.session_id.clone()));
                        }
                        Ok(WebDriverResponse::CloseWindow(CloseWindowResponse(ref handles))) => {
                            if handles.is_empty() {
                                debug!("Last window was closed, deleting session");
                                self.delete_session();
                            }
                        }
                        Ok(WebDriverResponse::DeleteSession) => self.delete_session(),
                        Err(ref x) if x.delete_session => self.delete_session(),
                        _ => {}
                    }

                    if resp_chan.send(resp).is_err() {
                        error!("Sending response to the main thread failed");
                    };
                }
                Ok(DispatchMessage::Quit) => break,
                Err(e) => panic!("Error receiving message in handler: {:?}", e),
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
            Some(ref msg_session_id) => match self.session {
                Some(ref existing_session) => {
                    if existing_session.id != *msg_session_id {
                        Err(WebDriverError::new(
                            ErrorStatus::InvalidSessionId,
                            format!("Got unexpected session id {}", msg_session_id),
                        ))
                    } else {
                        Ok(())
                    }
                }
                None => Ok(()),
            },
            None => {
                match self.session {
                    Some(_) => {
                        match msg.command {
                            WebDriverCommand::Status => Ok(()),
                            WebDriverCommand::NewSession(_) => Err(WebDriverError::new(
                                ErrorStatus::SessionNotCreated,
                                "Session is already started",
                            )),
                            _ => {
                                //This should be impossible
                                error!("Got a message with no session id");
                                Err(WebDriverError::new(
                                    ErrorStatus::UnknownError,
                                    "Got a command with no session?!",
                                ))
                            }
                        }
                    }
                    None => match msg.command {
                        WebDriverCommand::NewSession(_) => Ok(()),
                        WebDriverCommand::Status => Ok(()),
                        _ => Err(WebDriverError::new(
                            ErrorStatus::InvalidSessionId,
                            "Tried to run a command before creating a session",
                        )),
                    },
                }
            }
        }
    }
}

#[derive(Debug, Clone)]
struct HttpHandler<U: WebDriverExtensionRoute> {
    chan: Arc<Mutex<Sender<DispatchMessage<U>>>>,
    api: Arc<Mutex<WebDriverHttpApi<U>>>,
}

impl<U: WebDriverExtensionRoute> HttpHandler<U> {
    fn new(
        api: Arc<Mutex<WebDriverHttpApi<U>>>,
        chan: Sender<DispatchMessage<U>>,
    ) -> HttpHandler<U> {
        HttpHandler {
            chan: Arc::new(Mutex::new(chan)),
            api,
        }
    }
}

impl<U: WebDriverExtensionRoute + 'static> Service for HttpHandler<U> {
    type ReqBody = Body;
    type ResBody = Body;

    type Error = hyper::Error;
    type Future = Box<future::Future<Item = Response<Self::ResBody>, Error = hyper::Error> + Send>;

    fn call(&mut self, req: Request<Self::ReqBody>) -> Self::Future {
        let uri = req.uri().clone();
        let method = req.method().clone();
        let api = self.api.clone();
        let chan = self.chan.clone();

        Box::new(req.into_body().concat2().and_then(move |body| {
            let body = String::from_utf8(body.to_vec()).unwrap();
            debug!("-> {} {} {}", method, uri, body);

            let msg_result = {
                // The fact that this locks for basically the whole request doesn't
                // matter as long as we are only handling one request at a time.
                match api.lock() {
                    Ok(ref api) => api.decode_request(&method, &uri.path(), &body[..]),
                    Err(e) => panic!("Error decoding request: {:?}", e),
                }
            };

            let (status, resp_body) = match msg_result {
                Ok(message) => {
                    let (send_res, recv_res) = channel();
                    match chan.lock() {
                        Ok(ref c) => {
                            let res = c.send(DispatchMessage::HandleWebDriver(message, send_res));
                            match res {
                                Ok(x) => x,
                                Err(e) => panic!("Error: {:?}", e),
                            }
                        }
                        Err(e) => panic!("Error reading response: {:?}", e),
                    }

                    match recv_res.recv() {
                        Ok(data) => match data {
                            Ok(response) => {
                                (StatusCode::OK, serde_json::to_string(&response).unwrap())
                            }
                            Err(e) => (e.http_status(), serde_json::to_string(&e).unwrap()),
                        },
                        Err(e) => panic!("Error reading response: {:?}", e),
                    }
                }
                Err(e) => (e.http_status(), serde_json::to_string(&e).unwrap()),
            };

            debug!("<- {} {}", status, resp_body);

            let response = Response::builder()
                .status(status)
                .header(http::header::CONTENT_TYPE, "application/json; charset=utf8")
                .header(http::header::CACHE_CONTROL, "no-cache")
                .body(resp_body.into())
                .unwrap();

            Ok(response)
        }))
    }
}

pub struct Listener {
    _guard: Option<thread::JoinHandle<()>>,
    pub socket: SocketAddr,
}

impl Drop for Listener {
    fn drop(&mut self) {
        let _ = self._guard.take().map(|j| j.join());
    }
}

pub fn start<T, U>(
    address: SocketAddr,
    handler: T,
    extension_routes: &[(Method, &str, U)],
) -> ::std::io::Result<Listener>
where
    T: 'static + WebDriverHandler<U>,
    U: 'static + WebDriverExtensionRoute,
{
    let listener = StdTcpListener::bind(address)?;
    let addr = listener.local_addr()?;
    let (msg_send, msg_recv) = channel();

    let api = Arc::new(Mutex::new(WebDriverHttpApi::new(extension_routes)));

    let builder = thread::Builder::new().name("webdriver server".to_string());
    let handle = builder.spawn(move || {
        let mut rt = Runtime::new().unwrap();
        let listener = TcpListener::from_std(listener, &Handle::default()).unwrap();

        let http_handler = HttpHandler::new(api, msg_send.clone());
        let http = Http::new();
        let handle = rt.handle();

        let fut = listener.incoming()
            .for_each(move |socket| {
                let fut = http.serve_connection(socket, http_handler.clone()).map_err(|_| ());
                handle.spawn(fut).unwrap();
                Ok(())
            });

        rt.block_on(fut).unwrap();
    })?;

    let builder = thread::Builder::new().name("webdriver dispatcher".to_string());
    builder.spawn(move || {
        let mut dispatcher = Dispatcher::new(handler);
        dispatcher.run(&msg_recv);
    })?;

    Ok(Listener { _guard: Some(handle), socket: addr })
}
