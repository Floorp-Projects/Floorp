//! Futures
//!
//! This module contains the `Future` trait and a number of adaptors for this
//! trait. See the crate docs, and the docs for `Future`, for full detail.

use core::result;

// Primitive futures
mod empty;
mod lazy;
mod poll_fn;
#[path = "result.rs"]
mod result_;
mod loop_fn;
mod option;
pub use self::empty::{empty, Empty};
pub use self::lazy::{lazy, Lazy};
pub use self::poll_fn::{poll_fn, PollFn};
pub use self::result_::{result, ok, err, FutureResult};
pub use self::loop_fn::{loop_fn, Loop, LoopFn};

#[doc(hidden)]
#[deprecated(since = "0.1.4", note = "use `ok` instead")]
#[cfg(feature = "with-deprecated")]
pub use self::{ok as finished, Ok as Finished};
#[doc(hidden)]
#[deprecated(since = "0.1.4", note = "use `err` instead")]
#[cfg(feature = "with-deprecated")]
pub use self::{err as failed, Err as Failed};
#[doc(hidden)]
#[deprecated(since = "0.1.4", note = "use `result` instead")]
#[cfg(feature = "with-deprecated")]
pub use self::{result as done, FutureResult as Done};
#[doc(hidden)]
#[deprecated(since = "0.1.7", note = "use `FutureResult` instead")]
#[cfg(feature = "with-deprecated")]
pub use self::{FutureResult as Ok};
#[doc(hidden)]
#[deprecated(since = "0.1.7", note = "use `FutureResult` instead")]
#[cfg(feature = "with-deprecated")]
pub use self::{FutureResult as Err};

// combinators
mod and_then;
mod flatten;
mod flatten_stream;
mod fuse;
mod into_stream;
mod join;
mod map;
mod map_err;
mod from_err;
mod or_else;
mod select;
mod select2;
mod then;
mod either;

// impl details
mod chain;

pub use self::and_then::AndThen;
pub use self::flatten::Flatten;
pub use self::flatten_stream::FlattenStream;
pub use self::fuse::Fuse;
pub use self::into_stream::IntoStream;
pub use self::join::{Join, Join3, Join4, Join5};
pub use self::map::Map;
pub use self::map_err::MapErr;
pub use self::from_err::FromErr;
pub use self::or_else::OrElse;
pub use self::select::{Select, SelectNext};
pub use self::select2::Select2;
pub use self::then::Then;
pub use self::either::Either;

if_std! {
    mod catch_unwind;
    mod join_all;
    mod select_all;
    mod select_ok;
    mod shared;
    pub use self::catch_unwind::CatchUnwind;
    pub use self::join_all::{join_all, JoinAll};
    pub use self::select_all::{SelectAll, SelectAllNext, select_all};
    pub use self::select_ok::{SelectOk, select_ok};
    pub use self::shared::{Shared, SharedItem, SharedError};

    #[doc(hidden)]
    #[deprecated(since = "0.1.4", note = "use join_all instead")]
    #[cfg(feature = "with-deprecated")]
    pub use self::join_all::join_all as collect;
    #[doc(hidden)]
    #[deprecated(since = "0.1.4", note = "use JoinAll instead")]
    #[cfg(feature = "with-deprecated")]
    pub use self::join_all::JoinAll as Collect;

    /// A type alias for `Box<Future + Send>`
    pub type BoxFuture<T, E> = ::std::boxed::Box<Future<Item = T, Error = E> + Send>;

    impl<F: ?Sized + Future> Future for ::std::boxed::Box<F> {
        type Item = F::Item;
        type Error = F::Error;

        fn poll(&mut self) -> Poll<Self::Item, Self::Error> {
            (**self).poll()
        }
    }
}

use {Poll, stream};

