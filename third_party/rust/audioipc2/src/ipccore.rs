// Copyright © 2021 Mozilla Foundation
//
// This program is made available under an ISC-style license.  See the
// accompanying file LICENSE for details

use std::io::{self, Result};
use std::sync::{mpsc, Arc};
use std::thread;

use mio::{event::Event, Events, Interest, Poll, Registry, Token, Waker};
use slab::Slab;

use crate::messages::AssociateHandleForMessage;
use crate::rpccore::{
    make_client, make_server, Client, Handler, Proxy, RequestQueue, RequestQueueSender, Server,
};
use crate::{
    codec::Codec,
    codec::LengthDelimitedCodec,
    sys::{self, RecvMsg, SendMsg},
};

use serde::{de::DeserializeOwned, Serialize};
use std::fmt::Debug;

const WAKE_TOKEN: Token = Token(!0);

thread_local!(static IN_EVENTLOOP: std::cell::RefCell<Option<thread::ThreadId>> = const { std::cell::RefCell::new(None) });

fn assert_not_in_event_loop_thread() {
    IN_EVENTLOOP.with(|b| {
        assert_ne!(*b.borrow(), Some(thread::current().id()));
    });
}

// Requests sent by an EventLoopHandle to be serviced by
// the handle's associated EventLoop.
enum Request {
    // See EventLoop::add_connection
    AddConnection(
        sys::Pipe,
        Box<dyn Driver + Send>,
        mpsc::Sender<Result<Token>>,
    ),
    // See EventLoop::shutdown
    Shutdown,
    // See EventLoop::wake_connection
    WakeConnection(Token),
}

// EventLoopHandle is a cloneable external reference
// to a running EventLoop, allowing registration of
// new client and server connections, in addition to
// requesting the EventLoop shut down cleanly.
#[derive(Clone, Debug)]
pub struct EventLoopHandle {
    waker: Arc<Waker>,
    requests: RequestQueueSender<Request>,
}

impl EventLoopHandle {
    pub fn bind_client<C: Client + 'static>(
        &self,
        connection: sys::Pipe,
    ) -> Result<Proxy<<C as Client>::ServerMessage, <C as Client>::ClientMessage>>
    where
        <C as Client>::ServerMessage: Serialize + Debug + AssociateHandleForMessage + Send,
        <C as Client>::ClientMessage: DeserializeOwned + Debug + AssociateHandleForMessage + Send,
    {
        let (handler, mut proxy) = make_client::<C>()?;
        let driver = Box::new(FramedDriver::new(handler));
        let r = self.add_connection(connection, driver);
        trace!("EventLoop::bind_client {:?}", r);
        r.map(|token| {
            proxy.connect_event_loop(self.clone(), token);
            proxy
        })
    }

    pub fn bind_server<S: Server + Send + 'static>(
        &self,
        server: S,
        connection: sys::Pipe,
    ) -> Result<()>
    where
        <S as Server>::ServerMessage: DeserializeOwned + Debug + AssociateHandleForMessage + Send,
        <S as Server>::ClientMessage: Serialize + Debug + AssociateHandleForMessage + Send,
    {
        let handler = make_server::<S>(server);
        let driver = Box::new(FramedDriver::new(handler));
        let r = self.add_connection(connection, driver);
        trace!("EventLoop::bind_server {:?}", r);
        r.map(|_| ())
    }

    // Register a new connection with associated driver on the EventLoop.
    // TODO: Since this is called from a Gecko main thread, make this non-blocking wrt. the EventLoop.
    fn add_connection(
        &self,
        connection: sys::Pipe,
        driver: Box<dyn Driver + Send>,
    ) -> Result<Token> {
        assert_not_in_event_loop_thread();
        let (tx, rx) = mpsc::channel();
        self.requests
            .push(Request::AddConnection(connection, driver, tx))
            .map_err(|_| {
                debug!("EventLoopHandle::add_connection send failed");
                io::ErrorKind::ConnectionAborted
            })?;
        self.waker.wake()?;
        rx.recv().map_err(|_| {
            debug!("EventLoopHandle::add_connection recv failed");
            io::ErrorKind::ConnectionAborted
        })?
    }

    // Signal EventLoop to shutdown.  Causes EventLoop::poll to return Ok(false).
    fn shutdown(&self) -> Result<()> {
        self.requests.push(Request::Shutdown).map_err(|_| {
            debug!("EventLoopHandle::shutdown send failed");
            io::ErrorKind::ConnectionAborted
        })?;
        self.waker.wake()
    }

    // Signal EventLoop to wake connection specified by `token` for processing.
    pub(crate) fn wake_connection(&self, token: Token) {
        if self.requests.push(Request::WakeConnection(token)).is_ok() {
            self.waker.wake().expect("wake failed");
        }
    }
}

