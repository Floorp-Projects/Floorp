extern crate futures;

use std::mem;
use std::sync::Arc;
use std::rc::Rc;
use std::cell::{Cell, RefCell};
use std::sync::atomic::{Ordering, AtomicBool};

use futures::{Poll, Async, Future, AsyncSink, StartSend};
use futures::future::ok;
use futures::stream;
use futures::sync::{oneshot, mpsc};
use futures::task::{self, Task};
use futures::executor::{self, Unpark};
use futures::sink::*;

mod support;
use support::*;

#[test]
fn vec_sink() {
    let mut v = Vec::new();
    assert_eq!(v.start_send(0), Ok(AsyncSink::Ready));
    assert_eq!(v.start_send(1), Ok(AsyncSink::Ready));
    assert_eq!(v, vec![0, 1]);
    assert_done(move || v.flush(), Ok(vec![0, 1]));
}

#[test]
fn send() {
    let v = Vec::new();

    let v = v.send(0).wait().unwrap();
    assert_eq!(v, vec![0]);

    let v = v.send(1).wait().unwrap();
    assert_eq!(v, vec![0, 1]);

    assert_done(move || v.send(2),
                Ok(vec![0, 1, 2]));
}

#[test]
fn send_all() {
    let v = Vec::new();

    let (v, _) = v.send_all(stream::iter(vec![Ok(0), Ok(1)])).wait().unwrap();
    assert_eq!(v, vec![0, 1]);

    let (v, _) = v.send_all(stream::iter(vec![Ok(2), Ok(3)])).wait().unwrap();
    assert_eq!(v, vec![0, 1, 2, 3]);

    assert_done(
        move || v.send_all(stream::iter(vec![Ok(4), Ok(5)])).map(|(v, _)| v),
        Ok(vec![0, 1, 2, 3, 4, 5]));
}

// An Unpark struct that records unpark events for inspection
struct Flag(pub AtomicBool);

impl Flag {
    fn new() -> Arc<Flag> {
        Arc::new(Flag(AtomicBool::new(false)))
    }

    fn get(&self) -> bool {
        self.0.load(Ordering::SeqCst)
    }

    fn set(&self, v: bool) {
        self.0.store(v, Ordering::SeqCst)
    }
}

impl Unpark for Flag {
    fn unpark(&self) {
        self.set(true)
    }
}

// Sends a value on an i32 channel sink
struct StartSendFut<S: Sink>(Option<S>, Option<S::SinkItem>);

impl<S: Sink> StartSendFut<S> {
    fn new(sink: S, item: S::SinkItem) -> StartSendFut<S> {
        StartSendFut(Some(sink), Some(item))
    }
}

impl<S: Sink> Future for StartSendFut<S> {
    type Item = S;
    type Error = S::SinkError;

    fn poll(&mut self) -> Poll<S, S::SinkError> {
        match try!(self.0.as_mut().unwrap().start_send(self.1.take().unwrap())) {
            AsyncSink::Ready => Ok(Async::Ready(self.0.take().unwrap())),
            AsyncSink::NotReady(item) => {
                self.1 = Some(item);
                Ok(Async::NotReady)
            }
        }

    }
}

#[test]
// Test that `start_send` on an `mpsc` channel does indeed block when the
// channel is full
fn mpsc_blocking_start_send() {
    let (mut tx, mut rx) = mpsc::channel::<i32>(0);

    futures::future::lazy(|| {
        assert_eq!(tx.start_send(0).unwrap(), AsyncSink::Ready);

        let flag = Flag::new();
        let mut task = executor::spawn(StartSendFut::new(tx, 1));

        assert!(task.poll_future(flag.clone()).unwrap().is_not_ready());
        assert!(!flag.get());
        sassert_next(&mut rx, 0);
        assert!(flag.get());
        flag.set(false);
        assert!(task.poll_future(flag.clone()).unwrap().is_ready());
        assert!(!flag.get());
        sassert_next(&mut rx, 1);

        Ok::<(), ()>(())
    }).wait().unwrap();
}