/// Trait for types which are a placeholder of a value that may become
/// available at some later point in time.
///
/// In addition to the documentation here you can also find more information
/// about futures [online] at [https://tokio.rs](https://tokio.rs)
///
/// [online]: https://tokio.rs/docs/getting-started/futures/
///
/// Futures are used to provide a sentinel through which a value can be
/// referenced. They crucially allow chaining and composing operations through
/// consumption which allows expressing entire trees of computation as one
/// sentinel value.
///
/// The ergonomics and implementation of the `Future` trait are very similar to
/// the `Iterator` trait in that there is just one methods you need
/// to implement, but you get a whole lot of others for free as a result.
///
/// # The `poll` method
///
/// The core method of future, `poll`, is used to attempt to generate the value
/// of a `Future`. This method *does not block* but is allowed to inform the
/// caller that the value is not ready yet. Implementations of `poll` may
/// themselves do work to generate the value, but it's guaranteed that this will
/// never block the calling thread.
///
/// A key aspect of this method is that if the value is not yet available the
/// current task is scheduled to receive a notification when it's later ready to
/// be made available. This follows what's typically known as a "readiness" or
/// "pull" model where values are pulled out of futures on demand, and
/// otherwise a task is notified when a value might be ready to get pulled out.
///
/// The `poll` method is not intended to be called in general, but rather is
/// typically called in the context of a "task" which drives a future to
/// completion. For more information on this see the `task` module.
///
/// More information about the details of `poll` and the nitty-gritty of tasks
/// can be [found online at tokio.rs][poll-dox].
///
/// [poll-dox]: https://tokio.rs/docs/going-deeper/futures-model/
///
/// # Combinators
///
/// Like iterators, futures provide a large number of combinators to work with
/// futures to express computations in a much more natural method than
/// scheduling a number of callbacks. For example the `map` method can change
/// a `Future<Item=T>` to a `Future<Item=U>` or an `and_then` combinator could
/// create a future after the first one is done and only be resolved when the
/// second is done.
///
/// Combinators act very similarly to the methods on the `Iterator` trait itself
/// or those on `Option` and `Result`. Like with iterators, the combinators are
/// zero-cost and don't impose any extra layers of indirection you wouldn't
/// otherwise have to write down.
///
/// More information about combinators can be found [on tokio.rs].
///
/// [on tokio.rs]: https://tokio.rs/docs/going-deeper/futures-mechanics/
pub trait Future {
    /// The type of value that this future will resolved with if it is
    /// successful.
    type Item;

    /// The type of error that this future will resolve with if it fails in a
    /// normal fashion.
    type Error;

    /// Query this future to see if its value has become available, registering
    /// interest if it is not.
    ///
    /// This function will check the internal state of the future and assess
    /// whether the value is ready to be produced. Implementors of this function
    /// should ensure that a call to this **never blocks** as event loops may
    /// not work properly otherwise.
    ///
    /// When a future is not ready yet, the `Async::NotReady` value will be
    /// returned. In this situation the future will *also* register interest of
    /// the current task in the value being produced. This is done by calling
    /// `task::park` to retrieve a handle to the current `Task`. When the future
    /// is then ready to make progress (e.g. it should be `poll`ed again) the
    /// `unpark` method is called on the `Task`.
    ///
    /// More information about the details of `poll` and the nitty-gritty of
    /// tasks can be [found online at tokio.rs][poll-dox].
    ///
    /// [poll-dox]: https://tokio.rs/docs/going-deeper/futures-model/
    ///
    /// # Runtime characteristics
    ///
    /// This function, `poll`, is the primary method for 'making progress'
    /// within a tree of futures. For example this method will be called
    /// repeatedly as the internal state machine makes its various transitions.
    /// Executors are responsible for ensuring that this function is called in
    /// the right location (e.g. always on an I/O thread or not). Unless it is
    /// otherwise arranged to be so, it should be ensured that **implementations
    /// of this function finish very quickly**.
    ///
    /// Returning quickly prevents unnecessarily clogging up threads and/or
    /// event loops while a `poll` function call, for example, takes up compute
    /// resources to perform some expensive computation. If it is known ahead
    /// of time that a call to `poll` may end up taking awhile, the work should
    /// be offloaded to a thread pool (or something similar) to ensure that
    /// `poll` can return quickly.
    ///
    /// Note that the `poll` function is not called repeatedly in a loop for
    /// futures typically, but only whenever the future itself is ready. If
    /// you're familiar with the `poll(2)` or `select(2)` syscalls on Unix
    /// it's worth noting that futures typically do *not* suffer the same
    /// problems of "all wakeups must poll all events". Futures have enough
    /// support for only polling futures which cause a wakeup.
    ///
    /// # Return value
    ///
    /// This function returns `Async::NotReady` if the future is not ready yet,
    /// `Err` if the future is finished but resolved to an error, or
    /// `Async::Ready` with the result of this future if it's finished
    /// successfully. Once a future has finished it is considered a contract
    /// error to continue polling the future.
    ///
    /// If `NotReady` is returned, then the future will internally register
    /// interest in the value being produced for the current task (through
    /// `task::park`). In other words, the current task will receive a
    /// notification (through the `unpark` method) once the value is ready to be
    /// produced or the future can make progress.
    ///
    /// # Panics
    ///
    /// Once a future has completed (returned `Ready` or `Err` from `poll`),
    /// then any future calls to `poll` may panic, block forever, or otherwise
    /// cause wrong behavior. The `Future` trait itself provides no guarantees
    /// about the behavior of `poll` after a future has completed.
    ///
    /// Callers who may call `poll` too many times may want to consider using
    /// the `fuse` adaptor which defines the behavior of `poll`, but comes with
    /// a little bit of extra cost.
    ///
    /// Additionally, calls to `poll` must always be made from within the
    /// context of a task. If a current task is not set then this method will
    /// likely panic.
    ///
    /// # Errors
    ///
    /// This future may have failed to finish the computation, in which case
    /// the `Err` variant will be returned with an appropriate payload of an
    /// error.
    fn poll(&mut self) -> Poll<Self::Item, Self::Error>;

