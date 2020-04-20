//! Futures
//!
//! This module contains a number of functions for working with `Future`s,
//! including the `FutureExt` trait which adds methods to `Future` types.

#[cfg(feature = "compat")]
use crate::compat::Compat;
use core::pin::Pin;
use futures_core::{
    future::TryFuture,
    stream::TryStream,
    task::{Context, Poll},
};
#[cfg(feature = "sink")]
use futures_sink::Sink;

// Combinators

mod and_then;
#[allow(unreachable_pub)] // https://github.com/rust-lang/rust/issues/57411
pub use self::and_then::AndThen;

mod err_into;
#[allow(unreachable_pub)] // https://github.com/rust-lang/rust/issues/57411
pub use self::err_into::ErrInto;

#[cfg(feature = "sink")]
mod flatten_sink;
#[cfg(feature = "sink")]
#[allow(unreachable_pub)] // https://github.com/rust-lang/rust/issues/57411
pub use self::flatten_sink::FlattenSink;

mod inspect_ok;
#[allow(unreachable_pub)] // https://github.com/rust-lang/rust/issues/57411
pub use self::inspect_ok::InspectOk;

mod inspect_err;
#[allow(unreachable_pub)] // https://github.com/rust-lang/rust/issues/57411
pub use self::inspect_err::InspectErr;

mod into_future;
#[allow(unreachable_pub)] // https://github.com/rust-lang/rust/issues/57411
pub use self::into_future::IntoFuture;

mod map_err;
#[allow(unreachable_pub)] // https://github.com/rust-lang/rust/issues/57411
pub use self::map_err::MapErr;

mod map_ok;
#[allow(unreachable_pub)] // https://github.com/rust-lang/rust/issues/57411
pub use self::map_ok::MapOk;

mod map_ok_or_else;
#[allow(unreachable_pub)] // https://github.com/rust-lang/rust/issues/57411
pub use self::map_ok_or_else::MapOkOrElse;

mod or_else;
#[allow(unreachable_pub)] // https://github.com/rust-lang/rust/issues/57411
pub use self::or_else::OrElse;

mod try_flatten_stream;
#[allow(unreachable_pub)] // https://github.com/rust-lang/rust/issues/57411
pub use self::try_flatten_stream::TryFlattenStream;

mod unwrap_or_else;
#[allow(unreachable_pub)] // https://github.com/rust-lang/rust/issues/57411
pub use self::unwrap_or_else::UnwrapOrElse;

// Implementation details

mod flatten_stream_sink;
pub(crate) use self::flatten_stream_sink::FlattenStreamSink;

mod try_chain;
pub(crate) use self::try_chain::{TryChain, TryChainAction};

impl<Fut: ?Sized + TryFuture> TryFutureExt for Fut {}

/// Adapters specific to [`Result`]-returning futures
pub trait TryFutureExt: TryFuture {
    /// Flattens the execution of this future when the successful result of this
    /// future is a [`Sink`].
    ///
    /// This can be useful when sink initialization is deferred, and it is
    /// convenient to work with that sink as if the sink was available at the
    /// call site.
    ///
    /// Note that this function consumes this future and returns a wrapped
    /// version of it.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::{Future, TryFutureExt};
    /// use futures::sink::Sink;
    /// # use futures::channel::mpsc::{self, SendError};
    /// # type T = i32;
    /// # type E = SendError;
    ///
    /// fn make_sink_async() -> impl Future<Output = Result<
    ///     impl Sink<T, Error = E>,
    ///     E,
    /// >> { // ... }
    /// # let (tx, _rx) = mpsc::unbounded::<i32>();
    /// # futures::future::ready(Ok(tx))
    /// # }
    /// fn take_sink(sink: impl Sink<T, Error = E>) { /* ... */ }
    ///
    /// let fut = make_sink_async();
    /// take_sink(fut.flatten_sink())
    /// ```
    #[cfg(feature = "sink")]
    fn flatten_sink<Item>(self) -> FlattenSink<Self, Self::Ok>
    where
        Self::Ok: Sink<Item, Error = Self::Error>,
        Self: Sized,
    {
        FlattenSink::new(self)
    }

