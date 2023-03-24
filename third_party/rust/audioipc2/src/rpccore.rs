// Copyright © 2021 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use crossbeam_queue::ArrayQueue;
use mio::Token;
use std::cell::UnsafeCell;
use std::collections::VecDeque;
use std::io::{self, Error, ErrorKind, Result};
use std::marker::PhantomPinned;
use std::mem::ManuallyDrop;
use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, Weak};

use crate::ipccore::EventLoopHandle;

// This provides a safe-ish method for a thread to allocate
// stack storage space for a result, then pass a (wrapped)
// pointer to that location to another thread via
// a CompletionWriter to eventually store a result into.
struct Completion<T> {
    item: UnsafeCell<Option<T>>,
    writer: AtomicBool,
    _pin: PhantomPinned, // disable rustc's no-alias
}

impl<T> Completion<T> {
    fn new() -> Self {
        Completion {
            item: UnsafeCell::new(None),
            writer: AtomicBool::new(false),
            _pin: PhantomPinned,
        }
    }

    // Wait until the writer completes, then return the result.
    // This is intended to be a single-use function, once the writer
    // has completed any further attempts to wait will return None.
    fn wait(&self) -> Option<T> {
        // Wait for the writer to complete or be dropped.
        while self.writer.load(Ordering::Acquire) {
            std::thread::park();
        }
        unsafe { (*self.item.get()).take() }
    }

    // Create a writer for the other thread to store the
    // expected result into.
    fn writer(&self) -> CompletionWriter<T> {
        assert!(!self.writer.load(Ordering::Relaxed));
        self.writer.store(true, Ordering::Release);
        CompletionWriter {
            ptr: self as *const _ as *mut _,
            waiter: std::thread::current(),
        }
    }
}

impl<T> Drop for Completion<T> {
    fn drop(&mut self) {
        // Wait for the outstanding writer to complete before
        // dropping, since the CompletionWriter references
        // memory owned by this object.
        while self.writer.load(Ordering::Acquire) {
            std::thread::park();
        }
    }
}

struct CompletionWriter<T> {
    ptr: *mut Completion<T>, // Points to a Completion on another thread's stack
    waiter: std::thread::Thread, // Identifies thread waiting for completion
}

impl<T> CompletionWriter<T> {
    fn set(self, value: T) {
        // Store the result into the Completion's memory.
        // Since `set` consumes `self`, rely on `Drop` to
        // mark the writer as done and wake the Completion's
        // thread.
        unsafe {
            assert!((*self.ptr).writer.load(Ordering::Relaxed));
            *(*self.ptr).item.get() = Some(value);
        }
    }
}

impl<T> Drop for CompletionWriter<T> {
    fn drop(&mut self) {
        // Mark writer as complete - if `set` was not called,
        // the waiter will receive `None`.
        unsafe {
            (*self.ptr).writer.store(false, Ordering::Release);
        }
        // Wake the Completion's thread.
        self.waiter.unpark();
    }
}

// Safety: CompletionWriter holds a pointer to a Completion
// residing on another thread's stack.  The Completion always
// waits for an outstanding writer if present, and CompletionWriter
// releases the waiter and wakes the Completion's thread on drop,
// so this pointer will always be live for the duration of a
// CompletionWriter.
unsafe impl<T> Send for CompletionWriter<T> {}

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
type ProxyRequest<Request, Response> = (Request, CompletionWriter<Response>);

// RPC Proxy that may be `clone`d for use by multiple owners/threads.
// A Proxy `call` arranges for the supplied request to be transmitted
// to the associated Server via RPC and blocks awaiting the response
// via the associated `Completion`.
// A ClientHandler normally lives until the last Proxy is dropped, but if the ClientHandler
// encounters an internal error, `requests` will fail to upgrade, allowing
// the proxy to report an error.
#[derive(Debug)]
pub struct Proxy<Request, Response> {
    handle: Option<(EventLoopHandle, Token)>,
    requests: ManuallyDrop<RequestQueueSender<ProxyRequest<Request, Response>>>,
}