    /// Block the current thread until this future is resolved.
    ///
    /// This method will consume ownership of this future, driving it to
    /// completion via `poll` and blocking the current thread while it's waiting
    /// for the value to become available. Once the future is resolved the
    /// result of this future is returned.
    ///
    /// > **Note:** This method is not appropriate to call on event loops or
    /// >           similar I/O situations because it will prevent the event
    /// >           loop from making progress (this blocks the thread). This
    /// >           method should only be called when it's guaranteed that the
    /// >           blocking work associated with this future will be completed
    /// >           by another thread.
    ///
    /// This method is only available when the `use_std` feature of this
    /// library is activated, and it is activated by default.
    ///
    /// # Panics
    ///
    /// This function does not attempt to catch panics. If the `poll` function
    /// of this future panics, panics will be propagated to the caller.
    #[cfg(feature = "use_std")]
    fn wait(self) -> result::Result<Self::Item, Self::Error>
        where Self: Sized
    {
        ::executor::spawn(self).wait_future()
    }

    /// Convenience function for turning this future into a trait object which
    /// is also `Send`.
    ///
    /// This simply avoids the need to write `Box::new` and can often help with
    /// type inference as well by always returning a trait object. Note that
    /// this method requires the `Send` bound and returns a `BoxFuture`, which
    /// also encodes this. If you'd like to create a `Box<Future>` without the
    /// `Send` bound, then the `Box::new` function can be used instead.
    ///
    /// This method is only available when the `use_std` feature of this
    /// library is activated, and it is activated by default.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::*;
    ///
    /// let a: BoxFuture<i32, i32> = result(Ok(1)).boxed();
    /// ```
    #[cfg(feature = "use_std")]
    fn boxed(self) -> BoxFuture<Self::Item, Self::Error>
        where Self: Sized + Send + 'static
    {
        ::std::boxed::Box::new(self)
    }

    /// Map this future's result to a different type, returning a new future of
    /// the resulting type.
    ///
    /// This function is similar to the `Option::map` or `Iterator::map` where
    /// it will change the type of the underlying future. This is useful to
    /// chain along a computation once a future has been resolved.
    ///
    /// The closure provided will only be called if this future is resolved
    /// successfully. If this future returns an error, panics, or is dropped,
    /// then the closure provided will never be invoked.
    ///
    /// Note that this function consumes the receiving future and returns a
    /// wrapped version of it, similar to the existing `map` methods in the
    /// standard library.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::*;
    ///
    /// let future_of_1 = ok::<u32, u32>(1);
    /// let future_of_4 = future_of_1.map(|x| x + 3);
    /// ```
    fn map<F, U>(self, f: F) -> Map<Self, F>
        where F: FnOnce(Self::Item) -> U,
              Self: Sized,
    {
        assert_future::<U, Self::Error, _>(map::new(self, f))
    }