    /// Maps this future's success value to a different value.
    ///
    /// This method can be used to change the [`Ok`](TryFuture::Ok) type of the
    /// future into a different type. It is similar to the [`Result::map`]
    /// method. You can use this method to chain along a computation once the
    /// future has been resolved.
    ///
    /// The provided closure `f` will only be called if this future is resolved
    /// to an [`Ok`]. If it resolves to an [`Err`], panics, or is dropped, then
    /// the provided closure will never be invoked.
    ///
    /// Note that this method consumes the future it is called on and returns a
    /// wrapped version of it.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::TryFutureExt;
    ///
    /// # futures::executor::block_on(async {
    /// let future = async { Ok::<i32, i32>(1) };
    /// let future = future.map_ok(|x| x + 3);
    /// assert_eq!(future.await, Ok(4));
    /// # });
    /// ```
    ///
    /// Calling [`map_ok`](TryFutureExt::map_ok) on an errored future has no
    /// effect:
    ///
    /// ```
    /// use futures::future::TryFutureExt;
    ///
    /// # futures::executor::block_on(async {
    /// let future = async { Err::<i32, i32>(1) };
    /// let future = future.map_ok(|x| x + 3);
    /// assert_eq!(future.await, Err(1));
    /// # });
    /// ```
    fn map_ok<T, F>(self, f: F) -> MapOk<Self, F>
    where
        F: FnOnce(Self::Ok) -> T,
        Self: Sized,
    {
        MapOk::new(self, f)
    }

    /// Maps this future's success value to a different value, and permits for error handling resulting in the same type.
    ///
    /// This method can be used to coalesce your [`Ok`](TryFuture::Ok) type and [`Error`](TryFuture::Error) into another type,
    /// where that type is the same for both outcomes.
    ///
    /// The provided closure `f` will only be called if this future is resolved
    /// to an [`Ok`]. If it resolves to an [`Err`], panics, or is dropped, then
    /// the provided closure will never be invoked.
    /// 
    /// The provided closure `e` will only be called if this future is resolved
    /// to an [`Err`]. If it resolves to an [`Ok`], panics, or is dropped, then
    /// the provided closure will never be invoked.
    ///
    /// Note that this method consumes the future it is called on and returns a
    /// wrapped version of it.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::TryFutureExt;
    ///
    /// # futures::executor::block_on(async {
    /// let future = async { Ok::<i32, i32>(5) };
    /// let future = future.map_ok_or_else(|x| x * 2, |x| x + 3);
    /// assert_eq!(future.await, 8);
    /// 
    /// let future = async { Err::<i32, i32>(5) };
    /// let future = future.map_ok_or_else(|x| x * 2, |x| x + 3);
    /// assert_eq!(future.await, 10);
    /// # });
    /// ```
    /// 
    fn map_ok_or_else<T, E, F>(self, e: E, f: F) -> MapOkOrElse<Self, F, E>
    where
        F: FnOnce(Self::Ok) -> T,
        E: FnOnce(Self::Error) -> T,
        Self: Sized,
    {
        MapOkOrElse::new(self, e, f)
    }

    /// Maps this future's error value to a different value.
    ///
    /// This method can be used to change the [`Error`](TryFuture::Error) type
    /// of the future into a different type. It is similar to the
    /// [`Result::map_err`] method. You can use this method for example to
    /// ensure that futures have the same [`Error`](TryFuture::Error) type when
    /// using [`select!`] or [`join!`].
    ///
    /// The provided closure `f` will only be called if this future is resolved
    /// to an [`Err`]. If it resolves to an [`Ok`], panics, or is dropped, then
    /// the provided closure will never be invoked.
    ///
    /// Note that this method consumes the future it is called on and returns a
    /// wrapped version of it.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::TryFutureExt;
    ///
    /// # futures::executor::block_on(async {
    /// let future = async { Err::<i32, i32>(1) };
    /// let future = future.map_err(|x| x + 3);
    /// assert_eq!(future.await, Err(4));
    /// # });
    /// ```
    ///
    /// Calling [`map_err`](TryFutureExt::map_err) on a successful future has
    /// no effect:
    ///
    /// ```
    /// use futures::future::TryFutureExt;
    ///
    /// # futures::executor::block_on(async {
    /// let future = async { Ok::<i32, i32>(1) };
    /// let future = future.map_err(|x| x + 3);
    /// assert_eq!(future.await, Ok(1));
    /// # });
    /// ```
    fn map_err<E, F>(self, f: F) -> MapErr<Self, F>
    where
        F: FnOnce(Self::Error) -> E,
        Self: Sized,
    {
        MapErr::new(self, f)
    }

