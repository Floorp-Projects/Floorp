/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

use crate::command::{WebDriverCommand, WebDriverMessage};
use crate::error::{ErrorStatus, WebDriverError, WebDriverResult};
use crate::httpapi::{
    standard_routes, Route, VoidWebDriverExtensionRoute, WebDriverExtensionRoute,
};
use crate::response::{CloseWindowResponse, WebDriverResponse};
use crate::Parameters;
use bytes::Bytes;
use http::{self, Method, StatusCode};
use once_cell::sync::Lazy;
use regex::Regex;
use std::marker::PhantomData;
use std::net::{
    IpAddr, Ipv4Addr, Ipv6Addr, SocketAddr, TcpListener as StdTcpListener, ToSocketAddrs,
};
use std::sync::mpsc::{channel, Receiver, Sender};
use std::sync::{Arc, Mutex};
use std::thread;
use tokio::net::TcpListener;
use url::{Host, Url};
use warp::{self, Buf, Filter, Rejection};

// Not a complete regex for parsing the Host header, but enough to parse out
// either a ip6 address surrounded by [], or an undelimted hostname or ipv4 address,
// any of which can be followed by an optonal : followed by a port.
// Validation of the host part is done in Host::parse
static HOST_REGEXP: Lazy<Regex> = Lazy::new(|| {
    Regex::new(concat!(
        r"^(?P<host>(:?\[[^\]]+\])|(:?[^ :]+))",
        r"(:?:(?P<port>[[:digit:]]+))?$"
    ))
    .unwrap()
});

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
/// Representation of whether we managed to successfully send a DeleteSession message
/// and read the response during session teardown.
pub enum SessionTeardownKind {
    /// A DeleteSession message has been sent and the response handled.
    Deleted,
    /// No DeleteSession message has been sent, or the response was not received.
    NotDeleted,
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
    fn teardown_session(&mut self, kind: SessionTeardownKind);
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
                                // The teardown_session implementation is responsible for actually
                                // sending the DeleteSession message in this case
                                self.teardown_session(SessionTeardownKind::NotDeleted);
                            }
                        }
                        Ok(WebDriverResponse::DeleteSession) => {
                            self.teardown_session(SessionTeardownKind::Deleted);
                        }
                        Err(ref x) if x.delete_session => {
                            // This includes the case where we failed during session creation
                            self.teardown_session(SessionTeardownKind::NotDeleted)
                        }
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

    fn teardown_session(&mut self, kind: SessionTeardownKind) {
        debug!("Teardown session");
        let final_kind = match kind {
            SessionTeardownKind::NotDeleted if self.session.is_some() => {
                let delete_session = WebDriverMessage {
                    session_id: Some(
                        self.session
                            .as_ref()
                            .expect("Failed to get session")
                            .id
                            .clone(),
                    ),
                    command: WebDriverCommand::DeleteSession,
                };
                match self.handler.handle_command(&self.session, delete_session) {
                    Ok(_) => SessionTeardownKind::Deleted,
                    Err(_) => SessionTeardownKind::NotDeleted,
                }
            }
            _ => kind,
        };
        self.handler.teardown_session(final_kind);
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
    host: String,
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
        let wroutes = build_warp_routes(host, address, &extension_routes, msg_send.clone());
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
    host: String,
    address: SocketAddr,
    ext_routes: &[(Method, &'static str, U)],
    chan: Sender<DispatchMessage<U>>,
) -> impl Filter<Extract = impl warp::Reply, Error = Rejection> + Clone {
    let chan = Arc::new(Mutex::new(chan));
    let mut std_routes = standard_routes::<U>();
    let (method, path, res) = std_routes.pop().unwrap();
    let server_host = Host::parse(&host).expect("Failed to parse server hostname as a host");
    let mut wroutes = build_route(
        server_host.clone(),
        address,
        method,
        path,
        res,
        chan.clone(),
    );
    for (method, path, res) in std_routes {
        wroutes = wroutes
            .or(build_route(
                server_host.clone(),
                address,
                method,
                path,
                res.clone(),
                chan.clone(),
            ))
            .unify()
            .boxed()
    }
    for (method, path, res) in ext_routes {
        wroutes = wroutes
            .or(build_route(
                server_host.clone(),
                address,
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

fn parse_host(host_port: &str) -> WebDriverResult<(Host, Option<u16>)> {
    // Get a (host, port) tuple from host:port
    let make_err = || {
        WebDriverError::new(
            ErrorStatus::UnknownError,
            format!("Invalid Host {}", host_port),
        )
    };

    let captures = HOST_REGEXP.captures(host_port).ok_or_else(make_err)?;
    let host = captures
        .name("host")
        .map(|m| m.as_str())
        .ok_or_else(make_err)
        .and_then(|host| Host::parse(host).map_err(|_| make_err()))?;

    let maybe_port = if let Some(port) = captures.name("port").map(|m| m.as_str()) {
        Some(port.parse().map_err(|_| make_err())?)
    } else {
        None
    };
    Ok((host, maybe_port))
}

fn host_is_local(host: &Host) -> bool {
    // Check if host is a simple synonym of localhost
    // i.e. "127.0.0.1", "::1", or "localhost" with a loopback ip
    match host {
        Host::Domain(ref domain) => {
            // Check if the domain is a well-known local domain and it's actually bound to a local ip
            domain == "localhost"
                // port here is just a dummy; the DNS lookup doesn't depend on it
                && (domain.to_string(), 80)
                    .to_socket_addrs()
                    .map(|addr_iter| {
                        addr_iter.map(|addr| addr.ip()).any(|ip| match ip {
                            IpAddr::V4(ip_v4) => ip_v4.is_loopback(),
                            IpAddr::V6(ip_v6) => ip_v6.is_loopback(),
                        })
                    })
                    .unwrap_or(false)
        }
        Host::Ipv4(ip) => ip == &Ipv4Addr::LOCALHOST,
        Host::Ipv6(ip) => ip == &Ipv6Addr::LOCALHOST,
    }
}

fn host_and_port_match_server(
    server_host: &Host,
    server_address: &SocketAddr,
    header_host_port: (Host, Option<u16>),
) -> bool {
    // Validate that the result of parsing the Host header matches the server configuration

    // If there's no port we're a HTTP server so default to 80
    let host = header_host_port.0;
    let port = header_host_port.1.unwrap_or(80);
    let host_matches = if host_is_local(server_host) && host_is_local(&host) {
        // If both the host header and the server are standard loopback names,
        // accept the match. This means that the server can bind to 127.0.0.1 and
        // the request can be for http://localhost for example
        true
    } else if host == *server_host {
        match host {
            Host::Domain(ref domain) => {
                // For a domain we also check that the ip matches
                (domain.to_string(), port)
                    .to_socket_addrs()
                    .map(|addr_iter| {
                        addr_iter
                            .map(|addr| addr.ip())
                            .any(|ip| ip == server_address.ip())
                    })
                    .unwrap_or(false)
            }
            Host::Ipv4(_) | Host::Ipv6(_) => true,
        }
    } else {
        false
    };
    let port_matches = server_address.port() == port;
    host_matches && port_matches
}

fn origin_is_local(url_str: &str) -> WebDriverResult<bool> {
    // Validate that the URL string from an Origin header corresponds to a local interface
    let make_err = || {
        WebDriverError::new(
            ErrorStatus::UnknownError,
            format!("Invalid Origin {}", url_str),
        )
    };

    let url = Url::parse(url_str).map_err(|_| make_err())?;
    let sockets = url.socket_addrs(|| None).map_err(|_| make_err())?;

    Ok(!sockets.is_empty() && sockets.iter().all(|x| x.ip().is_loopback()))
}

fn build_route<U: 'static + WebDriverExtensionRoute + Send + Sync>(
    server_host: Host,
    server_address: SocketAddr,
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
        .and(warp::header::optional::<String>("origin"))
        .and(warp::header::optional::<String>("host"))
        .and(warp::header::optional::<String>("content-type"))
        .and(warp::body::bytes())
        .map(
            move |params,
                  full_path: warp::path::FullPath,
                  method,
                  origin_header: Option<String>,
                  host_header: Option<String>,
                  content_type_header: Option<String>,
                  body: Bytes| {
                if method == Method::HEAD {
                    return warp::reply::with_status("".into(), StatusCode::OK);
                }
                if let Some(host) = host_header {
                    let host_port = match parse_host(&host) {
                        Ok(x) => x,
                        Err(err) => {
                            return warp::reply::with_status(
                                serde_json::to_string(&err).unwrap(),
                                StatusCode::INTERNAL_SERVER_ERROR,
                            )
                        }
                    };
                    if !host_and_port_match_server(&server_host, &server_address, host_port) {
                        let err = WebDriverError::new(
                            ErrorStatus::UnknownError,
                            format!("Host header doesn't match server {}", host),
                        );
                        return warp::reply::with_status(
                            serde_json::to_string(&err).unwrap(),
                            StatusCode::INTERNAL_SERVER_ERROR,
                        );
                    };
                } else {
                    let err = WebDriverError::new(
                        ErrorStatus::UnknownError,
                        "Missing Host header".to_string(),
                    );
                    return warp::reply::with_status(
                        serde_json::to_string(&err).unwrap(),
                        StatusCode::INTERNAL_SERVER_ERROR,
                    );
                }
                if let Some(origin) = origin_header {
                    let origin_match_err = match origin_is_local(&origin) {
                        Ok(true) => None,
                        Ok(false) => Some(WebDriverError::new(
                            ErrorStatus::UnknownError,
                            format!("Request Origin {} isn't local", origin),
                        )),
                        Err(err) => Some(err),
                    };
                    if origin_match_err.is_some() {
                        return warp::reply::with_status(
                            serde_json::to_string(&origin_match_err).unwrap(),
                            StatusCode::INTERNAL_SERVER_ERROR,
                        );
                    }
                }
                if method == Method::POST {
                    // Disallow CORS-safelisted request headers
                    // c.f. https://fetch.spec.whatwg.org/#cors-safelisted-request-header
                    let content_type = content_type_header
                        .as_ref()
                        .map(|x| x.find(';').and_then(|idx| x.get(0..idx)).unwrap_or(x))
                        .map(|x| x.trim())
                        .map(|x| x.to_lowercase());
                    match content_type.as_ref().map(|x| x.as_ref()) {
                        Some("application/x-www-form-urlencoded")
                        | Some("multipart/form-data")
                        | Some("text/plain") => {
                            let err = WebDriverError::new(
                                ErrorStatus::UnknownError,
                                "Invalid Content-Type",
                            );
                            return warp::reply::with_status(
                                serde_json::to_string(&err).unwrap(),
                                StatusCode::INTERNAL_SERVER_ERROR,
                            );
                        }
                        Some(_) | None => {}
                    }
                }
                let body = String::from_utf8(body.bytes().to_vec());
                if body.is_err() {
                    let err = WebDriverError::new(
                        ErrorStatus::UnknownError,
                        "Request body wasn't valid UTF-8",
                    );
                    return warp::reply::with_status(
                        serde_json::to_string(&err).unwrap(),
                        StatusCode::INTERNAL_SERVER_ERROR,
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

#[cfg(test)]
mod tests {
    use super::*;
    use std::net::{IpAddr, Ipv4Addr, Ipv6Addr};
    use std::str::FromStr;

    #[test]
    fn test_parse_host() {
        assert_eq!(
            parse_host("example.org"),
            Ok((Host::parse("example.org").unwrap(), None))
        );
        assert_eq!(
            parse_host("example.org:8000"),
            Ok((Host::parse("example.org").unwrap(), Some(8000)))
        );
        assert_eq!(
            parse_host("127.0.0.1:8000"),
            Ok((Host::parse("127.0.0.1").unwrap(), Some(8000)))
        );
        assert_eq!(
            parse_host("127.0.0.1"),
            Ok((Host::parse("127.0.0.1").unwrap(), None))
        );
        assert_eq!(
            parse_host("[::1]"),
            Ok((Host::parse("[::1]").unwrap(), None))
        );
        assert_eq!(
            parse_host("[::1]:443"),
            Ok((Host::parse("[::1]").unwrap(), Some(443)))
        );
        assert!(parse_host("localhost:").is_err());
        assert!(parse_host("localhost/foo").is_err());
        assert!(parse_host("localhost:65536").is_err());
        assert!(parse_host("::1:8000").is_err());
        assert!(parse_host("localhost:80:433").is_err());
        assert!(parse_host("localhost:80:abc").is_err());
    }

    #[test]
    fn test_host_and_port_match_server() {
        let addr_80 = SocketAddr::new(IpAddr::from_str("127.0.0.1").unwrap(), 80);
        let addr_8000 = SocketAddr::new(IpAddr::from_str("127.0.0.1").unwrap(), 8000);
        let addr_v6_80 = SocketAddr::new(IpAddr::from_str("::1").unwrap(), 80);
        let addr_v6_8000 = SocketAddr::new(IpAddr::from_str("::1").unwrap(), 8000);

        // We match the host ip address to the server, so we can only use hosts that actually resolve
        let localhost_host = Host::Domain("localhost".to_string());
        let ipv4_host = Host::Ipv4(Ipv4Addr::from_str("127.0.0.1").unwrap());
        let ipv6_host = Host::Ipv6(Ipv6Addr::from_str("::1").unwrap());
        let subdomain_localhost_host = Host::Domain("subdomain.localhost".to_string());

        assert!(host_and_port_match_server(
            &localhost_host,
            &addr_80,
            (localhost_host.clone(), Some(80))
        ));
        assert!(host_and_port_match_server(
            &localhost_host,
            &addr_80,
            (localhost_host.clone(), None)
        ));
        assert!(host_and_port_match_server(
            &ipv4_host,
            &addr_80,
            (ipv4_host.clone(), None)
        ));
        assert!(host_and_port_match_server(
            &ipv4_host,
            &addr_8000,
            (ipv4_host.clone(), Some(8000))
        ));
        assert!(host_and_port_match_server(
            &ipv6_host,
            &addr_v6_80,
            (ipv6_host.clone(), None)
        ));
        assert!(host_and_port_match_server(
            &ipv6_host,
            &addr_v6_8000,
            (ipv6_host.clone(), Some(8000))
        ));

        // Cases where the server and host mismatch, but they're both known to be localhost
        assert!(host_and_port_match_server(
            &localhost_host,
            &addr_8000,
            (ipv4_host.clone(), Some(8000))
        ));
        assert!(host_and_port_match_server(
            &localhost_host,
            &addr_8000,
            (ipv6_host.clone(), Some(8000))
        ));
        assert!(host_and_port_match_server(
            &ipv4_host,
            &addr_8000,
            (localhost_host.clone(), Some(8000))
        ));
        assert!(host_and_port_match_server(
            &ipv4_host,
            &addr_8000,
            (ipv6_host.clone(), Some(8000))
        ));
        assert!(host_and_port_match_server(
            &ipv6_host,
            &addr_8000,
            (localhost_host.clone(), Some(8000))
        ));
        assert!(host_and_port_match_server(
            &ipv6_host,
            &addr_8000,
            (ipv4_host.clone(), Some(8000))
        ));

        // Mismatch cases

        assert!(!host_and_port_match_server(
            &subdomain_localhost_host,
            &addr_8000,
            (localhost_host.clone(), Some(8000))
        ));

        assert!(!host_and_port_match_server(
            &subdomain_localhost_host,
            &addr_8000,
            (ipv4_host.clone(), Some(8000))
        ));

        assert!(!host_and_port_match_server(
            &subdomain_localhost_host,
            &addr_8000,
            (ipv6_host.clone(), Some(8000))
        ));

        assert!(!host_and_port_match_server(
            &Host::parse("127.0.0.2").unwrap(),
            &addr_8000,
            (localhost_host.clone(), Some(8000))
        ));

        // Mismatch ports
        assert!(!host_and_port_match_server(
            &localhost_host,
            &addr_80,
            (localhost_host.clone(), Some(8000))
        ));
        assert!(!host_and_port_match_server(
            &localhost_host,
            &addr_8000,
            (localhost_host.clone(), None)
        ));
    }

    #[test]
    fn test_origin_is_local() {
        // This depends on network configuration; we assume that localhost, 127.0.0.1 and [::1] are loopback
        // and that example.org and 1.1.1.1 are not.
        assert!(origin_is_local("https://127.0.0.1").unwrap());
        assert!(origin_is_local("http://127.0.0.1:8000").unwrap());
        assert!(origin_is_local("http://[::1]").unwrap());
        assert!(origin_is_local("https://[::1]:9999").unwrap());
        assert!(origin_is_local("http://localhost").unwrap());
        assert!(origin_is_local("https://localhost").unwrap());
        assert!(origin_is_local("http://localhost:4444").unwrap());
        assert!(!origin_is_local("http://example.org").unwrap());
        assert!(!origin_is_local("https://example.org:1000").unwrap());
        assert!(!origin_is_local("http://1.1.1.1").unwrap());
        assert!(origin_is_local("localhost").is_err());
        assert!(origin_is_local("localhost:443").is_err());
        assert!(origin_is_local("127.0.0.1:443").is_err());
        assert!(origin_is_local("[::1]").is_err());
    }
}