    /// Map this future's error to a different error, returning a new future.
    ///
    /// This function is similar to the `Result::map_err` where it will change
    /// the error type of the underlying future. This is useful for example to
    /// ensure that futures have the same error type when used with combinators
    /// like `select` and `join`.
    ///
    /// The closure provided will only be called if this future is resolved
    /// with an error. If this future returns a success, panics, or is
    /// dropped, then the closure provided will never be invoked.
    ///
    /// Note that this function consumes the receiving future and returns a
    /// wrapped version of it.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::*;
    ///
    /// let future_of_err_1 = err::<u32, u32>(1);
    /// let future_of_err_4 = future_of_err_1.map_err(|x| x + 3);
    /// ```
    fn map_err<F, E>(self, f: F) -> MapErr<Self, F>
        where F: FnOnce(Self::Error) -> E,
              Self: Sized,
    {
        assert_future::<Self::Item, E, _>(map_err::new(self, f))
    }



    /// Map this future's error to any error implementing `From` for
    /// this future's `Error`, returning a new future.
    ///
    /// This function does for futures what `try!` does for `Result`,
    /// by letting the compiler infer the type of the resulting error.
    /// Just as `map_err` above, this is useful for example to ensure
    /// that futures have the same error type when used with
    /// combinators like `select` and `join`.
    ///
    /// Note that this function consumes the receiving future and returns a
    /// wrapped version of it.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::*;
    ///
    /// let future_of_err_1 = err::<u32, u32>(1);
    /// let future_of_err_4 = future_of_err_1.from_err::<u32>();
    /// ```
    fn from_err<E:From<Self::Error>>(self) -> FromErr<Self, E>
        where Self: Sized,
    {
        assert_future::<Self::Item, E, _>(from_err::new(self))
    }

    /// Chain on a computation for when a future finished, passing the result of
    /// the future to the provided closure `f`.
    ///
    /// This function can be used to ensure a computation runs regardless of
    /// the conclusion of the future. The closure provided will be yielded a
    /// `Result` once the future is complete.
    ///
    /// The returned value of the closure must implement the `IntoFuture` trait
    /// and can represent some more work to be done before the composed future
    /// is finished. Note that the `Result` type implements the `IntoFuture`
    /// trait so it is possible to simply alter the `Result` yielded to the
    /// closure and return it.
    ///
    /// If this future is dropped or panics then the closure `f` will not be
    /// run.
    ///
    /// Note that this function consumes the receiving future and returns a
    /// wrapped version of it.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::*;
    ///
    /// let future_of_1 = ok::<u32, u32>(1);
    /// let future_of_4 = future_of_1.then(|x| {
    ///     x.map(|y| y + 3)
    /// });
    ///
    /// let future_of_err_1 = err::<u32, u32>(1);
    /// let future_of_4 = future_of_err_1.then(|x| {
    ///     match x {
    ///         Ok(_) => panic!("expected an error"),
    ///         Err(y) => ok::<u32, u32>(y + 3),
    ///     }
    /// });
    /// ```
    fn then<F, B>(self, f: F) -> Then<Self, B, F>
        where F: FnOnce(result::Result<Self::Item, Self::Error>) -> B,
              B: IntoFuture,
              Self: Sized,
    {
        assert_future::<B::Item, B::Error, _>(then::new(self, f))
    }