    /// Maps this future's [`Error`](TryFuture::Error) to a new error type
    /// using the [`Into`](std::convert::Into) trait.
    ///
    /// This method does for futures what the `?`-operator does for
    /// [`Result`]: It lets the compiler infer the type of the resulting
    /// error. Just as [`map_err`](TryFutureExt::map_err), this is useful for
    /// example to ensure that futures have the same [`Error`](TryFuture::Error)
    /// type when using [`select!`] or [`join!`].
    ///
    /// Note that this method consumes the future it is called on and returns a
    /// wrapped version of it.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::TryFutureExt;
    ///
    /// # futures::executor::block_on(async {
    /// let future_err_u8 = async { Err::<(), u8>(1) };
    /// let future_err_i32 = future_err_u8.err_into::<i32>();
    /// # });
    /// ```
    fn err_into<E>(self) -> ErrInto<Self, E>
    where
        Self: Sized,
        Self::Error: Into<E>,
    {
        ErrInto::new(self)
    }

    /// Executes another future after this one resolves successfully. The
    /// success value is passed to a closure to create this subsequent future.
    ///
    /// The provided closure `f` will only be called if this future is resolved
    /// to an [`Ok`]. If this future resolves to an [`Err`], panics, or is
    /// dropped, then the provided closure will never be invoked. The
    /// [`Error`](TryFuture::Error) type of this future and the future
    /// returned by `f` have to match.
    ///
    /// Note that this method consumes the future it is called on and returns a
    /// wrapped version of it.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::TryFutureExt;
    ///
    /// # futures::executor::block_on(async {
    /// let future = async { Ok::<i32, i32>(1) };
    /// let future = future.and_then(|x| async move { Ok::<i32, i32>(x + 3) });
    /// assert_eq!(future.await, Ok(4));
    /// # });
    /// ```
    ///
    /// Calling [`and_then`](TryFutureExt::and_then) on an errored future has no
    /// effect:
    ///
    /// ```
    /// use futures::future::TryFutureExt;
    ///
    /// # futures::executor::block_on(async {
    /// let future = async { Err::<i32, i32>(1) };
    /// let future = future.and_then(|x| async move { Err::<i32, i32>(x + 3) });
    /// assert_eq!(future.await, Err(1));
    /// # });
    /// ```
    fn and_then<Fut, F>(self, f: F) -> AndThen<Self, Fut, F>
    where
        F: FnOnce(Self::Ok) -> Fut,
        Fut: TryFuture<Error = Self::Error>,
        Self: Sized,
    {
        AndThen::new(self, f)
    }

    /// Executes another future if this one resolves to an error. The
    /// error value is passed to a closure to create this subsequent future.
    ///
    /// The provided closure `f` will only be called if this future is resolved
    /// to an [`Err`]. If this future resolves to an [`Ok`], panics, or is
    /// dropped, then the provided closure will never be invoked. The
    /// [`Ok`](TryFuture::Ok) type of this future and the future returned by `f`
    /// have to match.
    ///
    /// Note that this method consumes the future it is called on and returns a
    /// wrapped version of it.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::TryFutureExt;
    ///
    /// # futures::executor::block_on(async {
    /// let future = async { Err::<i32, i32>(1) };
    /// let future = future.or_else(|x| async move { Err::<i32, i32>(x + 3) });
    /// assert_eq!(future.await, Err(4));
    /// # });
    /// ```
    ///
    /// Calling [`or_else`](TryFutureExt::or_else) on a successful future has
    /// no effect:
    ///
    /// ```
    /// use futures::future::TryFutureExt;
    ///
    /// # futures::executor::block_on(async {
    /// let future = async { Ok::<i32, i32>(1) };
    /// let future = future.or_else(|x| async move { Ok::<i32, i32>(x + 3) });
    /// assert_eq!(future.await, Ok(1));
    /// # });
    /// ```
    fn or_else<Fut, F>(self, f: F) -> OrElse<Self, Fut, F>
    where
        F: FnOnce(Self::Error) -> Fut,
        Fut: TryFuture<Ok = Self::Ok>,
        Self: Sized,
    {
        OrElse::new(self, f)
    }

    /// Do something with the success value of a future before passing it on.
    ///
    /// When using futures, you'll often chain several of them together.  While
    /// working on such code, you might want to check out what's happening at
    /// various parts in the pipeline, without consuming the intermediate
    /// value. To do that, insert a call to `inspect_ok`.
    ///
    /// # Examples
    ///
    /// ```
    /// # futures::executor::block_on(async {
    /// use futures::future::TryFutureExt;
    ///
    /// let future = async { Ok::<_, ()>(1) };
    /// let new_future = future.inspect_ok(|&x| println!("about to resolve: {}", x));
    /// assert_eq!(new_future.await, Ok(1));
    /// # });
    /// ```
    fn inspect_ok<F>(self, f: F) -> InspectOk<Self, F>
    where
        F: FnOnce(&Self::Ok),
        Self: Sized,
    {
        InspectOk::new(self, f)
    }

