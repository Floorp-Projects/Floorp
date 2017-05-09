use std::prelude::v1::*;

use std::cell::Cell;
use std::fmt;
use std::mem;
use std::sync::Arc;
use std::sync::atomic::{Ordering, AtomicBool, AtomicUsize, ATOMIC_USIZE_INIT};
use std::thread;

use {Poll, Future, Async, Stream, Sink, StartSend, AsyncSink};
use future::BoxFuture;

mod unpark_mutex;
use self::unpark_mutex::UnparkMutex;

mod task_rc;
mod data;
#[allow(deprecated)]
#[cfg(feature = "with-deprecated")]
pub use self::task_rc::TaskRc;
pub use self::data::LocalKey;

struct BorrowedTask<'a> {
    id: usize,
    unpark: &'a Arc<Unpark>,
    map: &'a data::LocalMap,
    events: Events,
}

thread_local!(static CURRENT_TASK: Cell<*const BorrowedTask<'static>> = {
    Cell::new(0 as *const _)
});

fn fresh_task_id() -> usize {
    // TODO: this assert is a real bummer, need to figure out how to reuse
    //       old IDs that are no longer in use.
    static NEXT_ID: AtomicUsize = ATOMIC_USIZE_INIT;
    let id = NEXT_ID.fetch_add(1, Ordering::Relaxed);
    assert!(id < usize::max_value() / 2,
            "too many previous tasks have been allocated");
    id
}

fn set<'a, F, R>(task: &BorrowedTask<'a>, f: F) -> R
    where F: FnOnce() -> R
{
    struct Reset(*const BorrowedTask<'static>);
    impl Drop for Reset {
        fn drop(&mut self) {
            CURRENT_TASK.with(|c| c.set(self.0));
        }
    }

    CURRENT_TASK.with(move |c| {
        let _reset = Reset(c.get());
        let task = unsafe {
            mem::transmute::<&BorrowedTask<'a>,
                             *const BorrowedTask<'static>>(task)
        };
        c.set(task);
        f()
    })
}

fn with<F: FnOnce(&BorrowedTask) -> R, R>(f: F) -> R {
    let task = CURRENT_TASK.with(|c| c.get());
    assert!(!task.is_null(), "no Task is currently running");
    unsafe {
        f(&*task)
    }
}

/// A handle to a "task", which represents a single lightweight "thread" of
/// execution driving a future to completion.
///
/// In general, futures are composed into large units of work, which are then
/// spawned as tasks onto an *executor*. The executor is responsible for polling
/// the future as notifications arrive, until the future terminates.
///
/// This is obtained by the `task::park` function.
#[derive(Clone)]
pub struct Task {
    id: usize,
    unpark: Arc<Unpark>,
    events: Events,
}

fn _assert_kinds() {
    fn _assert_send<T: Send>() {}
    _assert_send::<Task>();
}

/// Returns a handle to the current task to call `unpark` at a later date.
///
/// This function is similar to the standard library's `thread::park` function
/// except that it won't block the current thread but rather the current future
/// that is being executed.
///
/// The returned handle implements the `Send` and `'static` bounds and may also
/// be cheaply cloned. This is useful for squirreling away the handle into a
/// location which is then later signaled that a future can make progress.
///
/// Implementations of the `Future` trait typically use this function if they
/// would otherwise perform a blocking operation. When something isn't ready
/// yet, this `park` function is called to acquire a handle to the current
/// task, and then the future arranges it such that when the block operation
/// otherwise finishes (perhaps in the background) it will `unpark` the returned
/// handle.
///
/// It's sometimes necessary to pass extra information to the task when
/// unparking it, so that the task knows something about *why* it was woken. See
/// the `with_unpark_event` for details on how to do this.
///
/// # Panics
///
/// This function will panic if a task is not currently being executed. That
/// is, this method can be dangerous to call outside of an implementation of
/// `poll`.
pub fn park() -> Task {
    with(|task| {
        Task {
            id: task.id,
            events: task.events.clone(),
            unpark: task.unpark.clone(),
        }
    })
}

impl Task {
    /// Indicate that the task should attempt to poll its future in a timely
    /// fashion.
    ///
    /// It's typically guaranteed that, for each call to `unpark`, `poll` will
    /// be called at least once subsequently (unless the task has terminated).
    /// If the task is currently polling its future when `unpark` is called, it
    /// must poll the future *again* afterwards, ensuring that all relevant
    /// events are eventually observed by the future.
    pub fn unpark(&self) {
        self.events.trigger();
        self.unpark.unpark();
    }

    /// Returns `true` when called from within the context of the task. In
    /// other words, the task is currently running on the thread calling the
    /// function.
    pub fn is_current(&self) -> bool {
        with(|current| current.id == self.id)
    }
}

impl fmt::Debug for Task {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("Task")
         .field("id", &self.id)
         .finish()
    }
}