// EventLoop owns all registered connections, and is responsible for calling each connection's
// `handle_event` or `handle_wake` any time a readiness or wake event associated with that connection is
// produced.
struct EventLoop {
    poll: Poll,
    events: Events,
    waker: Arc<Waker>,
    name: String,
    connections: Slab<Connection>,
    requests: Arc<RequestQueue<Request>>,
}

const EVENT_LOOP_INITIAL_CLIENTS: usize = 64; // Initial client allocation, exceeding this will cause the connection slab to grow.
const EVENT_LOOP_EVENTS_PER_ITERATION: usize = 256; // Number of events per poll() step, arbitrary limit.

impl EventLoop {
    fn new(name: String) -> Result<EventLoop> {
        let poll = Poll::new()?;
        let waker = Arc::new(Waker::new(poll.registry(), WAKE_TOKEN)?);
        let eventloop = EventLoop {
            poll,
            events: Events::with_capacity(EVENT_LOOP_EVENTS_PER_ITERATION),
            waker,
            name,
            connections: Slab::with_capacity(EVENT_LOOP_INITIAL_CLIENTS),
            requests: Arc::new(RequestQueue::new(EVENT_LOOP_INITIAL_CLIENTS)),
        };

        Ok(eventloop)
    }

    // Return a cloneable handle for controlling the EventLoop externally.
    fn handle(&mut self) -> EventLoopHandle {
        EventLoopHandle {
            waker: self.waker.clone(),
            requests: self.requests.new_sender(),
        }
    }

    // Register a connection and driver.
    fn add_connection(
        &mut self,
        connection: sys::Pipe,
        driver: Box<dyn Driver + Send>,
    ) -> Result<Token> {
        if self.connections.len() == self.connections.capacity() {
            trace!("{}: connection slab full, insert will allocate", self.name);
        }
        let entry = self.connections.vacant_entry();
        let token = Token(entry.key());
        assert_ne!(token, WAKE_TOKEN);
        let connection = Connection::new(connection, token, driver, self.poll.registry())?;
        debug!("{}: {:?}: new connection", self.name, token);
        entry.insert(connection);
        Ok(token)
    }