    /// Execute another future after this one has resolved successfully.
    ///
    /// This function can be used to chain two futures together and ensure that
    /// the final future isn't resolved until both have finished. The closure
    /// provided is yielded the successful result of this future and returns
    /// another value which can be converted into a future.
    ///
    /// Note that because `Result` implements the `IntoFuture` trait this method
    /// can also be useful for chaining fallible and serial computations onto
    /// the end of one future.
    ///
    /// If this future is dropped, panics, or completes with an error then the
    /// provided closure `f` is never called.
    ///
    /// Note that this function consumes the receiving future and returns a
    /// wrapped version of it.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::*;
    ///
    /// let future_of_1 = ok::<u32, u32>(1);
    /// let future_of_4 = future_of_1.and_then(|x| {
    ///     Ok(x + 3)
    /// });
    ///
    /// let future_of_err_1 = err::<u32, u32>(1);
    /// future_of_err_1.and_then(|_| -> FutureResult<u32, u32> {
    ///     panic!("should not be called in case of an error");
    /// });
    /// ```
    fn and_then<F, B>(self, f: F) -> AndThen<Self, B, F>
        where F: FnOnce(Self::Item) -> B,
              B: IntoFuture<Error = Self::Error>,
              Self: Sized,
    {
        assert_future::<B::Item, Self::Error, _>(and_then::new(self, f))
    }

    /// Execute another future if this one resolves with an error.
    ///
    /// Return a future that passes along this future's value if it succeeds,
    /// and otherwise passes the error to the closure `f` and waits for the
    /// future it returns. The closure may also simply return a value that can
    /// be converted into a future.
    ///
    /// Note that because `Result` implements the `IntoFuture` trait this method
    /// can also be useful for chaining together fallback computations, where
    /// when one fails, the next is attempted.
    ///
    /// If this future is dropped, panics, or completes successfully then the
    /// provided closure `f` is never called.
    ///
    /// Note that this function consumes the receiving future and returns a
    /// wrapped version of it.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::*;
    ///
    /// let future_of_err_1 = err::<u32, u32>(1);
    /// let future_of_4 = future_of_err_1.or_else(|x| -> Result<u32, u32> {
    ///     Ok(x + 3)
    /// });
    ///
    /// let future_of_1 = ok::<u32, u32>(1);
    /// future_of_1.or_else(|_| -> FutureResult<u32, u32> {
    ///     panic!("should not be called in case of success");
    /// });
    /// ```
    fn or_else<F, B>(self, f: F) -> OrElse<Self, B, F>
        where F: FnOnce(Self::Error) -> B,
              B: IntoFuture<Item = Self::Item>,
              Self: Sized,
    {
        assert_future::<Self::Item, B::Error, _>(or_else::new(self, f))
    }

    /// Waits for either one of two futures to complete.
    ///
    /// This function will return a new future which awaits for either this or
    /// the `other` future to complete. The returned future will finish with
    /// both the value resolved and a future representing the completion of the
    /// other work. Both futures must have the same item and error type.
    ///
    /// Note that this function consumes the receiving futures and returns a
    /// wrapped version of them.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::*;
    ///
    /// // A poor-man's join implemented on top of select
    ///
    /// fn join<A>(a: A, b: A) -> BoxFuture<(u32, u32), u32>
    ///     where A: Future<Item = u32, Error = u32> + Send + 'static,
    /// {
    ///     a.select(b).then(|res| {
    ///         match res {
    ///             Ok((a, b)) => b.map(move |b| (a, b)).boxed(),
    ///             Err((a, _)) => err(a).boxed(),
    ///         }
    ///     }).boxed()
    /// }
    /// ```
    fn select<B>(self, other: B) -> Select<Self, B::Future>
        where B: IntoFuture<Item=Self::Item, Error=Self::Error>,
              Self: Sized,
    {
        let f = select::new(self, other.into_future());
        assert_future::<(Self::Item, SelectNext<Self, B::Future>),
                        (Self::Error, SelectNext<Self, B::Future>), _>(f)
    }