impl<Request, Response> Proxy<Request, Response> {
    fn new(requests: RequestQueueSender<ProxyRequest<Request, Response>>) -> Self {
        Self {
            handle: None,
            requests: ManuallyDrop::new(requests),
        }
    }

    pub fn call(&self, request: Request) -> Result<Response> {
        let response = Completion::new();
        self.requests.push((request, response.writer()))?;
        self.wake_connection();
        match response.wait() {
            Some(resp) => Ok(resp),
            None => Err(Error::new(ErrorKind::Other, "proxy recv error")),
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
        let mut clone = Self::new((*self.requests).clone());
        let (handle, token) = self
            .handle
            .as_ref()
            .expect("proxy not connected to event loop");
        clone.connect_event_loop(handle.clone(), *token);
        clone
    }
}

impl<Request, Response> Drop for Proxy<Request, Response> {
    fn drop(&mut self) {
        trace!("Proxy drop, waking EventLoop");
        // Must drop `requests` before waking the connection, otherwise
        // the wake may be processed before the (last) weak reference is
        // dropped.
        let last_proxy = self.requests.live_proxies();
        unsafe {
            ManuallyDrop::drop(&mut self.requests);
        }
        if last_proxy == 1 && self.handle.is_some() {
            self.wake_connection()
        }
    }
}

const RPC_CLIENT_INITIAL_PROXIES: usize = 32; // Initial proxy pre-allocation per client.

// Client-specific Handler implementation.
// The IPC EventLoop Driver calls this to execute client-specific
// RPC handling.  Serialized messages sent via a Proxy are queued
// for transmission when `produce` is called.
// Deserialized messages are passed via `consume` to
// trigger response completion by sending the response via a channel
// connected to a ProxyResponse.
pub(crate) struct ClientHandler<C: Client> {
    in_flight: VecDeque<CompletionWriter<C::ClientMessage>>,
    requests: Arc<RequestQueue<ProxyRequest<C::ServerMessage, C::ClientMessage>>>,
}

impl<C: Client> ClientHandler<C> {
    fn new(
        requests: Arc<RequestQueue<ProxyRequest<C::ServerMessage, C::ClientMessage>>>,
    ) -> ClientHandler<C> {
        ClientHandler::<C> {
            in_flight: VecDeque::with_capacity(RPC_CLIENT_INITIAL_PROXIES),
            requests,
        }
    }
}

impl<C: Client> Handler for ClientHandler<C> {
    type In = C::ClientMessage;
    type Out = C::ServerMessage;

    fn consume(&mut self, response: Self::In) -> Result<()> {
        trace!("ClientHandler::consume");
        if let Some(response_writer) = self.in_flight.pop_front() {
            response_writer.set(response);
        } else {
            return Err(Error::new(ErrorKind::Other, "request/response mismatch"));
        }

        Ok(())
    }

    fn produce(&mut self) -> Result<Option<Self::Out>> {
        trace!("ClientHandler::produce");

        // If the weak count is zero, no proxies are attached and
        // no further proxies can be attached since every proxy
        // after the initial one is cloned from an existing instance.
        self.requests.check_live_proxies()?;
        // Try to get a new message
        match self.requests.pop() {
            Some((request, response_writer)) => {
                trace!("  --> received request");
                self.in_flight.push_back(response_writer);
                Ok(Some(request))
            }
            None => {
                trace!("  --> no request");
                Ok(None)
            }
        }
    }
}

#[derive(Debug)]
pub(crate) struct RequestQueue<T> {
    queue: ArrayQueue<T>,
}

impl<T> RequestQueue<T> {
    pub(crate) fn new(size: usize) -> Self {
        RequestQueue {
            queue: ArrayQueue::new(size),
        }
    }

    pub(crate) fn pop(&self) -> Option<T> {
        self.queue.pop()
    }