/// For the duration of the given callback, add an "unpark event" to be
/// triggered when the task handle is used to unpark the task.
///
/// Unpark events are used to pass information about what event caused a task to
/// be unparked. In some cases, tasks are waiting on a large number of possible
/// events, and need precise information about the wakeup to avoid extraneous
/// polling.
///
/// Every `Task` handle comes with a set of unpark events which will fire when
/// `unpark` is called. When fired, these events insert an identifer into a
/// concurrent set, which the task can read from to determine what events
/// occurred.
///
/// This function immediately invokes the closure, `f`, but arranges things so
/// that `task::park` will produce a `Task` handle that includes the given
/// unpark event.
///
/// # Panics
///
/// This function will panic if a task is not currently being executed. That
/// is, this method can be dangerous to call outside of an implementation of
/// `poll`.
pub fn with_unpark_event<F, R>(event: UnparkEvent, f: F) -> R
    where F: FnOnce() -> R
{
    with(|task| {
        let new_task = BorrowedTask {
            id: task.id,
            unpark: task.unpark,
            events: task.events.with_event(event),
            map: task.map,
        };
        set(&new_task, f)
    })
}

#[derive(Clone)]
/// A set insertion to trigger upon `unpark`.
///
/// Unpark events are used to communicate information about *why* an unpark
/// occured, in particular populating sets with event identifiers so that the
/// unparked task can avoid extraneous polling. See `with_unpark_event` for
/// more.
pub struct UnparkEvent {
    set: Arc<EventSet>,
    item: usize,
}

impl UnparkEvent {
    /// Construct an unpark event that will insert `id` into `set` when
    /// triggered.
    pub fn new(set: Arc<EventSet>, id: usize) -> UnparkEvent {
        UnparkEvent {
            set: set,
            item: id,
        }
    }
}

impl fmt::Debug for UnparkEvent {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("UnparkEvent")
         .field("set", &"...")
         .field("item", &self.item)
         .finish()
    }
}

/// A concurrent set which allows for the insertion of `usize` values.
///
/// `EventSet`s are used to communicate precise information about the event(s)
/// that trigged a task notification. See `task::with_unpark_event` for details.
pub trait EventSet: Send + Sync + 'static {
    /// Insert the given ID into the set
    fn insert(&self, id: usize);
}

// A collection of UnparkEvents to trigger on `unpark`
#[derive(Clone)]
enum Events {
    Zero,
    One(UnparkEvent),
    Lots(Vec<UnparkEvent>),
}

impl Events {
    fn new() -> Events {
        Events::Zero
    }

    fn trigger(&self) {
        match *self {
            Events::Zero => {}
            Events::One(ref event) => event.set.insert(event.item),
            Events::Lots(ref list) => {
                for event in list {
                    event.set.insert(event.item);
                }
            }
        }
    }

    fn with_event(&self, event: UnparkEvent) -> Events {
        let mut list = match *self {
            Events::Zero => return Events::One(event),
            Events::One(ref event) => vec![event.clone()],
            Events::Lots(ref list) => list.clone(),
        };
        list.push(event);
        Events::Lots(list)
    }
}

/// Representation of a spawned future/stream.
///
/// This object is returned by the `spawn` function in this module. This
/// represents a "fused task and future", storing all necessary pieces of a task
/// and owning the top-level future that's being driven as well.
///
/// A `Spawn` can be poll'd for completion or execution of the current thread
/// can be blocked indefinitely until a notification arrives. This can be used
/// with either futures or streams, with different methods being available on
/// `Spawn` depending which is used.
pub struct Spawn<T> {
    obj: T,
    id: usize,
    data: data::LocalMap,
}

/// Spawns a new future, returning the fused future and task.
///
/// This function is the termination endpoint for running futures. This method
/// will conceptually allocate a new task to run the given object, which is
/// normally either a `Future` or `Stream`.
///
/// This function is similar to the `thread::spawn` function but does not
/// attempt to run code in the background. The future will not make progress
/// until the methods on `Spawn` are called in turn.
pub fn spawn<T>(obj: T) -> Spawn<T> {
    Spawn {
        obj: obj,
        id: fresh_task_id(),
        data: data::local_map(),
    }
}

impl<T> Spawn<T> {
    /// Get a shared reference to the object the Spawn is wrapping.
    pub fn get_ref(&self) -> &T {
        &self.obj
    }

    /// Get a mutable reference to the object the Spawn is wrapping.
    pub fn get_mut(&mut self) -> &mut T {
        &mut self.obj
    }

    /// Consume the Spawn, returning its inner object
    pub fn into_inner(self) -> T {
        self.obj
    }
}