    /// Waits for either one of two differently-typed futures to complete.
    ///
    /// This function will return a new future which awaits for either this or
    /// the `other` future to complete. The returned future will finish with
    /// both the value resolved and a future representing the completion of the
    /// other work.
    ///
    /// Note that this function consumes the receiving futures and returns a
    /// wrapped version of them.
    ///
    /// Also note that if both this and the second future have the same
    /// success/error type you can use the `Either::split` method to
    /// conveniently extract out the value at the end.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::*;
    ///
    /// // A poor-man's join implemented on top of select2
    ///
    /// fn join<A, B, E>(a: A, b: B) -> BoxFuture<(A::Item, B::Item), E>
    ///     where A: Future<Error = E> + Send + 'static,
    ///           B: Future<Error = E> + Send + 'static,
    ///           A::Item: Send, B::Item: Send, E: Send + 'static,
    /// {
    ///     a.select2(b).then(|res| {
    ///         match res {
    ///             Ok(Either::A((x, b))) => b.map(move |y| (x, y)).boxed(),
    ///             Ok(Either::B((y, a))) => a.map(move |x| (x, y)).boxed(),
    ///             Err(Either::A((e, _))) => err(e).boxed(),
    ///             Err(Either::B((e, _))) => err(e).boxed(),
    ///         }
    ///     }).boxed()
    /// }
    /// ```
    fn select2<B>(self, other: B) -> Select2<Self, B::Future>
        where B: IntoFuture, Self: Sized
    {
        select2::new(self, other.into_future())
    }

    /// Joins the result of two futures, waiting for them both to complete.
    ///
    /// This function will return a new future which awaits both this and the
    /// `other` future to complete. The returned future will finish with a tuple
    /// of both results.
    ///
    /// Both futures must have the same error type, and if either finishes with
    /// an error then the other will be dropped and that error will be
    /// returned.
    ///
    /// Note that this function consumes the receiving future and returns a
    /// wrapped version of it.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::*;
    ///
    /// let a = ok::<u32, u32>(1);
    /// let b = ok::<u32, u32>(2);
    /// let pair = a.join(b);
    ///
    /// pair.map(|(a, b)| {
    ///     assert_eq!(a, 1);
    ///     assert_eq!(b, 2);
    /// });
    /// ```
    fn join<B>(self, other: B) -> Join<Self, B::Future>
        where B: IntoFuture<Error=Self::Error>,
              Self: Sized,
    {
        let f = join::new(self, other.into_future());
        assert_future::<(Self::Item, B::Item), Self::Error, _>(f)
    }

    /// Same as `join`, but with more futures.
    fn join3<B, C>(self, b: B, c: C) -> Join3<Self, B::Future, C::Future>
        where B: IntoFuture<Error=Self::Error>,
              C: IntoFuture<Error=Self::Error>,
              Self: Sized,
    {
        join::new3(self, b.into_future(), c.into_future())
    }

    /// Same as `join`, but with more futures.
    fn join4<B, C, D>(self, b: B, c: C, d: D)
                      -> Join4<Self, B::Future, C::Future, D::Future>
        where B: IntoFuture<Error=Self::Error>,
              C: IntoFuture<Error=Self::Error>,
              D: IntoFuture<Error=Self::Error>,
              Self: Sized,
    {
        join::new4(self, b.into_future(), c.into_future(), d.into_future())
    }

    /// Same as `join`, but with more futures.
    fn join5<B, C, D, E>(self, b: B, c: C, d: D, e: E)
                         -> Join5<Self, B::Future, C::Future, D::Future, E::Future>
        where B: IntoFuture<Error=Self::Error>,
              C: IntoFuture<Error=Self::Error>,
              D: IntoFuture<Error=Self::Error>,
              E: IntoFuture<Error=Self::Error>,
              Self: Sized,
    {
        join::new5(self, b.into_future(), c.into_future(), d.into_future(),
                   e.into_future())
    }

    /// Convert this future into a single element stream.
    ///
    /// The returned stream contains single success if this future resolves to
    /// success or single error if this future resolves into error.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::{Stream, Async};
    /// use futures::future::*;
    ///
    /// let future = ok::<_, bool>(17);
    /// let mut stream = future.into_stream();
    /// assert_eq!(Ok(Async::Ready(Some(17))), stream.poll());
    /// assert_eq!(Ok(Async::Ready(None)), stream.poll());
    ///
    /// let future = err::<bool, _>(19);
    /// let mut stream = future.into_stream();
    /// assert_eq!(Err(19), stream.poll());
    /// assert_eq!(Ok(Async::Ready(None)), stream.poll());
    /// ```
    fn into_stream(self) -> IntoStream<Self>
        where Self: Sized
    {
        into_stream::new(self)
    }

