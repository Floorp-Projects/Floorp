// Copyright © 2021 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use std::collections::VecDeque;
use std::io::{self, Result};
use std::mem::ManuallyDrop;
use std::sync::{Arc, Mutex, Weak};

use crossbeam_channel::{self, Receiver, Sender};
use mio::Token;
use slab::Slab;

use crate::ipccore::EventLoopHandle;

// RPC message handler.  Implemented by ClientHandler (for Client)
// and ServerHandler (for Server).
pub(crate) trait Handler {
    type In;
    type Out;

    // Consume a request
    fn consume(&mut self, request: Self::In) -> Result<()>;

    // Produce a response
    fn produce(&mut self) -> Result<Option<Self::Out>>;
}

// Client RPC definition.  This supplies the expected message
// request and response types.
pub trait Client {
    type ServerMessage;
    type ClientMessage;
}

// Server RPC definition.  This supplies the expected message
// request and response types.  `process` is passed inbound RPC
// requests by the ServerHandler to be responded to by the server.
pub trait Server {
    type ServerMessage;
    type ClientMessage;

    fn process(&mut self, req: Self::ServerMessage) -> Self::ClientMessage;
}

// RPC Client Proxy implementation.
type ProxyKey = usize;
type ProxyRequest<Request> = (ProxyKey, Request);

// RPC Proxy that may be `clone`d for use by multiple owners/threads.
// A Proxy `call` arranges for the supplied request to be transmitted
// to the associated Server via RPC and blocks awaiting the response
// via `response_rx`.
// A Proxy is associated with the ClientHandler via `handler_tx` to send requests,
// `response_rx` to receive responses, and uses `key` to identify the Proxy with
// the sending side of `response_rx`  ClientHandler.
// Each Proxy is registered with the ClientHandler on initialization via the
// ProxyManager and unregistered when dropped.
// A ClientHandler normally lives until the last Proxy is dropped, but if the ClientHandler
// encounters an internal error, `response_tx` will be closed and `proxy_mgr` can
// no longer be upgraded to register new Proxies.
#[derive(Debug)]
pub struct Proxy<Request, Response> {
    handle: Option<(EventLoopHandle, Token)>,
    key: ProxyKey,
    response_rx: Receiver<Response>,
    handler_tx: ManuallyDrop<Sender<ProxyRequest<Request>>>,
    proxy_mgr: Weak<ProxyManager<Response>>,
}

impl<Request, Response> Proxy<Request, Response> {
    fn new(
        handler_tx: Sender<ProxyRequest<Request>>,
        proxy_mgr: Weak<ProxyManager<Response>>,
    ) -> Self {
        let (tx, rx) = crossbeam_channel::bounded(1);
        Self {
            handle: None,
            key: proxy_mgr.upgrade().unwrap().register_proxy(tx),
            response_rx: rx,
            handler_tx: ManuallyDrop::new(handler_tx),
            proxy_mgr,
        }
    }

    pub fn call(&self, request: Request) -> Result<Response> {
        match self.handler_tx.send((self.key, request)) {
            Ok(_) => self.wake_connection(),
            Err(_) => {
                return Err(std::io::Error::new(
                    std::io::ErrorKind::Other,
                    "proxy send error",
                ))
            }
        }
        match self.response_rx.recv() {
            Ok(resp) => Ok(resp),
            Err(_) => Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "proxy recv error",
            )),
        }
    }

    pub(crate) fn connect_event_loop(&mut self, handle: EventLoopHandle, token: Token) {
        self.handle = Some((handle, token));
    }

    fn wake_connection(&self) {
        let (handle, token) = self
            .handle
            .as_ref()
            .expect("proxy not connected to event loop");
        handle.wake_connection(*token);
    }
}

impl<Request, Response> Clone for Proxy<Request, Response> {
    fn clone(&self) -> Self {
        let (tx, rx) = crossbeam_channel::bounded(1);
        Self {
            handle: self.handle.clone(),
            key: self.proxy_mgr.upgrade().unwrap().register_proxy(tx),
            response_rx: rx,
            handler_tx: self.handler_tx.clone(),
            proxy_mgr: self.proxy_mgr.clone(),
        }
    }
}

impl<Request, Response> Drop for Proxy<Request, Response> {
    fn drop(&mut self) {
        trace!("Proxy drop, waking EventLoop");
        if let Some(mgr) = self.proxy_mgr.upgrade() {
            mgr.unregister_proxy(self.key)
        }
        // Must drop Sender before waking the connection, otherwise
        // the wake may be processed before Sender is closed.
        unsafe {
            ManuallyDrop::drop(&mut self.handler_tx);
        }
        if self.handle.is_some() {
            self.wake_connection()
        }
    }
}

const RPC_CLIENT_INITIAL_PROXIES: usize = 32; // Initial proxy pre-allocation per client.

// Manage the Sender side of a ClientHandler's Proxies.  Each Proxy registers itself with
// the manager on initialization.
#[derive(Debug)]
struct ProxyManager<Response> {
    proxies: Mutex<Slab<Sender<Response>>>,
}