    // Step EventLoop once.  Call this in a loop from a dedicated thread.
    // Returns false if EventLoop is shutting down.
    // Each step may call `handle_event` on any registered connection that
    // has received readiness events from the poll wakeup.
    fn poll(&mut self) -> Result<bool> {
        loop {
            let r = self.poll.poll(&mut self.events, None);
            match r {
                Ok(()) => break,
                Err(ref e) if interrupted(e) => continue,
                Err(e) => return Err(e),
            }
        }

        for event in self.events.iter() {
            match event.token() {
                WAKE_TOKEN => {
                    debug!("{}: WAKE: wake event, will process requests", self.name);
                }
                token => {
                    debug!("{}: {:?}: connection event: {:?}", self.name, token, event);
                    let done = if let Some(connection) = self.connections.get_mut(token.0) {
                        match connection.handle_event(event, self.poll.registry()) {
                            Ok(done) => {
                                debug!("{}: connection {:?} done={}", self.name, token, done);
                                done
                            }
                            Err(e) => {
                                debug!("{}: {:?}: connection error: {:?}", self.name, token, e);
                                true
                            }
                        }
                    } else {
                        // Spurious event, log and ignore.
                        debug!(
                            "{}: {:?}: token not found in slab: {:?}",
                            self.name, token, event
                        );
                        false
                    };
                    if done {
                        debug!("{}: {:?}: done, removing", self.name, token);
                        let mut connection = self.connections.remove(token.0);
                        if let Err(e) = connection.shutdown(self.poll.registry()) {
                            debug!(
                                "{}: EventLoop drop - closing connection for {:?} failed: {:?}",
                                self.name, token, e
                            );
                        }
                    }
                }
            }
        }

        // If the waker was signalled there may be pending requests to process.
        while let Some(req) = self.requests.pop() {
            match req {
                Request::AddConnection(pipe, driver, tx) => {
                    debug!("{}: EventLoop: handling add_connection", self.name);
                    let r = self.add_connection(pipe, driver);
                    tx.send(r).expect("EventLoop::add_connection");
                }
                Request::Shutdown => {
                    debug!("{}: EventLoop: handling shutdown", self.name);
                    return Ok(false);
                }
                Request::WakeConnection(token) => {
                    debug!(
                        "{}: EventLoop: handling wake_connection {:?}",
                        self.name, token
                    );
                    let done = if let Some(connection) = self.connections.get_mut(token.0) {
                        match connection.handle_wake(self.poll.registry()) {
                            Ok(done) => done,
                            Err(e) => {
                                debug!("{}: {:?}: connection error: {:?}", self.name, token, e);
                                true
                            }
                        }
                    } else {
                        // Spurious wake, log and ignore.
                        debug!(
                            "{}: {:?}: token not found in slab: wake_connection",
                            self.name, token
                        );
                        false
                    };
                    if done {
                        debug!("{}: {:?}: done (wake), removing", self.name, token);
                        let mut connection = self.connections.remove(token.0);
                        if let Err(e) = connection.shutdown(self.poll.registry()) {
                            debug!(
                                "{}: EventLoop drop - closing connection for {:?} failed: {:?}",
                                self.name, token, e
                            );
                        }
                    }
                }
            }
        }

        Ok(true)
    }
}

impl Drop for EventLoop {
    fn drop(&mut self) {
        debug!("{}: EventLoop drop", self.name);
        for (token, connection) in &mut self.connections {
            debug!(
                "{}: EventLoop drop - closing connection for {:?}",
                self.name, token
            );
            if let Err(e) = connection.shutdown(self.poll.registry()) {
                debug!(
                    "{}: EventLoop drop - closing connection for {:?} failed: {:?}",
                    self.name, token, e
                );
            }
        }
        debug!("{}: EventLoop drop done", self.name);
    }
}

// Connection wraps an interprocess connection (Pipe) and manages
// receiving inbound and sending outbound buffers (and associated handles, if any).
// The associated driver is responsible for message framing and serialization.
struct Connection {
    io: sys::Pipe,
    token: Token,
    interest: Option<Interest>,
    inbound: sys::ConnectionBuffer,
    outbound: sys::ConnectionBuffer,
    driver: Box<dyn Driver + Send>,
}

pub(crate) const IPC_CLIENT_BUFFER_SIZE: usize = 16384;

impl Connection {
    fn new(
        mut io: sys::Pipe,
        token: Token,
        driver: Box<dyn Driver + Send>,
        registry: &Registry,
    ) -> Result<Connection> {
        let interest = Interest::READABLE;
        registry.register(&mut io, token, interest)?;
        Ok(Connection {
            io,
            token,
            interest: Some(interest),
            inbound: sys::ConnectionBuffer::with_capacity(IPC_CLIENT_BUFFER_SIZE),
            outbound: sys::ConnectionBuffer::with_capacity(IPC_CLIENT_BUFFER_SIZE),
            driver,
        })
    }

    fn shutdown(&mut self, registry: &Registry) -> Result<()> {
        trace!(
            "{:?}: connection shutdown interest={:?}",
            self.token,
            self.interest
        );
        let r = self.io.shutdown();
        trace!("{:?}: connection shutdown r={:?}", self.token, r);
        self.interest = None;
        registry.deregister(&mut self.io)
    }

    // Connections are always interested in READABLE.  clear_readable is only
    // called when the connection is in the process of shutting down.
    fn clear_readable(&mut self, registry: &Registry) -> Result<()> {
        self.update_registration(
            registry,
            self.interest.and_then(|i| i.remove(Interest::READABLE)),
        )
    }