    /// Flatten the execution of this future when the successful result of this
    /// future is itself another future.
    ///
    /// This can be useful when combining futures together to flatten the
    /// computation out the the final result. This method can only be called
    /// when the successful result of this future itself implements the
    /// `IntoFuture` trait and the error can be created from this future's error
    /// type.
    ///
    /// This method is roughly equivalent to `self.and_then(|x| x)`.
    ///
    /// Note that this function consumes the receiving future and returns a
    /// wrapped version of it.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::*;
    ///
    /// let future_of_a_future = ok::<_, u32>(ok::<u32, u32>(1));
    /// let future_of_1 = future_of_a_future.flatten();
    /// ```
    fn flatten(self) -> Flatten<Self>
        where Self::Item: IntoFuture,
        <<Self as Future>::Item as IntoFuture>::Error:
            From<<Self as Future>::Error>,
        Self: Sized
    {
        let f = flatten::new(self);
        assert_future::<<<Self as Future>::Item as IntoFuture>::Item,
                        <<Self as Future>::Item as IntoFuture>::Error,
                        _>(f)
    }

    /// Flatten the execution of this future when the successful result of this
    /// future is a stream.
    ///
    /// This can be useful when stream initialization is deferred, and it is
    /// convenient to work with that stream as if stream was available at the
    /// call site.
    ///
    /// Note that this function consumes this future and returns a wrapped
    /// version of it.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::stream::{self, Stream};
    /// use futures::future::*;
    ///
    /// let stream_items = vec![Ok(17), Err(true), Ok(19)];
    /// let future_of_a_stream = ok::<_, bool>(stream::iter(stream_items));
    ///
    /// let stream = future_of_a_stream.flatten_stream();
    ///
    /// let mut iter = stream.wait();
    /// assert_eq!(Ok(17), iter.next().unwrap());
    /// assert_eq!(Err(true), iter.next().unwrap());
    /// assert_eq!(Ok(19), iter.next().unwrap());
    /// assert_eq!(None, iter.next());
    /// ```
    fn flatten_stream(self) -> FlattenStream<Self>
        where <Self as Future>::Item: stream::Stream<Error=Self::Error>,
              Self: Sized
    {
        flatten_stream::new(self)
    }

    /// Fuse a future such that `poll` will never again be called once it has
    /// completed.
    ///
    /// Currently once a future has returned `Ready` or `Err` from
    /// `poll` any further calls could exhibit bad behavior such as blocking
    /// forever, panicking, never returning, etc. If it is known that `poll`
    /// may be called too often then this method can be used to ensure that it
    /// has defined semantics.
    ///
    /// Once a future has been `fuse`d and it returns a completion from `poll`,
    /// then it will forever return `NotReady` from `poll` again (never
    /// resolve).  This, unlike the trait's `poll` method, is guaranteed.
    ///
    /// This combinator will drop this future as soon as it's been completed to
    /// ensure resources are reclaimed as soon as possible.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use futures::Async;
    /// use futures::future::*;
    ///
    /// let mut future = ok::<i32, u32>(2);
    /// assert_eq!(future.poll(), Ok(Async::Ready(2)));
    ///
    /// // Normally, a call such as this would panic:
    /// //future.poll();
    ///
    /// // This, however, is guaranteed to not panic
    /// let mut future = ok::<i32, u32>(2).fuse();
    /// assert_eq!(future.poll(), Ok(Async::Ready(2)));
    /// assert_eq!(future.poll(), Ok(Async::NotReady));
    /// ```
    fn fuse(self) -> Fuse<Self>
        where Self: Sized
    {
        let f = fuse::new(self);
        assert_future::<Self::Item, Self::Error, _>(f)
    }