    /// Do something with the error value of a future before passing it on.
    ///
    /// When using futures, you'll often chain several of them together.  While
    /// working on such code, you might want to check out what's happening at
    /// various parts in the pipeline, without consuming the intermediate
    /// value. To do that, insert a call to `inspect_err`.
    ///
    /// # Examples
    ///
    /// ```
    /// # futures::executor::block_on(async {
    /// use futures::future::TryFutureExt;
    ///
    /// let future = async { Err::<(), _>(1) };
    /// let new_future = future.inspect_err(|&x| println!("about to error: {}", x));
    /// assert_eq!(new_future.await, Err(1));
    /// # });
    /// ```
    fn inspect_err<F>(self, f: F) -> InspectErr<Self, F>
    where
        F: FnOnce(&Self::Error),
        Self: Sized,
    {
        InspectErr::new(self, f)
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
    /// # futures::executor::block_on(async {
    /// use futures::future::TryFutureExt;
    /// use futures::stream::{self, TryStreamExt};
    ///
    /// let stream_items = vec![17, 18, 19].into_iter().map(Ok);
    /// let future_of_a_stream = async { Ok::<_, ()>(stream::iter(stream_items)) };
    ///
    /// let stream = future_of_a_stream.try_flatten_stream();
    /// let list = stream.try_collect::<Vec<_>>().await;
    /// assert_eq!(list, Ok(vec![17, 18, 19]));
    /// # });
    /// ```
    fn try_flatten_stream(self) -> TryFlattenStream<Self>
    where
        Self::Ok: TryStream<Error = Self::Error>,
        Self: Sized,
    {
        TryFlattenStream::new(self)
    }

    /// Unwraps this future's ouput, producing a future with this future's
    /// [`Ok`](TryFuture::Ok) type as its
    /// [`Output`](std::future::Future::Output) type.
    ///
    /// If this future is resolved successfully, the returned future will
    /// contain the original future's success value as output. Otherwise, the
    /// closure `f` is called with the error value to produce an alternate
    /// success value.
    ///
    /// This method is similar to the [`Result::unwrap_or_else`] method.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::TryFutureExt;
    ///
    /// # futures::executor::block_on(async {
    /// let future = async { Err::<(), &str>("Boom!") };
    /// let future = future.unwrap_or_else(|_| ());
    /// assert_eq!(future.await, ());
    /// # });
    /// ```
    fn unwrap_or_else<F>(self, f: F) -> UnwrapOrElse<Self, F>
    where
        Self: Sized,
        F: FnOnce(Self::Error) -> Self::Ok,
    {
        UnwrapOrElse::new(self, f)
    }

    /// Wraps a [`TryFuture`] into a future compatable with libraries using
    /// futures 0.1 future definitons. Requires the `compat` feature to enable.
    #[cfg(feature = "compat")]
    fn compat(self) -> Compat<Self>
    where
        Self: Sized + Unpin,
    {
        Compat::new(self)
    }

    /// Wraps a [`TryFuture`] into a type that implements
    /// [`Future`](std::future::Future).
    ///
    /// [`TryFuture`]s currently do not implement the
    /// [`Future`](std::future::Future) trait due to limitations of the
    /// compiler.
    ///
    /// # Examples
    ///
    /// ```
    /// use futures::future::{Future, TryFuture, TryFutureExt};
    ///
    /// # type T = i32;
    /// # type E = ();
    /// fn make_try_future() -> impl TryFuture<Ok = T, Error = E> { // ... }
    /// # async { Ok::<i32, ()>(1) }
    /// # }
    /// fn take_future(future: impl Future<Output = Result<T, E>>) { /* ... */ }
    ///
    /// take_future(make_try_future().into_future());
    /// ```
    fn into_future(self) -> IntoFuture<Self>
    where
        Self: Sized,
    {
        IntoFuture::new(self)
    }

    /// A convenience method for calling [`TryFuture::try_poll`] on [`Unpin`]
    /// future types.
    fn try_poll_unpin(&mut self, cx: &mut Context<'_>) -> Poll<Result<Self::Ok, Self::Error>>
    where
        Self: Unpin,
    {
        Pin::new(self).try_poll(cx)
    }
}