    // Connections toggle WRITABLE based on the state of the `outbound` buffer.
    fn set_writable(&mut self, registry: &Registry) -> Result<()> {
        self.update_registration(
            registry,
            Some(
                self.interest
                    .map_or_else(|| Interest::WRITABLE, |i| i.add(Interest::WRITABLE)),
            ),
        )
    }

    fn clear_writable(&mut self, registry: &Registry) -> Result<()> {
        self.update_registration(
            registry,
            self.interest.and_then(|i| i.remove(Interest::WRITABLE)),
        )
    }

    // Update connection registration with the current readiness event interests.
    fn update_registration(
        &mut self,
        registry: &Registry,
        new_interest: Option<Interest>,
    ) -> Result<()> {
        // Note: Updating registration always triggers a writable event with NamedPipes, so
        // it's important to skip updating registration when the set of interests hasn't changed.
        if new_interest != self.interest {
            trace!(
                "{:?}: updating readiness registration old={:?} new={:?}",
                self.token,
                self.interest,
                new_interest
            );
            self.interest = new_interest;
            if let Some(interest) = self.interest {
                registry.reregister(&mut self.io, self.token, interest)?;
            } else {
                registry.deregister(&mut self.io)?;
            }
        }
        Ok(())
    }

    // Handle readiness event.  Errors returned are fatal for this connection, resulting in removal from the EventLoop connection list.
    // The EventLoop will call this for any connection that has received an event.
    fn handle_event(&mut self, event: &Event, registry: &Registry) -> Result<bool> {
        debug!("{:?}: handling event {:?}", self.token, event);
        assert_eq!(self.token, event.token());
        let done = if event.is_readable() {
            self.recv_inbound()?
        } else {
            trace!("{:?}: not readable", self.token);
            false
        };
        self.flush_outbound()?;
        if self.send_outbound(registry)? {
            // Hit EOF during send
            return Ok(true);
        }
        debug!(
            "{:?}: handling event done (recv done={}, outbound={})",
            self.token,
            done,
            self.outbound.is_empty()
        );
        let done = done && self.outbound.is_empty();
        // If driver is done and outbound is clear, unregister connection.
        if done {
            trace!("{:?}: driver done, clearing read interest", self.token);
            self.clear_readable(registry)?;
        }
        Ok(done)
    }

    // Handle wake event.  Errors returned are fatal for this connection, resulting in removal from the EventLoop connection list.
    // The EventLoop will call this to clear the outbound buffer for any connection that has received a wake event.
    fn handle_wake(&mut self, registry: &Registry) -> Result<bool> {
        debug!("{:?}: handling wake", self.token);
        self.flush_outbound()?;
        if self.send_outbound(registry)? {
            // Hit EOF during send
            return Ok(true);
        }
        debug!("{:?}: handling wake done", self.token);
        Ok(false)
    }

    fn recv_inbound(&mut self) -> Result<bool> {
        // If the connection is readable, read into inbound and pass to driver for processing until all ready data
        // has been consumed.
        loop {
            trace!("{:?}: pre-recv inbound: {:?}", self.token, self.inbound);
            let r = self.io.recv_msg(&mut self.inbound);
            match r {
                Ok(0) => {
                    trace!(
                        "{:?}: recv EOF unprocessed inbound={}",
                        self.token,
                        self.inbound.is_empty()
                    );
                    return Ok(true);
                }
                Ok(n) => {
                    trace!("{:?}: recv bytes: {}, process_inbound", self.token, n);
                    let r = self.driver.process_inbound(&mut self.inbound);
                    trace!("{:?}: process_inbound done: {:?}", self.token, r);
                    match r {
                        Ok(done) => {
                            if done {
                                return Ok(true);
                            }
                        }
                        Err(e) => {
                            debug!(
                                "{:?}: process_inbound error: {:?} unprocessed inbound={}",
                                self.token,
                                e,
                                self.inbound.is_empty()
                            );
                            return Err(e);
                        }
                    }
                }
                Err(ref e) if would_block(e) => {
                    trace!("{:?}: recv would_block: {:?}", self.token, e);
                    return Ok(false);
                }
                Err(ref e) if interrupted(e) => {
                    trace!("{:?}: recv interrupted: {:?}", self.token, e);
                    continue;
                }
                Err(e) => {
                    debug!("{:?}: recv error: {:?}", self.token, e);
                    return Err(e);
                }
            }
        }
    }