    /// Catches unwinding panics while polling the future.
    ///
    /// In general, panics within a future can propagate all the way out to the
    /// task level. This combinator makes it possible to halt unwinding within
    /// the future itself. It's most commonly used within task executors. It's
    /// not recommended to use this for error handling.
    ///
    /// Note that this method requires the `UnwindSafe` bound from the standard
    /// library. This isn't always applied automatically, and the standard
    /// library provides an `AssertUnwindSafe` wrapper type to apply it
    /// after-the fact. To assist using this method, the `Future` trait is also
    /// implemented for `AssertUnwindSafe<F>` where `F` implements `Future`.
    ///
    /// This method is only available when the `use_std` feature of this
    /// library is activated, and it is activated by default.
    ///
    /// # Examples
    ///
    /// ```rust
    /// use futures::future::*;
    ///
    /// let mut future = ok::<i32, u32>(2);
    /// assert!(future.catch_unwind().wait().is_ok());
    ///
    /// let mut future = lazy(|| -> FutureResult<i32, u32> {
    ///     panic!();
    ///     ok::<i32, u32>(2)
    /// });
    /// assert!(future.catch_unwind().wait().is_err());
    /// ```
    #[cfg(feature = "use_std")]
    fn catch_unwind(self) -> CatchUnwind<Self>
        where Self: Sized + ::std::panic::UnwindSafe
    {
        catch_unwind::new(self)
    }

    /// Create a cloneable handle to this future where all handles will resolve
    /// to the same result.
    ///
    /// The shared() method provides a mean to convert any future into a
    /// cloneable future. It enables a future to be polled by multiple threads.
    ///
    /// The returned `Shared` future resolves successfully with
    /// `SharedItem<Self::Item>` or erroneously with `SharedError<Self::Error>`.
    /// Both `SharedItem` and `SharedError` implements `Deref` to allow shared
    /// access to the underlying result. Ownership of `Self::Item` and
    /// `Self::Error` cannot currently be reclaimed.
    ///
    /// This method is only available when the `use_std` feature of this
    /// library is activated, and it is activated by default.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::*;
    ///
    /// let future = ok::<_, bool>(6);
    /// let shared1 = future.shared();
    /// let shared2 = shared1.clone();
    /// assert_eq!(6, *shared1.wait().unwrap());
    /// assert_eq!(6, *shared2.wait().unwrap());
    /// ```
    ///
    /// ```
    /// use std::thread;
    /// use futures::future::*;
    ///
    /// let future = ok::<_, bool>(6);
    /// let shared1 = future.shared();
    /// let shared2 = shared1.clone();
    /// let join_handle = thread::spawn(move || {
    ///     assert_eq!(6, *shared2.wait().unwrap());
    /// });
    /// assert_eq!(6, *shared1.wait().unwrap());
    /// join_handle.join().unwrap();
    /// ```
    #[cfg(feature = "use_std")]
    fn shared(self) -> Shared<Self>
        where Self: Sized
    {
        shared::new(self)
    }
}

impl<'a, F: ?Sized + Future> Future for &'a mut F {
    type Item = F::Item;
    type Error = F::Error;

    fn poll(&mut self) -> Poll<Self::Item, Self::Error> {
        (**self).poll()
    }
}

// Just a helper function to ensure the futures we're returning all have the
// right implementations.
fn assert_future<A, B, F>(t: F) -> F
    where F: Future<Item=A, Error=B>,
{
    t
}

/// Class of types which can be converted into a future.
///
/// This trait is very similar to the `IntoIterator` trait and is intended to be
/// used in a very similar fashion.
pub trait IntoFuture {
    /// The future that this type can be converted into.
    type Future: Future<Item=Self::Item, Error=Self::Error>;

    /// The item that the future may resolve with.
    type Item;
    /// The error that the future may resolve with.
    type Error;

    /// Consumes this object and produces a future.
    fn into_future(self) -> Self::Future;
}

impl<F: Future> IntoFuture for F {
    type Future = F;
    type Item = F::Item;
    type Error = F::Error;

    fn into_future(self) -> F {
        self
    }
}

impl<T, E> IntoFuture for result::Result<T, E> {
    type Future = FutureResult<T, E>;
    type Item = T;
    type Error = E;

    fn into_future(self) -> FutureResult<T, E> {
        result(self)
    }
}

/// Asynchronous conversion from a type `T`.
///
/// This trait is analogous to `std::convert::From`, adapted to asynchronous
/// computation.
pub trait FutureFrom<T>: Sized {
    /// The future for the conversion.
    type Future: Future<Item=Self, Error=Self::Error>;

    /// Possible errors during conversion.
    type Error;

    /// Consume the given value, beginning the conversion.
    fn future_from(T) -> Self::Future;
}
