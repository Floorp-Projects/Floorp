use serde_json;
use std::marker::PhantomData;
use std::net::{SocketAddr, TcpListener as StdTcpListener};
use std::sync::mpsc::{channel, Receiver, Sender};
use std::sync::{Arc, Mutex};
use std::thread;

use bytes::Bytes;
use http::{self, Method, StatusCode};
use tokio::net::TcpListener;
use warp::{self, Buf, Filter, Rejection};

use crate::command::{WebDriverCommand, WebDriverMessage};
use crate::error::{ErrorStatus, WebDriverError, WebDriverResult};
use crate::httpapi::{
    standard_routes, Route, VoidWebDriverExtensionRoute, WebDriverExtensionRoute,
};
use crate::response::{CloseWindowResponse, WebDriverResponse};
use crate::Parameters;

// Silence warning about Quit being unused for now.
#[allow(dead_code)]
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

pub struct Listener {
    guard: Option<thread::JoinHandle<()>>,
    pub socket: SocketAddr,
}

impl Drop for Listener {
    fn drop(&mut self) {
        let _ = self.guard.take().map(|j| j.join());
    }
}

pub fn start<T, U>(
    address: SocketAddr,
    handler: T,
    extension_routes: Vec<(Method, &'static str, U)>,
) -> ::std::io::Result<Listener>
where
    T: 'static + WebDriverHandler<U>,
    U: 'static + WebDriverExtensionRoute + Send + Sync,
{
    let listener = StdTcpListener::bind(address)?;
    let addr = listener.local_addr()?;
    let (msg_send, msg_recv) = channel();

    let builder = thread::Builder::new().name("webdriver server".to_string());
    let handle = builder.spawn(move || {
        let mut rt = tokio::runtime::Builder::new()
            .basic_scheduler()
            .enable_io()
            .build()
            .unwrap();
        let mut listener = rt
            .handle()
            .enter(|| TcpListener::from_std(listener).unwrap());
        let wroutes = build_warp_routes(&extension_routes, msg_send.clone());
        let fut = warp::serve(wroutes).run_incoming(listener.incoming());
        rt.block_on(fut);
    })?;

    let builder = thread::Builder::new().name("webdriver dispatcher".to_string());
    builder.spawn(move || {
        let mut dispatcher = Dispatcher::new(handler);
        dispatcher.run(&msg_recv);
    })?;

    Ok(Listener {
        guard: Some(handle),
        socket: addr,
    })
}

fn build_warp_routes<U: 'static + WebDriverExtensionRoute + Send + Sync>(
    ext_routes: &[(Method, &'static str, U)],
    chan: Sender<DispatchMessage<U>>,
) -> impl Filter<Extract = impl warp::Reply, Error = Rejection> + Clone {
    let chan = Arc::new(Mutex::new(chan));
    let mut std_routes = standard_routes::<U>();
    let (method, path, res) = std_routes.pop().unwrap();
    let mut wroutes = build_route(method, path, res, chan.clone());
    for (method, path, res) in std_routes {
        wroutes = wroutes
            .or(build_route(method, path, res.clone(), chan.clone()))
            .unify()
            .boxed()
    }
    for (method, path, res) in ext_routes {
        wroutes = wroutes
            .or(build_route(
                method.clone(),
                path,
                Route::Extension(res.clone()),
                chan.clone(),
            ))
            .unify()
            .boxed()
    }
    wroutes
}

fn build_route<U: 'static + WebDriverExtensionRoute + Send + Sync>(
    method: Method,
    path: &'static str,
    route: Route<U>,
    chan: Arc<Mutex<Sender<DispatchMessage<U>>>>,
) -> warp::filters::BoxedFilter<(impl warp::Reply,)> {
    // Create an empty filter based on the provided method and append an empty hashmap to it. The
    // hashmap will be used to store path parameters.
    let mut subroute = match method {
        Method::GET => warp::get().boxed(),
        Method::POST => warp::post().boxed(),
        Method::DELETE => warp::delete().boxed(),
        Method::OPTIONS => warp::options().boxed(),
        Method::PUT => warp::put().boxed(),
        _ => panic!("Unsupported method"),
    }
    .or(warp::head())
    .unify()
    .map(Parameters::new)
    .boxed();

    // For each part of the path, if it's a normal part, just append it to the current filter,
    // otherwise if it's a parameter (a named enclosed in { }), we take that parameter and insert
    // it into the hashmap created earlier.
    for part in path.split('/') {
        if part.is_empty() {
            continue;
        } else if part.starts_with('{') {
            assert!(part.ends_with('}'));

            subroute = subroute
                .and(warp::path::param())
                .map(move |mut params: Parameters, param: String| {
                    let name = &part[1..part.len() - 1];
                    params.insert(name.to_string(), param);
                    params
                })
                .boxed();
        } else {
            subroute = subroute.and(warp::path(part)).boxed();
        }
    }

    // Finally, tell warp that the path is complete
    subroute
        .and(warp::path::end())
        .and(warp::path::full())
        .and(warp::method())
        .and(warp::body::bytes())
        .map(
            move |params, full_path: warp::path::FullPath, method, body: Bytes| {
                if method == Method::HEAD {
                    return warp::reply::with_status("".into(), StatusCode::OK);
                }
                let body = String::from_utf8(body.bytes().to_vec());
                if body.is_err() {
                    return warp::reply::with_status(
                        "The body wasn't valid UTF-8".to_string(),
                        StatusCode::BAD_REQUEST,
                    );
                }
                let body = body.unwrap();

                debug!("-> {} {} {}", method, full_path.as_str(), body);
                let msg_result = WebDriverMessage::from_http(
                    route.clone(),
                    &params,
                    &body,
                    method == Method::POST,
                );

                let (status, resp_body) = match msg_result {
                    Ok(message) => {
                        let (send_res, recv_res) = channel();
                        match chan.lock() {
                            Ok(ref c) => {
                                let res =
                                    c.send(DispatchMessage::HandleWebDriver(message, send_res));
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
                warp::reply::with_status(resp_body, status)
            },
        )
        .with(warp::reply::with::header(
            http::header::CONTENT_TYPE,
            "application/json; charset=utf-8",
        ))
        .with(warp::reply::with::header(
            http::header::CACHE_CONTROL,
            "no-cache",
        ))
        .boxed()
}