    pub(crate) fn new_sender(self: &Arc<Self>) -> RequestQueueSender<T> {
        RequestQueueSender {
            inner: Arc::downgrade(self),
        }
    }

    pub(crate) fn check_live_proxies(self: &Arc<Self>) -> Result<()> {
        if Arc::weak_count(self) == 0 {
            return Err(io::ErrorKind::ConnectionAborted.into());
        }
        Ok(())
    }
}

pub(crate) struct RequestQueueSender<T> {
    inner: Weak<RequestQueue<T>>,
}

impl<T> RequestQueueSender<T> {
    pub(crate) fn push(&self, request: T) -> Result<()> {
        if let Some(consumer) = self.inner.upgrade() {
            if consumer.queue.push(request).is_err() {
                debug!("Proxy[{:p}]: call failed - CH::requests full", self);
                return Err(io::ErrorKind::ConnectionAborted.into());
            }
            return Ok(());
        }
        debug!("Proxy[{:p}]: call failed - CH::requests dropped", self);
        Err(Error::new(ErrorKind::Other, "proxy send error"))
    }

    pub(crate) fn live_proxies(&self) -> usize {
        Weak::weak_count(&self.inner)
    }
}

impl<T> Clone for RequestQueueSender<T> {
    fn clone(&self) -> Self {
        Self {
            inner: self.inner.clone(),
        }
    }
}

impl<T> std::fmt::Debug for RequestQueueSender<T> {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        f.debug_struct("RequestQueueProducer")
            .field("inner", &self.inner.as_ptr())
            .finish()
    }
}

#[allow(clippy::type_complexity)]
pub(crate) fn make_client<C: Client>(
) -> Result<(ClientHandler<C>, Proxy<C::ServerMessage, C::ClientMessage>)> {
    let requests = Arc::new(RequestQueue::new(RPC_CLIENT_INITIAL_PROXIES));
    let proxy_req = requests.new_sender();
    let handler = ClientHandler::new(requests);

    Ok((handler, Proxy::new(proxy_req)))
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

#[cfg(test)]
mod test {
    use super::*;

    #[test]
    fn basic() {
        let queue = Arc::new(RequestQueue::new(1));
        let producer = queue.new_sender();
        assert!(queue.pop().is_none());
        producer.push(1).unwrap();
        assert!(queue.pop().is_some());
        assert!(queue.pop().is_none());
    }

    #[test]
    fn queue_dropped() {
        let queue = Arc::new(RequestQueue::new(1));
        let producer = queue.new_sender();
        drop(queue);
        assert!(producer.push(1).is_err());
    }

    #[test]
    fn queue_full() {
        let queue = Arc::new(RequestQueue::new(1));
        let producer = queue.new_sender();
        producer.push(1).unwrap();
        assert!(producer.push(2).is_err());
    }

    #[test]
    fn queue_producer_clone() {
        let queue = Arc::new(RequestQueue::new(1));
        let producer = queue.new_sender();
        let producer2 = producer.clone();
        producer.push(1).unwrap();
        assert!(producer2.push(2).is_err());
    }

    #[test]
    fn queue_producer_drop() {
        let queue = Arc::new(RequestQueue::new(1));
        let producer = queue.new_sender();
        let producer2 = producer.clone();
        drop(producer);
        assert!(producer2.push(2).is_ok());
    }

    #[test]
    fn queue_producer_weak() {
        let queue = Arc::new(RequestQueue::new(1));
        let producer = queue.new_sender();
        let producer2 = producer.clone();
        drop(queue);
        assert!(producer2.push(2).is_err());
    }

    #[test]
    fn queue_producer_shutdown() {
        let queue = Arc::new(RequestQueue::new(1));
        let producer = queue.new_sender();
        let producer2 = producer.clone();
        producer.push(1).unwrap();
        assert!(Arc::weak_count(&queue) == 2);
        drop(producer);
        assert!(Arc::weak_count(&queue) == 1);
        drop(producer2);
        assert!(Arc::weak_count(&queue) == 0);
    }
}