    fn flush_outbound(&mut self) -> Result<()> {
        // Enqueue outbound messages to the outbound buffer, then try to write out to connection.
        // There may be outbound messages even if there was no inbound processing, so always attempt
        // to enqueue and flush.
        trace!("{:?}: flush_outbound", self.token);
        let r = self.driver.flush_outbound(&mut self.outbound);
        trace!("{:?}: flush_outbound done: {:?}", self.token, r);
        if let Err(e) = r {
            debug!("{:?}: flush_outbound error: {:?}", self.token, e);
            return Err(e);
        }
        Ok(())
    }

    fn send_outbound(&mut self, registry: &Registry) -> Result<bool> {
        // Attempt to flush outbound buffer.  If the connection's write buffer is full, register for WRITABLE
        // and complete flushing when associated notitication arrives later.
        while !self.outbound.is_empty() {
            let r = self.io.send_msg(&mut self.outbound);
            match r {
                Ok(0) => {
                    trace!("{:?}: send EOF", self.token);
                    return Ok(true);
                }
                Ok(n) => {
                    trace!("{:?}: send bytes: {}", self.token, n);
                }
                Err(ref e) if would_block(e) => {
                    trace!(
                        "{:?}: send would_block: {:?}, setting write interest",
                        self.token,
                        e
                    );
                    // Register for write events.
                    self.set_writable(registry)?;
                    break;
                }
                Err(ref e) if interrupted(e) => {
                    trace!("{:?}: send interrupted: {:?}", self.token, e);
                    continue;
                }
                Err(e) => {
                    debug!("{:?}: send error: {:?}", self.token, e);
                    return Err(e);
                }
            }
            trace!("{:?}: post-send: outbound {:?}", self.token, self.outbound);
        }
        // Outbound buffer flushed, clear registration for WRITABLE.
        if self.outbound.is_empty() {
            trace!("{:?}: outbound empty, clearing write interest", self.token);
            self.clear_writable(registry)?;
        }
        Ok(false)
    }
}

impl Drop for Connection {
    fn drop(&mut self) {
        debug!("{:?}: Connection drop", self.token);
    }
}

fn would_block(err: &std::io::Error) -> bool {
    err.kind() == std::io::ErrorKind::WouldBlock
}

fn interrupted(err: &std::io::Error) -> bool {
    err.kind() == std::io::ErrorKind::Interrupted
}

// Driver only has a single implementation, but must be hidden behind a Trait object to
// hide the varying FramedDriver sizes (due to different `T` values).
trait Driver {
    // Handle inbound messages.  Returns true if Driver is done; this will trigger Connection removal and cleanup.
    fn process_inbound(&mut self, inbound: &mut sys::ConnectionBuffer) -> Result<bool>;

    // Write outbound messages to `outbound`.
    fn flush_outbound(&mut self, outbound: &mut sys::ConnectionBuffer) -> Result<()>;
}