impl<F: Future> Spawn<F> {
    /// Polls the internal future, scheduling notifications to be sent to the
    /// `unpark` argument.
    ///
    /// This method will poll the internal future, testing if it's completed
    /// yet. The `unpark` argument is used as a sink for notifications sent to
    /// this future. That is, while the future is being polled, any call to
    /// `task::park()` will return a handle that contains the `unpark`
    /// specified.
    ///
    /// If this function returns `NotReady`, then the `unpark` should have been
    /// scheduled to receive a notification when poll can be called again.
    /// Otherwise if `Ready` or `Err` is returned, the `Spawn` task can be
    /// safely destroyed.
    pub fn poll_future(&mut self, unpark: Arc<Unpark>) -> Poll<F::Item, F::Error> {
        self.enter(&unpark, |f| f.poll())
    }

    /// Waits for the internal future to complete, blocking this thread's
    /// execution until it does.
    ///
    /// This function will call `poll_future` in a loop, waiting for the future
    /// to complete. When a future cannot make progress it will use
    /// `thread::park` to block the current thread.
    pub fn wait_future(&mut self) -> Result<F::Item, F::Error> {
        let unpark = Arc::new(ThreadUnpark::new(thread::current()));
        loop {
            match try!(self.poll_future(unpark.clone())) {
                Async::NotReady => unpark.park(),
                Async::Ready(e) => return Ok(e),
            }
        }
    }

    /// A specialized function to request running a future to completion on the
    /// specified executor.
    ///
    /// This function only works for futures whose item and error types are `()`
    /// and also implement the `Send` and `'static` bounds. This will submit
    /// units of work (instances of `Run`) to the `exec` argument provided
    /// necessary to drive the future to completion.
    ///
    /// When the future would block, it's arranged that when the future is again
    /// ready it will submit another unit of work to the `exec` provided. This
    /// will happen in a loop until the future has completed.
    ///
    /// This method is not appropriate for all futures, and other kinds of
    /// executors typically provide a similar function with perhaps relaxed
    /// bounds as well.
    pub fn execute(self, exec: Arc<Executor>)
        where F: Future<Item=(), Error=()> + Send + 'static,
    {
        exec.clone().execute(Run {
            // Ideally this method would be defined directly on
            // `Spawn<BoxFuture<(), ()>>` so we wouldn't have to box here and
            // it'd be more explicit, but unfortunately that currently has a
            // link error on nightly: rust-lang/rust#36155
            spawn: Spawn {
                id: self.id,
                data: self.data,
                obj: self.obj.boxed(),
            },
            inner: Arc::new(Inner {
                exec: exec,
                mutex: UnparkMutex::new()
            }),
        })
    }
}

impl<S: Stream> Spawn<S> {
    /// Like `poll_future`, except polls the underlying stream.
    pub fn poll_stream(&mut self, unpark: Arc<Unpark>)
                       -> Poll<Option<S::Item>, S::Error> {
        self.enter(&unpark, |stream| stream.poll())
    }

    /// Like `wait_future`, except only waits for the next element to arrive on
    /// the underlying stream.
    pub fn wait_stream(&mut self) -> Option<Result<S::Item, S::Error>> {
        let unpark = Arc::new(ThreadUnpark::new(thread::current()));
        loop {
            match self.poll_stream(unpark.clone()) {
                Ok(Async::NotReady) => unpark.park(),
                Ok(Async::Ready(Some(e))) => return Some(Ok(e)),
                Ok(Async::Ready(None)) => return None,
                Err(e) => return Some(Err(e)),
            }
        }
    }
}

impl<S: Sink> Spawn<S> {
    /// Invokes the underlying `start_send` method with this task in place.
    ///
    /// If the underlying operation returns `NotReady` then the `unpark` value
    /// passed in will receive a notification when the operation is ready to be
    /// attempted again.
    pub fn start_send(&mut self, value: S::SinkItem, unpark: &Arc<Unpark>)
                       -> StartSend<S::SinkItem, S::SinkError> {
        self.enter(unpark, |sink| sink.start_send(value))
    }

    /// Invokes the underlying `poll_complete` method with this task in place.
    ///
    /// If the underlying operation returns `NotReady` then the `unpark` value
    /// passed in will receive a notification when the operation is ready to be
    /// attempted again.
    pub fn poll_flush(&mut self, unpark: &Arc<Unpark>)
                       -> Poll<(), S::SinkError> {
        self.enter(unpark, |sink| sink.poll_complete())
    }

