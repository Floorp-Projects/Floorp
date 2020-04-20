//! Futures
//!
//! This module contains a number of functions for working with `Future`s,
//! including the [`FutureExt`] trait and the [`TryFutureExt`] trait which add
//! methods to `Future` types.

#[cfg(feature = "alloc")]
pub use futures_core::future::{BoxFuture, LocalBoxFuture};
pub use futures_core::future::{FusedFuture, Future, TryFuture};
pub use futures_task::{FutureObj, LocalFutureObj, UnsafeFutureObj};

// Extension traits and combinators

#[allow(clippy::module_inception)]
mod future;
pub use self::future::{
    Flatten, FlattenStream, Fuse, FutureExt, Inspect, IntoStream, Map, NeverError, Then, UnitError,
};

#[cfg(feature = "std")]
pub use self::future::CatchUnwind;

#[cfg(feature = "channel")]
#[cfg(feature = "std")]
pub use self::future::{Remote, RemoteHandle};

#[cfg(feature = "std")]
pub use self::future::Shared;

mod try_future;
pub use self::try_future::{
    AndThen, ErrInto, InspectErr, InspectOk, IntoFuture, MapErr, MapOk, OrElse, TryFlattenStream,
    TryFutureExt, UnwrapOrElse,
};

#[cfg(feature = "sink")]
pub use self::try_future::FlattenSink;

// Primitive futures

mod lazy;
pub use self::lazy::{lazy, Lazy};

mod pending;
pub use self::pending::{pending, Pending};

mod maybe_done;
pub use self::maybe_done::{maybe_done, MaybeDone};

mod option;
pub use self::option::OptionFuture;

mod poll_fn;
pub use self::poll_fn::{poll_fn, PollFn};

mod ready;
pub use self::ready::{err, ok, ready, Ready};

mod join;
pub use self::join::{join, join3, join4, join5, Join, Join3, Join4, Join5};

#[cfg(feature = "alloc")]
mod join_all;
#[cfg(feature = "alloc")]
pub use self::join_all::{join_all, JoinAll};

mod select;
pub use self::select::{select, Select};

#[cfg(feature = "alloc")]
mod select_all;
#[cfg(feature = "alloc")]
pub use self::select_all::{select_all, SelectAll};

mod try_join;
pub use self::try_join::{
    try_join, try_join3, try_join4, try_join5, TryJoin, TryJoin3, TryJoin4, TryJoin5,
};

#[cfg(feature = "alloc")]
mod try_join_all;
#[cfg(feature = "alloc")]
pub use self::try_join_all::{try_join_all, TryJoinAll};

mod try_select;
pub use self::try_select::{try_select, TrySelect};

#[cfg(feature = "alloc")]
mod select_ok;
#[cfg(feature = "alloc")]
pub use self::select_ok::{select_ok, SelectOk};

mod either;
pub use self::either::Either;

cfg_target_has_atomic! {
    #[cfg(feature = "alloc")]
    mod abortable;
    #[cfg(feature = "alloc")]
    pub use self::abortable::{abortable, Abortable, AbortHandle, AbortRegistration, Aborted};
}

// Just a helper function to ensure the futures we're returning all have the
// right implementations.
fn assert_future<T, F>(future: F) -> F
where
    F: Future<Output = T>,
{
    future
}