// Length-delimited connection framing and (de)serialization is handled by the inbound and outbound processing.
// Handlers can then process message Requests and Responses without knowledge of serialization or
// handle remoting.
impl<T> Driver for FramedDriver<T>
where
    T: Handler,
    T::In: DeserializeOwned + Debug + AssociateHandleForMessage,
    T::Out: Serialize + Debug + AssociateHandleForMessage,
{
    // Caller passes `inbound` data, this function will trim any complete messages from `inbound` and pass them to the handler for processing.
    fn process_inbound(&mut self, inbound: &mut sys::ConnectionBuffer) -> Result<bool> {
        debug!("process_inbound: {:?}", inbound);

        // Repeatedly call `decode` as long as it produces items, passing each produced item to the handler to action.
        #[allow(unused_mut)]
        while let Some(mut item) = self.codec.decode(&mut inbound.buf)? {
            if item.has_associated_handle() {
                // On Unix, dequeue a handle from the connection and update the item's handle.
                #[cfg(unix)]
                {
                    let new = inbound
                        .pop_handle()
                        .expect("inbound handle expected for item");
                    unsafe { item.set_local_handle(new.take()) };
                }
                // On Windows, the deserialized item contains the correct handle value, so
                // convert it to an owned handle on the item.
                #[cfg(windows)]
                {
                    assert!(inbound.pop_handle().is_none());
                    unsafe { item.set_local_handle() };
                }
            }

            self.handler.consume(item)?;
        }

        Ok(false)
    }

    // Caller will try to write `outbound` to associated connection, queuing any data that can't be transmitted immediately.
    fn flush_outbound(&mut self, outbound: &mut sys::ConnectionBuffer) -> Result<()> {
        debug!("flush_outbound: {:?}", outbound.buf);

        // Repeatedly grab outgoing items from the handler, passing each to `encode` for serialization into `outbound`.
        while let Some(mut item) = self.handler.produce()? {
            let handle = if item.has_associated_handle() {
                #[allow(unused_mut)]
                let mut handle = item.take_handle();
                // On Windows, the handle is transferred by duplicating it into the target remote process.
                #[cfg(windows)]
                unsafe {
                    item.set_remote_handle(handle.send_to_target()?);
                }
                Some(handle)
            } else {
                None
            };

            self.codec.encode(item, &mut outbound.buf)?;
            if let Some(handle) = handle {
                // `outbound` retains ownership of the handle until the associated
                // encoded item in `outbound.buf` is sent to the remote process.
                outbound.push_handle(handle);
            }
        }
        Ok(())
    }
}

struct FramedDriver<T: Handler> {
    codec: LengthDelimitedCodec<T::Out, T::In>,
    handler: T,
}

impl<T: Handler> FramedDriver<T> {
    fn new(handler: T) -> FramedDriver<T> {
        FramedDriver {
            codec: Default::default(),
            handler,
        }
    }
}

#[derive(Debug)]
pub struct EventLoopThread {
    thread: Option<thread::JoinHandle<Result<()>>>,
    name: String,
    handle: EventLoopHandle,
}

impl EventLoopThread {
    pub fn new<F1, F2>(
        name: String,
        stack_size: Option<usize>,
        after_start: F1,
        before_stop: F2,
    ) -> Result<Self>
    where
        F1: Fn() + Send + 'static,
        F2: Fn() + Send + 'static,
    {
        let mut event_loop = EventLoop::new(name.clone())?;
        let handle = event_loop.handle();

        let builder = thread::Builder::new()
            .name(name.clone())
            .stack_size(stack_size.unwrap_or(64 * 4096));

        let thread = builder.spawn(move || {
            trace!("{}: event loop thread enter", event_loop.name);
            after_start();
            let _thread_exit_guard = scopeguard::guard((), |_| before_stop());

            let r = loop {
                let start = std::time::Instant::now();
                let r = event_loop.poll();
                trace!(
                    "{}: event loop poll r={:?}, took={}μs",
                    event_loop.name,
                    r,
                    start.elapsed().as_micros()
                );
                match r {
                    Ok(true) => continue,
                    _ => break r,
                }
            };

            trace!("{}: event loop thread exit", event_loop.name);
            r.map(|_| ())
        })?;

        Ok(EventLoopThread {
            thread: Some(thread),
            name,
            handle,
        })
    }

    pub fn handle(&self) -> &EventLoopHandle {
        &self.handle
    }
}

impl Drop for EventLoopThread {
    // Shut down event loop and executor thread.  Blocks until complete.
    fn drop(&mut self) {
        trace!("{}: EventLoopThread shutdown", self.name);
        if let Err(e) = self.handle.shutdown() {
            debug!("{}: initiating shutdown failed: {:?}", self.name, e);
        }
        let thread = self.thread.take().expect("event loop thread");
        if let Err(e) = thread.join() {
            error!("{}: EventLoopThread failed: {:?}", self.name, e);
        }
        trace!("{}: EventLoopThread shutdown done", self.name);
    }
}

#[cfg(test)]
mod test {
    use serde_derive::{Deserialize, Serialize};

    use super::*;

    #[derive(Debug, Serialize, Deserialize, PartialEq)]
    enum TestServerMessage {
        TestRequest,
    }
    impl AssociateHandleForMessage for TestServerMessage {}

    struct TestServerImpl {}

    impl Server for TestServerImpl {
        type ServerMessage = TestServerMessage;
        type ClientMessage = TestClientMessage;