#[test]
// test `flush` by using `with` to make the first insertion into a sink block
// until a oneshot is completed
fn with_flush() {
    let (tx, rx) = oneshot::channel();
    let mut block = rx.boxed();
    let mut sink = Vec::new().with(|elem| {
        mem::replace(&mut block, ok(()).boxed())
            .map(move |_| elem + 1).map_err(|_| -> () { panic!() })
    });

    assert_eq!(sink.start_send(0), Ok(AsyncSink::Ready));

    let flag = Flag::new();
    let mut task = executor::spawn(sink.flush());
    assert!(task.poll_future(flag.clone()).unwrap().is_not_ready());
    tx.send(()).unwrap();
    assert!(flag.get());

    let sink = match task.poll_future(flag.clone()).unwrap() {
        Async::Ready(sink) => sink,
        _ => panic!()
    };

    assert_eq!(sink.send(1).wait().unwrap().get_ref(), &[1, 2]);
}

#[test]
// test simple use of with to change data
fn with_as_map() {
    let sink = Vec::new().with(|item| -> Result<i32, ()> {
        Ok(item * 2)
    });
    let sink = sink.send(0).wait().unwrap();
    let sink = sink.send(1).wait().unwrap();
    let sink = sink.send(2).wait().unwrap();
    assert_eq!(sink.get_ref(), &[0, 2, 4]);
}

// Immediately accepts all requests to start pushing, but completion is managed
// by manually flushing
struct ManualFlush<T> {
    data: Vec<T>,
    waiting_tasks: Vec<Task>,
}

impl<T> Sink for ManualFlush<T> {
    type SinkItem = Option<T>; // Pass None to flush
    type SinkError = ();

    fn start_send(&mut self, op: Option<T>) -> StartSend<Option<T>, ()> {
        if let Some(item) = op {
            self.data.push(item);
        } else {
            self.force_flush();
        }
        Ok(AsyncSink::Ready)
    }

    fn poll_complete(&mut self) -> Poll<(), ()> {
        if self.data.is_empty() {
            Ok(Async::Ready(()))
        } else {
            self.waiting_tasks.push(task::park());
            Ok(Async::NotReady)
        }
    }

    fn close(&mut self) -> Poll<(), ()> {
        Ok(().into())
    }
}

impl<T> ManualFlush<T> {
    fn new() -> ManualFlush<T> {
        ManualFlush {
            data: Vec::new(),
            waiting_tasks: Vec::new()
        }
    }

    fn force_flush(&mut self) -> Vec<T> {
        for task in self.waiting_tasks.drain(..) {
            task.unpark()
        }
        mem::replace(&mut self.data, Vec::new())
    }
}

#[test]
// test that the `with` sink doesn't require the underlying sink to flush,
// but doesn't claim to be flushed until the underlyig sink is
fn with_flush_propagate() {
    let mut sink = ManualFlush::new().with(|x| -> Result<Option<i32>, ()> { Ok(x) });
    assert_eq!(sink.start_send(Some(0)).unwrap(), AsyncSink::Ready);
    assert_eq!(sink.start_send(Some(1)).unwrap(), AsyncSink::Ready);

    let flag = Flag::new();
    let mut task = executor::spawn(sink.flush());
    assert!(task.poll_future(flag.clone()).unwrap().is_not_ready());
    assert!(!flag.get());
    assert_eq!(task.get_mut().get_mut().get_mut().force_flush(), vec![0, 1]);
    assert!(flag.get());
    assert!(task.poll_future(flag.clone()).unwrap().is_ready());
}