impl<Response> ProxyManager<Response> {
    fn new() -> Self {
        Self {
            proxies: Mutex::new(Slab::with_capacity(RPC_CLIENT_INITIAL_PROXIES)),
        }
    }

    // Register a Proxy's response Sender, returning a unique ID identifying
    // the Proxy to the ClientHandler.
    fn register_proxy(&self, tx: Sender<Response>) -> ProxyKey {
        let mut proxies = self.proxies.lock().unwrap();
        let entry = proxies.vacant_entry();
        let key = entry.key();
        entry.insert(tx);
        key
    }

    fn unregister_proxy(&self, key: ProxyKey) {
        let _ = self.proxies.lock().unwrap().remove(key);
    }

    // Deliver ClientHandler's Response to the Proxy associated with `key`.
    fn deliver(&self, key: ProxyKey, resp: Response) {
        let _ = self.proxies.lock().unwrap()[key].send(resp);
    }
}

// Client-specific Handler implementation.
// The IPC EventLoop Driver calls this to execute client-specific
// RPC handling.  Serialized messages sent via a Proxy are queued
// for transmission when `produce` is called.
// Deserialized messages are passed via `consume` to
// trigger response completion by sending the response via a channel
// connected to a ProxyResponse.
pub(crate) struct ClientHandler<C: Client> {
    messages: Receiver<ProxyRequest<C::ServerMessage>>,
    // Proxies hold a Weak<ProxyManager> to register on initialization.
    // When ClientHandler drops, any Proxies blocked on a response will
    // error due to the Sender closing.
    proxies: Arc<ProxyManager<C::ClientMessage>>,
    in_flight: VecDeque<ProxyKey>,
}

impl<C: Client> ClientHandler<C> {
    fn new(rx: Receiver<ProxyRequest<C::ServerMessage>>) -> ClientHandler<C> {
        ClientHandler::<C> {
            messages: rx,
            proxies: Arc::new(ProxyManager::new()),
            in_flight: VecDeque::with_capacity(RPC_CLIENT_INITIAL_PROXIES),
        }
    }

    fn proxy_manager(&self) -> Weak<ProxyManager<<C as Client>::ClientMessage>> {
        Arc::downgrade(&self.proxies)
    }
}

impl<C: Client> Handler for ClientHandler<C> {
    type In = C::ClientMessage;
    type Out = C::ServerMessage;

    fn consume(&mut self, response: Self::In) -> Result<()> {
        trace!("ClientHandler::consume");
        // `proxy` identifies the waiting Proxy expecting `response`.
        if let Some(proxy) = self.in_flight.pop_front() {
            self.proxies.deliver(proxy, response);
        } else {
            return Err(std::io::Error::new(
                std::io::ErrorKind::Other,
                "request/response mismatch",
            ));
        }

        Ok(())
    }

    fn produce(&mut self) -> Result<Option<Self::Out>> {
        trace!("ClientHandler::produce");

        // Try to get a new message
        match self.messages.try_recv() {
            Ok((proxy, request)) => {
                trace!("  --> received request");
                self.in_flight.push_back(proxy);
                Ok(Some(request))
            }
            Err(crossbeam_channel::TryRecvError::Empty) => {
                trace!("  --> no request");
                Ok(None)
            }
            Err(e) => {
                trace!("  --> client disconnected");
                Err(io::Error::new(io::ErrorKind::ConnectionAborted, e))
            }
        }
    }
}

pub(crate) fn make_client<C: Client>(
) -> (ClientHandler<C>, Proxy<C::ServerMessage, C::ClientMessage>) {
    let (tx, rx) = crossbeam_channel::bounded(RPC_CLIENT_INITIAL_PROXIES);

    let handler = ClientHandler::new(rx);
    let proxy_mgr = handler.proxy_manager();

    (handler, Proxy::new(tx, proxy_mgr))
}

// Server-specific Handler implementation.
// The IPC EventLoop Driver calls this to execute server-specific
// RPC handling.  Deserialized messages are passed via `consume` to the
// associated `server` for processing.  Server responses are then queued
// for RPC to the associated client when `produce` is called.
pub(crate) struct ServerHandler<S: Server> {
    server: S,
    in_flight: VecDeque<S::ClientMessage>,
}

impl<S: Server> Handler for ServerHandler<S> {
    type In = S::ServerMessage;
    type Out = S::ClientMessage;

    fn consume(&mut self, message: Self::In) -> Result<()> {
        trace!("ServerHandler::consume");
        let response = self.server.process(message);
        self.in_flight.push_back(response);
        Ok(())
    }

    fn produce(&mut self) -> Result<Option<Self::Out>> {
        trace!("ServerHandler::produce");

        // Return the ready response
        match self.in_flight.pop_front() {
            Some(res) => {
                trace!("  --> received response");
                Ok(Some(res))
            }
            None => {
                trace!("  --> no response ready");
                Ok(None)
            }
        }
    }
}

const RPC_SERVER_INITIAL_CLIENTS: usize = 32; // Initial client allocation per server.

pub(crate) fn make_server<S: Server>(server: S) -> ServerHandler<S> {
    ServerHandler::<S> {
        server,
        in_flight: VecDeque::with_capacity(RPC_SERVER_INITIAL_CLIENTS),
    }
}