        fn process(&mut self, req: Self::ServerMessage) -> Self::ClientMessage {
            assert_eq!(req, TestServerMessage::TestRequest);
            TestClientMessage::TestResponse
        }
    }

    #[derive(Debug, Serialize, Deserialize, PartialEq)]
    enum TestClientMessage {
        TestResponse,
    }

    impl AssociateHandleForMessage for TestClientMessage {}

    struct TestClientImpl {}

    impl Client for TestClientImpl {
        type ServerMessage = TestServerMessage;
        type ClientMessage = TestClientMessage;
    }

    fn init() {
        let _ = env_logger::builder().is_test(true).try_init();
    }

    fn setup() -> (
        EventLoopThread,
        EventLoopThread,
        Proxy<TestServerMessage, TestClientMessage>,
    ) {
        // Server setup and registration.
        let server = EventLoopThread::new("test-server".to_string(), None, || {}, || {})
            .expect("server EventLoopThread");
        let server_handle = server.handle();

        let (server_pipe, client_pipe) = sys::make_pipe_pair().expect("server make_pipe_pair");
        server_handle
            .bind_server(TestServerImpl {}, server_pipe)
            .expect("server bind_server");

        // Client setup and registration.
        let client = EventLoopThread::new("test-client".to_string(), None, || {}, || {})
            .expect("client EventLoopThread");
        let client_handle = client.handle();

        let client_pipe = unsafe { sys::Pipe::from_raw_handle(client_pipe) };
        let client_proxy = client_handle
            .bind_client::<TestClientImpl>(client_pipe)
            .expect("client bind_client");

        (server, client, client_proxy)
    }

    // Verify basic EventLoopThread functionality works.  Create a server and client EventLoopThread, then send
    // a single message from the client to the server and wait for the expected response.
    #[test]
    fn basic() {
        init();
        let (server, client, client_proxy) = setup();

        // RPC message from client to server.
        let response = client_proxy.call(TestServerMessage::TestRequest);
        let response = response.expect("client response");
        assert_eq!(response, TestClientMessage::TestResponse);

        // Explicit shutdown.
        drop(client);
        drop(server);
    }

    // Same as `basic`, but shut down server before client.
    #[test]
    fn basic_reverse_drop_order() {
        init();
        let (server, client, client_proxy) = setup();

        // RPC message from client to server.
        let response = client_proxy.call(TestServerMessage::TestRequest);
        let response = response.expect("client response");
        assert_eq!(response, TestClientMessage::TestResponse);

        // Explicit shutdown.
        drop(server);
        drop(client);
    }

    #[test]
    fn dead_server() {
        init();
        let (server, _client, client_proxy) = setup();
        drop(server);

        let response = client_proxy.call(TestServerMessage::TestRequest);
        response.expect_err("sending on closed channel");
    }

    #[test]
    fn dead_client() {
        init();
        let (_server, client, client_proxy) = setup();
        drop(client);

        let response = client_proxy.call(TestServerMessage::TestRequest);
        response.expect_err("sending on a closed channel");
    }

    #[test]
    fn disconnected_handle() {
        init();
        let server = EventLoopThread::new("test-server".to_string(), None, || {}, || {})
            .expect("server EventLoopThread");
        let server_handle = server.handle().clone();
        drop(server);

        server_handle
            .shutdown()
            .expect_err("sending on closed channel");
    }

    #[test]
    fn clone_after_drop() {
        init();
        let (server, client, client_proxy) = setup();
        drop(server);
        drop(client);

        let clone = client_proxy.clone();
        let response = clone.call(TestServerMessage::TestRequest);
        response.expect_err("sending to a dropped ClientHandler");
    }

    #[test]
    fn basic_event_loop_thread_callbacks() {
        init();
        let (start_tx, start_rx) = mpsc::channel();
        let (stop_tx, stop_rx) = mpsc::channel();

        let elt = EventLoopThread::new(
            "test-thread-callbacks".to_string(),
            None,
            move || start_tx.send(()).unwrap(),
            move || stop_tx.send(()).unwrap(),
        )
        .expect("server EventLoopThread");

        start_rx.recv().expect("after_start callback done");

        drop(elt);

        stop_rx.recv().expect("before_stop callback done");
    }
}