    /// Blocks the current thread until it's able to send `value` on this sink.
    ///
    /// This function will send the `value` on the sink that this task wraps. If
    /// the sink is not ready to send the value yet then the current thread will
    /// be blocked until it's able to send the value.
    pub fn wait_send(&mut self, mut value: S::SinkItem)
                     -> Result<(), S::SinkError> {
        let unpark = Arc::new(ThreadUnpark::new(thread::current()));
        let unpark2 = unpark.clone() as Arc<Unpark>;
        loop {
            value = match try!(self.start_send(value, &unpark2)) {
                AsyncSink::NotReady(v) => v,
                AsyncSink::Ready => return Ok(()),
            };
            unpark.park();
        }
    }

    /// Blocks the current thread until it's able to flush this sink.
    ///
    /// This function will call the underlying sink's `poll_complete` method
    /// until it returns that it's ready, proxying out errors upwards to the
    /// caller if one occurs.
    ///
    /// The thread will be blocked until `poll_complete` returns that it's
    /// ready.
    pub fn wait_flush(&mut self) -> Result<(), S::SinkError> {
        let unpark = Arc::new(ThreadUnpark::new(thread::current()));
        let unpark2 = unpark.clone() as Arc<Unpark>;
        loop {
            if try!(self.poll_flush(&unpark2)).is_ready() {
                return Ok(())
            }
            unpark.park();
        }
    }
}

impl<T> Spawn<T> {
    fn enter<F, R>(&mut self, unpark: &Arc<Unpark>, f: F) -> R
        where F: FnOnce(&mut T) -> R
    {
        let task = BorrowedTask {
            id: self.id,
            unpark: unpark,
            events: Events::new(),
            map: &self.data,
        };
        let obj = &mut self.obj;
        set(&task, || f(obj))
    }
}

impl<T: fmt::Debug> fmt::Debug for Spawn<T> {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("Spawn")
         .field("obj", &self.obj)
         .field("id", &self.id)
         .finish()
    }
}

/// A trait which represents a sink of notifications that a future is ready to
/// make progress.
///
/// This trait is provided as an argument to the `Spawn::poll_future` and
/// `Spawn::poll_stream` functions. It's transitively used as part of the
/// `Task::unpark` method to internally deliver notifications of readiness of a
/// future to move forward.
pub trait Unpark: Send + Sync {
    /// Indicates that an associated future and/or task are ready to make
    /// progress.
    ///
    /// Typically this means that the receiver of the notification should
    /// arrange for the future to get poll'd in a prompt fashion.
    fn unpark(&self);
}

/// A trait representing requests to poll futures.
///
/// This trait is an argument to the `Spawn::execute` which is used to run a
/// future to completion. An executor will receive requests to run a future and
/// an executor is responsible for ensuring that happens in a timely fashion.
pub trait Executor: Send + Sync + 'static {
    /// Requests that `Run` is executed soon on the given executor.
    fn execute(&self, r: Run);
}

struct ThreadUnpark {
    thread: thread::Thread,
    ready: AtomicBool,
}

impl ThreadUnpark {
    fn new(thread: thread::Thread) -> ThreadUnpark {
        ThreadUnpark {
            thread: thread,
            ready: AtomicBool::new(false),
        }
    }

    fn park(&self) {
        if !self.ready.swap(false, Ordering::SeqCst) {
            thread::park();
        }
    }
}

impl Unpark for ThreadUnpark {
    fn unpark(&self) {
        self.ready.store(true, Ordering::SeqCst);
        self.thread.unpark()
    }
}

/// Units of work submitted to an `Executor`, currently only created
/// internally.
pub struct Run {
    spawn: Spawn<BoxFuture<(), ()>>,
    inner: Arc<Inner>,
}

struct Inner {
    mutex: UnparkMutex<Run>,
    exec: Arc<Executor>,
}

impl Run {
    /// Actually run the task (invoking `poll` on its future) on the current
    /// thread.
    pub fn run(self) {
        let Run { mut spawn, inner } = self;

        // SAFETY: the ownership of this `Run` object is evidence that
        // we are in the `POLLING`/`REPOLL` state for the mutex.
        unsafe {
            inner.mutex.start_poll();

            loop {
                match spawn.poll_future(inner.clone()) {
                    Ok(Async::NotReady) => {}
                    Ok(Async::Ready(())) |
                    Err(()) => return inner.mutex.complete(),
                }
                let run = Run { spawn: spawn, inner: inner.clone() };
                match inner.mutex.wait(run) {
                    Ok(()) => return,            // we've waited
                    Err(r) => spawn = r.spawn,   // someone's notified us
                }
            }
        }
    }
}

impl fmt::Debug for Run {
    fn fmt(&self, f: &mut fmt::Formatter) -> fmt::Result {
        f.debug_struct("Run")
         .field("contents", &"...")
         .finish()
    }
}

impl Unpark for Inner {
    fn unpark(&self) {
        match self.mutex.notify() {
            Ok(run) => self.exec.execute(run),
            Err(()) => {}
        }
    }
}