#[test]
// test that a buffer is a no-nop around a sink that always accepts sends
fn buffer_noop() {
    let sink = Vec::new().buffer(0);
    let sink = sink.send(0).wait().unwrap();
    let sink = sink.send(1).wait().unwrap();
    assert_eq!(sink.get_ref(), &[0, 1]);

    let sink = Vec::new().buffer(1);
    let sink = sink.send(0).wait().unwrap();
    let sink = sink.send(1).wait().unwrap();
    assert_eq!(sink.get_ref(), &[0, 1]);
}

struct ManualAllow<T> {
    data: Vec<T>,
    allow: Rc<Allow>,
}

struct Allow {
    flag: Cell<bool>,
    tasks: RefCell<Vec<Task>>,
}

impl Allow {
    fn new() -> Allow {
        Allow {
            flag: Cell::new(false),
            tasks: RefCell::new(Vec::new()),
        }
    }

    fn check(&self) -> bool {
        if self.flag.get() {
            true
        } else {
            self.tasks.borrow_mut().push(task::park());
            false
        }
    }

    fn start(&self) {
        self.flag.set(true);
        let mut tasks = self.tasks.borrow_mut();
        for task in tasks.drain(..) {
            task.unpark();
        }
    }
}

impl<T> Sink for ManualAllow<T> {
    type SinkItem = T;
    type SinkError = ();

    fn start_send(&mut self, item: T) -> StartSend<T, ()> {
        if self.allow.check() {
            self.data.push(item);
            Ok(AsyncSink::Ready)
        } else {
            Ok(AsyncSink::NotReady(item))
        }
    }

    fn poll_complete(&mut self) -> Poll<(), ()> {
        Ok(Async::Ready(()))
    }

    fn close(&mut self) -> Poll<(), ()> {
        Ok(().into())
    }
}

fn manual_allow<T>() -> (ManualAllow<T>, Rc<Allow>) {
    let allow = Rc::new(Allow::new());
    let manual_allow = ManualAllow {
        data: Vec::new(),
        allow: allow.clone(),
    };
    (manual_allow, allow)
}

#[test]
// test basic buffer functionality, including both filling up to capacity,
// and writing out when the underlying sink is ready
fn buffer() {
    let (sink, allow) = manual_allow::<i32>();
    let sink = sink.buffer(2);

    let sink = StartSendFut::new(sink, 0).wait().unwrap();
    let sink = StartSendFut::new(sink, 1).wait().unwrap();

    let flag = Flag::new();
    let mut task = executor::spawn(sink.send(2));
    assert!(task.poll_future(flag.clone()).unwrap().is_not_ready());
    assert!(!flag.get());
    allow.start();
    assert!(flag.get());
    match task.poll_future(flag.clone()).unwrap() {
        Async::Ready(sink) => {
            assert_eq!(sink.get_ref().data, vec![0, 1, 2]);
        }
        _ => panic!()
    }
}

#[test]
fn map_err() {
    {
        let (tx, _rx) = mpsc::channel(1);
        let mut tx = tx.sink_map_err(|_| ());
        assert_eq!(tx.start_send(()), Ok(AsyncSink::Ready));
        assert_eq!(tx.poll_complete(), Ok(Async::Ready(())));
    }

    let tx = mpsc::channel(0).0;
    assert_eq!(tx.sink_map_err(|_| ()).start_send(()), Err(()));
}

#[derive(Copy, Clone, Debug, PartialEq, Eq)]
struct FromErrTest;

impl<T> From<mpsc::SendError<T>> for FromErrTest {
    fn from(_: mpsc::SendError<T>) -> FromErrTest {
        FromErrTest
    }
}

#[test]
fn from_err() {
    {
        let (tx, _rx) = mpsc::channel(1);
        let mut tx: SinkFromErr<mpsc::Sender<()>, FromErrTest> = tx.sink_from_err();
        assert_eq!(tx.start_send(()), Ok(AsyncSink::Ready));
        assert_eq!(tx.poll_complete(), Ok(Async::Ready(())));
    }

    let tx = mpsc::channel(0).0;
    assert_eq!(tx.sink_from_err().start_send(()), Err(FromErrTest));
}
